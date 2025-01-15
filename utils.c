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
#include <sys/resource.h>
#include <dirent.h>

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

void set_color(const char* color) 
{
    printf("%s", color);
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
		// set_color(RESET);
		// printf("tab[%d] = %d, do skasowania: %d\n", i, tab[i], pid);
		if (tab[i] == pid)
		{
			tab[i] = 0;
			return;
		}
	}
	// set_color(RESET);
	// printf("nie skasowalim, roz = %d, pid do kasowania = %d\n", roz, pid);
	// for(int j = 0; j <= roz; j++)
	// {
	// 	set_color(RESET);
	// 	printf("tab[%d] = %d\n", j, tab[j]);
	// }
	// exit(2);
}

void usun_z_tablicy_X2(int (*tab)[X2 + 1], int roz, int pid)
{
	for (int i = 1; i <= roz; i++)
	{
		// set_color(RESET);
		// printf("tab[0][%d] = %d, do skasowania: %d\n", i, tab[0][i], pid);
		if (tab[0][i] == pid)
		{
			tab[0][i] = 0;
			tab[1][i] = 0;
			return;
		}
	}
	// set_color(RESET);
	// printf("nie skasowalim, roz = %d, pid do kasowania = %d\n", roz, pid);
	// for(int j = 0; j < 2; j++)
	// 	for (int k = 0; k <= roz; k++)
	// 	{
	// 		set_color(RESET);
	// 		printf("tab[%d][%d] = %d\n", j, k, tab[j][k]);
	// 	}
	// exit(3);
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

void lock_mutex(pthread_mutex_t *mutex)
{
	if (pthread_mutex_lock(mutex) != 0)
	{
		perror("pthread_mutex_lock - problem z zablokowaniem mutexa");
		exit(EXIT_FAILURE);
	}
}

void unlock_mutex(pthread_mutex_t *mutex)
{
	if (pthread_mutex_unlock(mutex) != 0)
	{
		perror("pthread_mutex_lock - problem z odblokowaniem mutexa");
		exit(EXIT_FAILURE);
	}
}

void my_sleep(int qs)
{
	if (usleep(qs) != 0)
	{
		if (errno != EINTR)
		{
			perror("usleep");
			exit(EXIT_FAILURE);
		}
	}
}

void simple_error_handler(int status, const char *msg)
{
	if (status != 0)
	{
		fprintf(stderr, "%s, status: %d\n", msg, status);
        exit(EXIT_FAILURE);
	}
}

int licz_procesy_uzytkownika() {
    DIR *proc = opendir("/proc");
    if (!proc) 
	{
        perror("opendir /proc");
        exit(EXIT_FAILURE);
    }

    int uid = getuid();
    int liczba_procesow = 0;
    struct dirent *entry;

    while ((entry = readdir(proc)) != NULL) 
	{
        if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0) 
		{
            char path[256];
            sprintf(path, "/proc/%s/status", entry->d_name);

            FILE *status = fopen(path, "r");
            if (!status) continue;

            char line[256];
            while (fgets(line, sizeof(line), status)) 
			{
                if (strncmp(line, "Uid:", 4) == 0) 
				{
                    int process_uid;
                    sscanf(line, "Uid:\t%d", &process_uid);
                    if (process_uid == uid)
                        liczba_procesow++;
                    break;
                }
            }
            fclose(status);
        }
    }
    closedir(proc);
    return liczba_procesow;
}
