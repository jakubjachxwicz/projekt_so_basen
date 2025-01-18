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


// Formatowanie czasu w symulacji do czytelnego formatu
void godz_sym(int sekundy, char* res)
{
    int sek_r = sekundy % 60;
    int minuty = sekundy / 60;
    int min_r = minuty % 60;
    int godziny = (minuty / 60) + 9;

    snprintf(res, 9, "%02d:%02d:%02d\n", godziny, min_r, sek_r);
}

// Podniesienie semafora
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

// Opuszczenie semafora
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

// Ustawienie koloru wyjścia terminala
void set_color(const char* color) 
{
    printf("%s", color);
}

// Dodanie PID klienta do tablicy (basen olimpijski i brodzik)
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

// Dodanie PID i wieku klienta do tablicy (basen rekreacyjny)
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

// Usuniecie PID klienta do tablicy (basen olimpijski i brodzik)
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

// Usuniecie PID i wieku klienta do tablicy (basen rekreacyjny)
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

// Liczenie ilosci osob przebywajacych na basenie
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

// Licznie sredniej wieku w basenie rekreacyjnym
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

// Funkcja blokowania mutexa z obsługą błędów
void lock_mutex(pthread_mutex_t *mutex)
{
	int status;
	if ((status = pthread_mutex_lock(mutex)) != 0)
	{
		fprintf(stderr, "pthread_mutex_lock - problem z zablokowaniem mutexa, status: %d\n", status);
		exit(EXIT_FAILURE);
	}
}

// Funkcja odblokowania mutexa z obsługą błędów
void unlock_mutex(pthread_mutex_t *mutex)
{
	int status;
	if ((status = pthread_mutex_unlock(mutex)) != 0)
	{
		fprintf(stderr, "pthread_mutex_lock - problem z odblokowaniem mutexa, status: %d\n", status);
		exit(EXIT_FAILURE);
	}
}

// Funkcja usleep z obsługą błędów
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

// Obsluga błędów dla funkcji nie ustawiających errno
void simple_error_handler(int status, const char *msg)
{
	if (status != 0)
	{
		fprintf(stderr, "%s, status: %d\n", msg, status);
        exit(EXIT_FAILURE);
	}
}

// Liczenie procesów użytkownika
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

// Wyświetlanie ilości oraz PID klientów (basen olimpijski i brodzik)
void wyswietl_klientow(int ktory_basen, int* klienci)
{
	set_color(RESET);
	switch (ktory_basen)
	{
		case 1:
			printf("****   W olimpijskim: %d ****\n", klienci[0]);
			for (int i = 1; i <= X1; i++)
				(i < X1) ? printf("%d, ", klienci[i]) : printf("%d\n", klienci[i]);
			printf("****************************\n");
		break;
		case 3:
			printf("*****   W brodziku: %d *****\n", klienci[0]);
			for (int i = 1; i <= X3; i++)
				(i < X3) ? printf("%d, ", klienci[i]) : printf("%d\n", klienci[i]);
			printf("***************************\n");
		break;
	}
}

// Wyświetlanie ilości, PID oraz wieku klientów (basen rekreacyjny)
void wyswietl_klientow_rek(int (*klienci)[X2 + 1])
{
	set_color(RESET);
	printf("****   W rekreacyjnym: %d ****\n", klienci[0][0]);
	for (int i = 1; i <= X2; i++)
		(i < X2) ? printf("%d, ", klienci[0][i]) : printf("%d\n", klienci[0][i]);
	for (int i = 1; i <= X2; i++)
		(i < X2) ? printf("%d, ", klienci[1][i]) : printf("%d\n", klienci[1][i]);
	printf("****************************\n");
}
