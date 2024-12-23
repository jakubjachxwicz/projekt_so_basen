#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <pthread.h>
#include <string.h>

#include "header.h"

// Ile mikrosekund irl trwa jedna sekunda w symulacji
// 5000 - 3 min 36 s + 7 s
// 2500 - 1 min 48 s + 7 s
// 1250 - 54 s + 7 s
// 1667 - 1 min 12 s + 7 s
#define SEKUNDA 5000
// Adres zmiennej przechowujacej czas
char* shm_czas_adres;

bool stop_time;

void *czasomierz();
void czyszczenie();
void signal_handler(int sig);

pid_t pid_klienci, pid_kasjer;
pthread_t t_czasomierz;
int shm_id, shm_czas_id, semafor;


int main()
{
    signal(SIGINT, signal_handler);

    stop_time = false;
    
    
    // Inicjowanie semaforkow
    key_t key = ftok(".", 51);
    if (key == -1)
	{
		perror("ftok - nie udalo sie utworzyc klucza");
		exit(EXIT_FAILURE);
	}

    key_t key_czas = ftok(".", 52);
    if (key_czas == -1)
	{
		perror("ftok - nie udalo sie utworzyc klucza");
		exit(EXIT_FAILURE);
	}

    semafor = semget(key, 4, 0660|IPC_CREAT);
    if (semafor == -1)
	{
		perror("semget - nie udalo sie utworzyc semafora");
		exit(EXIT_FAILURE);
	}

    if (semctl(semafor, 0, SETVAL, 5) == -1)
    {
        perror("semctl - nie mozna ustawic semafora");
        exit(EXIT_FAILURE);
    }
    if (semctl(semafor, 1, SETVAL, 1) == -1)
    {
        perror("semctl - nie mozna ustawic semafora");
        exit(EXIT_FAILURE);
    }
    if (semctl(semafor, 2, SETVAL, 0) == -1)
    {
        perror("semctl - nie mozna ustawic semafora");
        exit(EXIT_FAILURE);
    }
    if (semctl(semafor, 3, SETVAL, 0) == -1)
    {
        perror("semctl - nie mozna ustawic semafora");
        exit(EXIT_FAILURE);
    }

    // Inicjowanie pam. wspoldzielonej do wymiany klient/kasjer
    shm_id = shmget(key, sizeof(struct dane_klienta), 0600|IPC_CREAT);
    if (shm_id == -1)
    {
        perror("shmget - tworzenie pamieci wspoldzielonej");
        exit(EXIT_FAILURE);
    }

    // Inicjowanie pam. wspoldzielonej do obslugi czasu
    shm_czas_id = shmget(key_czas, sizeof(int), 0600|IPC_CREAT);
    if (shm_czas_id == -1)
    {
        perror("shmget - tworzenie pamieci wspoldzielonej do obługi czasu");
        exit(EXIT_FAILURE);
    }

    shm_czas_adres = (char*)shmat(shm_czas_id, NULL, 0);
    if (shm_czas_adres == (char*)(-1))
    {
        perror("shmat - problem z dolaczeniem pamieci do obslugi czasu");
        exit(EXIT_FAILURE);
    }

    *shm_czas_adres = 0;
    pthread_create(&t_czasomierz, NULL, &czasomierz, NULL);


    pid_klienci = fork();
    if (pid_klienci < 0)
    {
        perror("fork error - proces klientow");
        exit(EXIT_FAILURE);
    } else if (pid_klienci == 0)
    {
        execl("./klient", "klient", NULL);
        exit(0);
    }
    else
    {
        pid_kasjer = fork();
        if (pid_kasjer < 0)
        {
            perror("fork error - proces kasjera");
            exit(EXIT_FAILURE);
        } else if (pid_kasjer == 0)
        {
            execl("./kasjer", "kasjer", NULL);
            exit(0);
        }
    }

    printf("Kasjer PID: %d, klienci PID: %d\n\n", pid_kasjer, pid_klienci);

    czyszczenie();

    return 0;
}


void *czasomierz()
{
    int *jaki_czas = (int *)shm_czas_adres;
    while (*jaki_czas < 44100 && !stop_time)
    {
        usleep(SEKUNDA);
        (*jaki_czas)++;
    }

    kill(pid_kasjer, SIGINT);
    return 0;
}


void czyszczenie()
{
    int status;
    pid_t finished;
    finished = waitpid(pid_klienci, &status, 0);
    if (finished == -1) perror("wait");  
    else if (WIFEXITED(status)) 
        printf("Proces potomny (PID: %d) zakonczyl sie z kodem: %d\n", finished, WEXITSTATUS(status));
    else
        printf("Proces potomny (PID: %d) zakonczyl sie w nieoczekiwany sposob, status: %d\n", finished, status);

    finished = waitpid(pid_kasjer, &status, 0);
    if (finished == -1) perror("wait");  
    else if (WIFEXITED(status)) 
        printf("Proces potomny (PID: %d) zakonczyl sie z kodem: %d\n", finished, WEXITSTATUS(status));
    else
        printf("Proces potomny (PID: %d) zakonczyl sie w nieoczekiwany sposob, status: %d\n", finished, status);

    pthread_join(t_czasomierz, NULL);

    if (shmctl(shm_id, IPC_RMID, 0) == -1)
    {
        perror("shmctl - problem z usunieciem pamieci wspoldzielonej");
        exit(EXIT_FAILURE);
    }
    if (shmctl(shm_czas_id, IPC_RMID, 0) == -1)
    {
        perror("shmctl - problem z usunieciem pamieci wspoldzielonej");
        exit(EXIT_FAILURE);
    }

    if (semctl(semafor, 0, IPC_RMID) == -1)
	{
		perror("semctl - nie mozna uzunac semaforow");
		exit(EXIT_FAILURE);
	}
}

void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        stop_time = true;
        
        czyszczenie();
        exit(0);
    }
}