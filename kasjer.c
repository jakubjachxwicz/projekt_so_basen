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
#include <unistd.h>
#include "header.h"
#include <string.h>


static void semafor_v(int semafor_id, int numer_semafora);
static void semafor_p(int semafor_id, int numer_semafora);
void odlacz_pamiec();
void signal_handler(int sig);

char* shm_adres;


int main()
{
    signal(SIGINT, signal_handler);
	
	struct dane_klienta klient;

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

    int semafor = semget(key, 4, 0660);
    if (semafor == -1)
	{
		perror("KASJER: semget - blad dostepu do semaforow");
		exit(EXIT_FAILURE);
	}

	int shm_id = shmget(key, sizeof(struct dane_klienta), 0600);
    if (shm_id == -1)
    {
        perror("shmget - tworzenie pamieci wspoldzielonej");
        exit(EXIT_FAILURE);
    }

    shm_adres = (char*)shmat(shm_id, NULL, 0);
    if (shm_adres == (char*)(-1))
    {
        perror("shmat - problem z dolaczeniem pamieci");
        exit(EXIT_FAILURE);
    }

	// Uzyskanie pamieci wspoldzielonej do obslugi czasu
    int shm_czas_id = shmget(key_czas, sizeof(int), 0600);
    if (shm_czas_id == -1)
    {
        perror("shmget - tworzenie pamieci wspoldzielonej do obługi czasu");
        exit(EXIT_FAILURE);
    }

    char* shm_czas_adres = (char*)shmat(shm_czas_id, NULL, 0);
    if (shm_czas_adres == (char*)(-1))
    {
        perror("shmat - problem z dolaczeniem pamieci do obslugi czasu");
        exit(EXIT_FAILURE);
    }

	int aktualny_czas;
    memcpy(&aktualny_czas, shm_czas_adres, sizeof(int));
    while (aktualny_czas < 43200)
    {
		semafor_p(semafor, 2);
        // Procesowanie klienta
		memcpy(&klient, shm_adres, sizeof(struct dane_klienta));
		sleep(3);

		if ((klient.wiek > 18 || klient.wiek < 10) && klient.pieniadze >= 60)
		{
			klient.pieniadze -= 60;
			klient.wpuszczony = true;
		} else if (klient.pieniadze >= 30)
		{
			klient.pieniadze -= 30;
			klient.wpuszczony = true;
		} else
			klient.wpuszczony = false;

        memcpy(shm_adres, &klient, sizeof(struct dane_klienta));
        printf("[KASJER] Klient o PID = %d obsluzony\n", klient.PID);
        semafor_v(semafor, 3);


		memcpy(&aktualny_czas, shm_czas_adres, sizeof(int));
    }

	odlacz_pamiec();
	exit(0);
}


static void semafor_v(int semafor_id, int numer_semafora)
{
	struct sembuf bufor_sem;
	bufor_sem.sem_num = numer_semafora;
	bufor_sem.sem_op = 1;
	bufor_sem.sem_flg = SEM_UNDO;

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

void odlacz_pamiec()
{
	if (shmdt(shm_adres) == -1)
	{
		perror("KASJER: shmdt - problem z odlaczeniem pamieci od procesu");
        exit(EXIT_FAILURE);
	}
}

void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        printf("AAAAAAAAAAAAAAAA\n");
		
		odlacz_pamiec();
        exit(0);
    }
}