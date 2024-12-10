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
#include <stdbool.h>


static void semafor_v(int semafor_id, int numer_semafora);
static void semafor_p(int semafor_id, int numer_semafora);

void handle_sigusr1(int sig);
bool working_flag;


int main()
{
	struct sigaction sa;
	sa.sa_handler = handle_sigusr1;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
	
	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Error setting signal handler");
        return 1;
    }

	working_flag = true;
	
	key_t key = ftok(".", 51);
    if (key == -1)
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


    while (working_flag)
    {
        // printf("Kasjer czeka na klienta\n");
		semafor_p(semafor, 2);
        // Procesowanie klienta
		//sleep(1);
        printf("Klient wpuszczony na basen\n");
        semafor_v(semafor, 3);
    }

	exit(0);
}


static void semafor_v(int semafor_id, int numer_semafora)
{
	struct sembuf bufor_sem;
	bufor_sem.sem_num = numer_semafora;
	bufor_sem.sem_op = 1;
	bufor_sem.sem_flg = SEM_UNDO;

	//printf("V: PID=%d, sem[%d] przed: %d\n", getpid(), numer_semafora, semctl(semafor_id, numer_semafora, GETVAL));
    semop(semafor_id, &bufor_sem, 1);
    //printf("V: PID=%d, sem[%d] po: %d\n", getpid(), numer_semafora, semctl(semafor_id, numer_semafora, GETVAL));

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

    //printf("P: PID=%d, sem[%d] przed: %d\n", getpid(), numer_semafora, semctl(semafor_id, numer_semafora, GETVAL));
    semop(semafor_id, &bufor_sem, 1);
    //printf("P: PID=%d, sem[%d] po: %d\n", getpid(), numer_semafora, semctl(semafor_id, numer_semafora, GETVAL));

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

void handle_sigusr1(int sig)
{
    working_flag = false;
}
