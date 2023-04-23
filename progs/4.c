#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>

#define TEXT_SIZE 100

int main()
{
    char text[TEXT_SIZE];
    int n;

    // Ввод текста и количества процессов-шифровальщиков
    printf("Введите текст: ");
    scanf("%s", text);
    printf("Введите количество процессов-шифровальщиков: ");
    scanf("%d", &n);
    if (n > 9 || n < 1) {
        printf("Некорректное кол-во процессов-шифровальщиков");
        exit(-1);
    }

    // Получение дескриптора разделяемой памяти
    int mem_fd = shm_open("my_shared_memory", O_CREAT | O_RDWR, 0666);
    if (mem_fd == -1)
    {
        perror("shm_open failed");
        exit(1);
    }

    // Выделение памяти для массива зашифрованных символов
    size_t mem_size = sizeof(int) * strlen(text);
    if (ftruncate(mem_fd, mem_size) == -1)
    {
        perror("ftruncate failed");
        exit(1);
    }

    // Отображение памяти в адресное пространство процесса
    int *encoded_text = (int *)mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0);
    if (encoded_text == MAP_FAILED)
    {
        perror("mmap failed");
        exit(1);
    }

    // Создание семафоров
    sem_t *semaphores[n];
    for (int i = 0; i < n; i++)
    {
        char semaphore_name[10];
        sprintf(semaphore_name, "/sem_%d", i);
        semaphores[i] = sem_open(semaphore_name, O_CREAT, 0666, 0);
        if (semaphores[i] == SEM_FAILED) {
            perror("sem_open failed");
            exit(1);
        } else {
            printf("Семафор %s успешно создан\n", semaphore_name);
        }
    }

    // Создание процессов-шифровальщиков
    for (int i = 0; i < n; i++)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork failed");
            exit(1);
        }
        else if (pid == 0)
        { // Процесс-шифровальщик
            char semaphore_name[10];
            sprintf(semaphore_name, "/sem_%d", i);
            sem_t *semaphore = sem_open(semaphore_name, 0);
            if (semaphore == SEM_FAILED) {
                perror("sem_open failed");
                exit(1);
            } else {
                printf("Семафор %s успешно открылся\n", semaphore_name);
            }
            int text_len = strlen(text);
            for (int j = i; j < text_len; j += n)
            {
                encoded_text[j] = (int)text[j];
                sem_post(semaphore);
                // int sem_value;
                // sem_getvalue(semaphore, &sem_value);
                // printf("Value of semaphore %s: write %d\n", semaphore_name, text[j]);
                if (j + n >= text_len)
                {
                    break;
                }
                sem_wait(semaphore);
            }

            exit(0);
        }
    }

    // Ожидание завершения всех процессов-шифровальщиков
    for (int i = 0; i < n; i++) {
        wait(NULL);
    }

    // Процесс менеджер
    int text_len = strlen(text);
    for (int i = 0; i < text_len; i++)
    {
        sem_wait(semaphores[i % n]);
        printf("%d ", encoded_text[i]);
        sem_post(semaphores[i % n]);
    }


    // Освобождаем разделяемую память и семафоры
    munmap(encoded_text, sizeof(int) * text_len);
    munmap(semaphores, sizeof(sem_t *) * n);
    for (int i = 0; i < n; i++)
    {
        char sem_name[20];
        sprintf(sem_name, "%s%d", "/sem_", i);
        sem_unlink(sem_name);
    }

    return 0;
}