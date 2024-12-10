#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define MAX_KLIENCI 20

static void semafor_v(int semafor_id, int numer_semafora);
static void semafor_p(int semafor_id, int numer_semafora);


int main()
{
    srand(time(NULL));
    
    // Tworzenie semafora kolejki
    key_t key = ftok(".", "q");
    if (key == -1)
	{
		perror("ftok - nie udalo sie utworzyc klucza");
		exit(EXIT_FAILURE);
	}

    int semafor_kolejka = semget(key, 1, 0600|IPC_CREAT);
    if (semafor_kolejka == -1)
	{
		perror("semget - nie udalo sie utworzyc semafora");
		exit(EXIT_FAILURE);
	}

    if (semctl(semafor_kolejka, 0, SETVAL, 5) == -1)
    {
        perror("semctl - nie mozna ustawic semafora");
        exit(EXIT_FAILURE);
    }
    
    // Tworzenie klientow
    for (int i = 0; i < MAX_KLIENCI; i++)
    {
        sleep((rand() % 3));
        
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork error - nowy klient");
            exit(EXIT_FAILURE);
        } else if (pid == 0)
        {
            // Kod dzialania klienta
            semafor_p(semafor_kolejka, 0);

            printf("PID = %d, w kolejce na basen\n", getpid());
            sleep(10);

            printf("PID = %d, wchodze na basen\n", getpid());
            semafor_v(semafor_kolejka, 0);

            exit(0);
        }
    }


    while (wait(NULL) != -1) {}

	if (semctl(semafor_kolejka, 0, IPC_RMID) == -1)
	{
		perror("semctl - nie mozna uzunac semaforow");
		exit(EXIT_FAILURE);
	}
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