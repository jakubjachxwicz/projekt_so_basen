#include <pthread.h>
#include "utils.c"
#include "header.h"

#define MAX_KLIENCI 15

static void semafor_v(int semafor_id, int numer_semafora);
static void semafor_p(int semafor_id, int numer_semafora);
void signal_handler(int sig);
void *usuwanie_procesow();

pid_t pid_macierzysty;
pthread_t t_usuwanie_procesow;

bool flag_usuwanie;


int main()
{
    signal(SIGINT, signal_handler);
    pid_macierzysty = getpid();

    char godzina[9];
    flag_usuwanie = true;
    
    srand(time(NULL));

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
		perror("semget - blad dostepu do semaforow");
		exit(EXIT_FAILURE);
	}

    int shm_id = shmget(key, sizeof(struct dane_klienta), 0600);
    if (shm_id == -1)
    {
        perror("shmget - tworzenie pamieci wspoldzielonej");
        exit(EXIT_FAILURE);
    }

    char* shm_adres = (char*)shmat(shm_id, NULL, 0);
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

    pthread_create(&t_usuwanie_procesow, NULL, &usuwanie_procesow, NULL);

    // Tworzenie klientow
    while (*((int*)(shm_czas_adres)) < (43200 - 3600))
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
            struct dane_klienta klient;
            klient.PID = getpid();
            klient.wiek = (rand() % 70) + 1;
            klient.wiek_opiekuna = (klient.wiek < 10) ? ((rand() % 53) + 18) : 0;
            klient.pampers = (klient.wiek <= 3) ? true : false;
            klient.czepek = rand() % 2;
            klient.pieniadze = rand() % 100;
            klient.VIP = (rand() % 7 == 1 ) ? true : false;

            godz_sym(*((int *)shm_czas_adres), godzina);
            printf("[%s KLIENT PID = %d] w kolejce na basen\n", godzina, getpid());

            semafor_p(semafor, 1);
            memcpy(shm_adres, &klient, sizeof(struct dane_klienta));
            usleep(1000);
            semafor_v(semafor, 2);
                
            semafor_p(semafor, 3);
            memcpy(&klient, shm_adres, sizeof(struct dane_klienta));
            semafor_v(semafor, 1);

            if (klient.wpuszczony)
            {
                godz_sym(*((int *)shm_czas_adres), godzina);
                printf("[%s KLIENT PID = %d] wchodze na basenik\n", godzina, getpid());
            } else
            {
                godz_sym(*((int *)shm_czas_adres), godzina);
                printf("[%s KLIENT PID = %d] nie wpuszczono mnie na basen\n", godzina, getpid());
            }

            if (shmdt(shm_adres) == -1)
            {
                perror("shmdt - problem z odlaczeniem pamieci od procesu");
                exit(EXIT_FAILURE);
            }

            exit(0);
        }
    }


    while (wait(NULL) != -1) {}
    flag_usuwanie = false;
    pthread_join(t_usuwanie_procesow, NULL);
    exit(0);
}

void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        while (wait(NULL) != -1) {}
        exit(0);
    }
}

void *usuwanie_procesow()
{
    while (flag_usuwanie)
        wait(NULL);

    return 0;
}