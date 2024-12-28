#pragma once

#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/msg.h>


void godz_sym(int sekundy, char* res)
{
    int sek_r = sekundy % 60;
    int minuty = sekundy / 60;
    int min_r = minuty % 60;
    int godziny = (minuty / 60) + 9;

    snprintf(res, 9, "%02d:%02d:%02d\n", godziny, min_r, sek_r);
}

static void semafor_v(int semafor_id, int numer_semafora)
{
	struct sembuf bufor_sem;
	bufor_sem.sem_num = numer_semafora;
	bufor_sem.sem_op = 1;
	bufor_sem.sem_flg = 0;

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

void dodaj_do_tablicy(int* tab, int roz, int pid)
{
	for (int i = 1; i <= roz; i++)
	{
		if (tab[i] == -1)
		{
			tab[i] = pid;
			return;
		}
	}
}