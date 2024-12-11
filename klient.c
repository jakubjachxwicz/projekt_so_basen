#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include "header.h"
#include <string.h>

#define MAX_KLIENCI 15

static void semafor_v(int semafor_id, int numer_semafora);
static void semafor_p(int semafor_id, int numer_semafora);


int main()
{
    srand(time(NULL));

    key_t key = ftok(".", 51);
    if (key == -1)
	{
		perror("ftok - nie udalo sie utworzyc klucza");
		exit(EXIT_FAILURE);
	}

    int semafor = semget(key, 4, 0660);
    if (semafor == -1)
	{
		perror("semget - blad dostepu do semaforow");
		exit(EXIT_FAILURE);
	}

    int shm_id = shmget(key, sizeof(struct dane_klienta), 0600);
    if (shm_id == -1)
    {
        perror("shmget - tworzenie pamieci wspoldzielonej");
        exit(EXIT_FAILURE);
    }

    char* shm_adres = (char*)shmat(shm_id, NULL, 0);
    if (shm_adres == (char*)(-1))
    {
        perror("shmat - problem z dolaczeniem pamieci");
        exit(EXIT_FAILURE);
    }
    
    // Tworzenie klientow
    for (int i = 0; i < MAX_KLIENCI; i++)
    {
        sleep((rand() % 6) + 2);
        
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork error - nowy klient");
            exit(EXIT_FAILURE);
        } else if (pid == 0)
        {
            // Kod dzialania klienta
            struct dane_klienta klient;
            klient.PID = getpid();
            klient.wiek = (rand() % 70) + 1;
            klient.wiek_opiekuna = (klient.wiek < 18) ? ((rand() % 53) + 18) : 0;
            klient.pampers = (klient.wiek <= 3) ? true : false;
            klient.czepek = rand() % 2;

            // printf("**************\n");
            // printf("PID = %d, wiek = %d, opiekun = %d, pampers = %d, czepek = %d\n",
            //     klient.PID, klient.wiek, klient.wiek_opiekuna, klient.pampers, klient.czepek);
            // printf("**************\n");
            // semafor_p(semafor, 0);

            printf("PID = %d, w kolejce na basen\n", getpid());
            //sleep(10);

            semafor_p(semafor, 1);
            memcpy(shm_adres, &klient, sizeof(struct dane_klienta));
            usleep(1000);
            semafor_v(semafor, 2);
            // printf("U kasjera\n");
            //sleep(1);
            semafor_p(semafor, 3);
            printf("PID = %d, wchodze na basenik\n", getpid());

            // printf("PID = %d, wchodze na basen\n", getpid());
            // semafor_v(semafor, 0);
            semafor_v(semafor, 1);

            if (shmdt(shm_adres) == -1)
            {
                perror("shmdt - problem z odlaczeniem pamieci od procesu");
                exit(EXIT_FAILURE);
            }

            return 0;
        }
    }


    while (wait(NULL) != -1) {}
}


static void semafor_v(int semafor_id, int numer_semafora)
{
	struct sembuf bufor_sem;
	bufor_sem.sem_num = numer_semafora;
	bufor_sem.sem_op = 1;
	bufor_sem.sem_flg = 0;

	// printf("V: PID=%d, sem[%d] przed: %d\n", getpid(), numer_semafora, semctl(semafor_id, numer_semafora, GETVAL));
    // semop(semafor_id, &bufor_sem, 1);
    // printf("V: PID=%d, sem[%d] po: %d\n", getpid(), numer_semafora, semctl(semafor_id, numer_semafora, GETVAL));

    while (semop(semafor_id, &bufor_sem, 1) == -1)
	{
		if (errno == EINTR)
			continue;
		else
		{
			perror("Problem z otwarciem semafora");
			exit(EXIT_FAILURE);
		}
	}
}

static void semafor_p(int semafor_id, int numer_semafora)
{
	struct sembuf bufor_sem;
	bufor_sem.sem_num = numer_semafora;
	bufor_sem.sem_op = -1;
	bufor_sem.sem_flg = 0;

    // printf("P: PID=%d, sem[%d] przed: %d\n", getpid(), numer_semafora, semctl(semafor_id, numer_semafora, GETVAL));
    // semop(semafor_id, &bufor_sem, 1);
    // printf("P: PID=%d, sem[%d] po: %d\n", getpid(), numer_semafora, semctl(semafor_id, numer_semafora, GETVAL));

	while (semop(semafor_id, &bufor_sem, 1) == -1)
	{
		if (errno == EINTR)
			continue;
		else
		{
			perror("Problem z zamknięciem semafora");
			exit(EXIT_FAILURE);
		}
	}
}