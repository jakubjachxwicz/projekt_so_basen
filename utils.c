#pragma once

#include <unistd.h>
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
#include <sys/stat.h>
#include <fcntl.h>

#include "header.h"


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
		if (tab[i] <= 0)
		{
			tab[i] = pid;
			return;
		}
	}
}

void dodaj_do_tablicy_X2(int (*tab)[X2 + 1], int roz, int pid, int wiek)
{
	for (int i = 1; i <= roz; i++)
	{
		if (tab[0][i] <= 0)
		{
			tab[0][i] = pid;
			tab[1][i] = wiek;
			return;
		}
	}
}

void usun_z_tablicy(int* tab, int roz, int pid)
{
	for (int i = 1; i <= roz; i++)
	{
		if (tab[i] == pid)
		{
			tab[i] = 0;
			return;
		}
	}
}

void usun_z_tablicy_X2(int (*tab)[X2 + 1], int roz, int pid)
{
	for (int i = 1; i <= roz; i++)
	{
		if (tab[0][i] == pid)
		{
			tab[0][i] = 0;
			tab[1][i] = 0;
			return;
		}
	}
}

int ile_osob(int* tab, int roz, int pid)
{
	int ile = 0;
	for (int i = 1; i <= roz; i++)
	{
		if (tab[i] == pid)
			ile++;
	}

	return ile;
}

double srednia_wieku(int* tab, int roz, int nowy)
{
	int n = 0, suma = 0;
	for (int i = 1; i <= roz; i++)
	{
		if (tab[i] != 0)
		{
			suma += tab[i];
			n++;
		}
	}

	double sr = (double)(suma + nowy) / (double)(n + 1);
	return sr;
}
