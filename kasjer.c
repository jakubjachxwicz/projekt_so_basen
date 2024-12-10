#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/wait.h>


static void semafor_v(int semafor_id, int numer_semafora);
static void semafor_p(int semafor_id, int numer_semafora);


int main()
{
    key_t key = ftok(".", "q");
    if (key == -1)
	{
		perror("ftok - nie udalo sie utworzyc klucza");
		exit(EXIT_FAILURE);
	}

    int semafor = semget(key, 3, 0600|IPC_CREAT);
    if (semafor == -1)
	{
		perror("semget - nie udalo sie utworzyc semafora");
		exit(EXIT_FAILURE);
	}


    while (1)
    {
        // printf("Kasjer czeka na klienta\n");
		semafor_p(semafor, 2);
        // Procesowanie klienta
		sleep(1);
        printf("Klient wpuszczony na basen\n");
        semafor_v(semafor, 3);
    }
}


static void semafor_v(int semafor_id, int numer_semafora)
{
	struct sembuf bufor_sem;
	bufor_sem.sem_num = numer_semafora;
	bufor_sem.sem_op = 1;
	bufor_sem.sem_flg = SEM_UNDO;

	printf("V: PID=%d, sem[%d] przed: %d\n", getpid(), numer_semafora, semctl(semafor_id, numer_semafora, GETVAL));
    semop(semafor_id, &bufor_sem, 1);
    printf("V: PID=%d, sem[%d] po: %d\n", getpid(), numer_semafora, semctl(semafor_id, numer_semafora, GETVAL));

    // while (semop(semafor_id, &bufor_sem, 1) == -1)
	// {
	// 	if (errno == EINTR)
	// 		continue;
	// 	else
	// 	{
	// 		perror("Problem z otwarciem semafora");
	// 		exit(EXIT_FAILURE);
	// 	}
	// }
}

static void semafor_p(int semafor_id, int numer_semafora)
{
	struct sembuf bufor_sem;
	bufor_sem.sem_num = numer_semafora;
	bufor_sem.sem_op = -1;
	bufor_sem.sem_flg = 0;

    printf("P: PID=%d, sem[%d] przed: %d\n", getpid(), numer_semafora, semctl(semafor_id, numer_semafora, GETVAL));
    semop(semafor_id, &bufor_sem, 1);
    printf("P: PID=%d, sem[%d] po: %d\n", getpid(), numer_semafora, semctl(semafor_id, numer_semafora, GETVAL));

	// while (semop(semafor_id, &bufor_sem, 1) == -1)
	// {
	// 	if (errno == EINTR)
	// 		continue;
	// 	else
	// 	{
	// 		perror("Problem z zamknięciem semafora");
	// 		exit(EXIT_FAILURE);
	// 	}
	// }
}