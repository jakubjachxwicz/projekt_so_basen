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
            // semafor_p(semafor, 0);

            printf("PID = %d, w kolejce na basen\n", getpid());
            //sleep(10);

            semafor_p(semafor, 1);
            usleep(1000);
            semafor_v(semafor, 2);
            // printf("U kasjera\n");
            //sleep(1);
            semafor_p(semafor, 3);
            printf("PID = %d, wchodze na basenik\n", getpid());

            // printf("PID = %d, wchodze na basen\n", getpid());
            // semafor_v(semafor, 0);
            semafor_v(semafor, 1);


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