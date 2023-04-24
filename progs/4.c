#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define TEXT_SIZE 100

int n;
int *encoded_text;
int text_len;


void handler(int nsig) {
    // Удаление семафоров
    for (int i = 0; i < n; i++) {
        char semaphore_name[10];
        sprintf(semaphore_name, "/sem_%hi", i);
        if (sem_unlink(semaphore_name)) {
            perror("При удалении процесса при помощи ctrl+c возникла ошибка");
        }
    }
    if (munmap(encoded_text, sizeof(int) * text_len) == -1) {
        perror("При удалении памяти при помощи ctrl+c возникла ошибка, вероятно память еще не была выделена в основном процессе");
        exit(EXIT_FAILURE);
    }
    exit(0);
}

int main()
{
    char text[TEXT_SIZE];
    // Ввод текста и количества процессов-шифровальщиков
    printf("Введите текст: ");
    fgets(text, 100, stdin);
    text_len = strlen(text) - 1;
    printf("Введите количество процессов-шифровальщиков: ");
    scanf("%d", &n);
    if (n > 9 || n < 1 || n > text_len) {
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
    size_t mem_size = sizeof(int) * text_len;
    if (ftruncate(mem_fd, mem_size) == -1)
    {
        perror("ftruncate failed");
        exit(1);
    }

    // Отображение памяти в адресное пространство процесса
    encoded_text = (int *)mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0);
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
        sprintf(semaphore_name, "/sem_%hi", i);
        semaphores[i] = sem_open(semaphore_name, O_CREAT, 0666, 0);
        int sem_value;
        sem_getvalue(semaphores[i], &sem_value);
        if (semaphores[i] == SEM_FAILED) {
            perror("sem_open failed");
            exit(1);
        } else {
            printf("Семафор %s успешно создан, значение: %d\n", semaphore_name, sem_value);
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
            int j = i;
            sprintf(semaphore_name, "/sem_%hi", i);
            sem_t *semaphore = sem_open(semaphore_name, 0);
            int sem_value;
            sem_getvalue(semaphore, &sem_value);
            if (semaphore == SEM_FAILED) {
                perror("sem_open failed");
                exit(1);
            } else {
                printf("Семафор %s успешно открылся, значение %d\n", semaphore_name, sem_value);
            }
            while (1) {
                sem_getvalue(semaphore, &sem_value);
                printf("Семафор %s ждет команду, значение %d\n", semaphore_name, sem_value);
                sem_wait(semaphores[i]); // ждем, пока менеджер передаст команду на шифрование текущей буквы
                sem_getvalue(semaphore, &sem_value);
                printf("Семафор %s получил команду, значение %d\n", semaphore_name, sem_value);
                encoded_text[j] = (int)text[j]; // шифруем текущую букву
                printf("Шифровальщик %s зашифровал букву %c\n", semaphore_name, text[j]);
                if (j + n >= text_len) {
                    printf("Шифровальщик %s завершил свою работу на букве с индексом %d\n", semaphore_name, j);
                    exit(0);
                }
                j += n;
            }
        }
    }


    // Процесс менеджер
    for (int i = 0; i < text_len; i++)
    {
        int index = i % n; // индекс шифровальщика, отвечающего за шифрование текущей буквы
        sem_post(semaphores[index]);
    }

    // Ожидание завершения всех процессов-шифровальщиков
    for (int i = 0; i < n; i++) {
        wait(NULL);
    }

    for (int i = 0; i < text_len; i++) {
        printf("%d ", encoded_text[i]);
    }
    signal(SIGINT, handler);
    // Удаление семафоров
    for (int i = 0; i < n; i++) {
        char semaphore_name[10];
        sprintf(semaphore_name, "/sem_%hi", i);
        if (sem_unlink(semaphore_name)) {
            perror("При удалении процесса возникла ошибка\n");
        }
    }
    // Освобождаем разделяемую память
    if (munmap(encoded_text, sizeof(int) * text_len) == -1) {
        perror("При удалении памяти возникла ошибка\n");
        exit(EXIT_FAILURE);
    }
    return 0;
}