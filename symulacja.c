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


int main()
{
    key_t key = ftok(".", 51);
    if (key == -1)
	{
		perror("ftok - nie udalo sie utworzyc klucza");
		exit(EXIT_FAILURE);
	}

    int semafor = semget(key, 4, 0660|IPC_CREAT);
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


    pid_t pid_klienci, pid_kasjer;
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

    printf("Kasjer: %d, klienci: %d\n\n", pid_kasjer, pid_klienci);


    int status;
    pid_t finished;
    finished = waitpid(pid_klienci, &status, 0);
    if (finished == -1) perror("wait");  
    else if (WIFEXITED(status)) 
        printf("Proces potomny (PID: %d) zakonczyl sie z kodem: %d\n", finished, WEXITSTATUS(status));
    else
        printf("Proces potomny (PID: %d) zakonczyl sie w nieoczekiwany sposob, status: %d\n", finished, status);

    if (kill(pid_kasjer, SIGKILL) == -1)
    {
        perror("kill - zabicie kasjera");
        exit(EXIT_FAILURE);
    }


    finished = waitpid(pid_kasjer, &status, 0);
    if (finished == -1) perror("wait");  
    else if (WIFEXITED(status)) 
        printf("Proces potomny (PID: %d) zakonczyl sie z kodem: %d\n", finished, WEXITSTATUS(status));
    else
        printf("Proces potomny (PID: %d) zakonczyl sie w nieoczekiwany sposob, status: %d\n", finished, status);


    if (semctl(semafor, 0, IPC_RMID) == -1)
	{
		perror("semctl - nie mozna uzunac semaforow");
		exit(EXIT_FAILURE);
	}

    return 0;
}