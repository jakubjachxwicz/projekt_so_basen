#include "utils.c"
#include "header.h"


pid_t pid_macierzysty, pid_ratownik1, pid_ratownik2, pid_ratownik3;
char godzina[9];
char* shm_czas_adres;
pthread_mutex_t mutex_olimp, mutex_rek, mutex_brod;
pthread_t t_wpuszczanie_klientow, t_wychodzenie_klientow;
struct komunikat kom;
int msq_klient_ratownik, fifo_fd;
volatile bool flag_obsluga_klientow;

void* wpuszczanie_klientow_brodzik(void *arg);
void* wychodzenie_klientow_brodzik(void *arg);
void signal_handler(int sig);

int main()
{
    signal(SIGINT, signal_handler);
    pid_macierzysty = getpid();
    
    flag_obsluga_klientow = true;
    
    key_t key_czas = ftok(".", 52);
    if (key_czas == -1)
	{
		perror("ftok - nie udalo sie utworzyc klucza");
		exit(EXIT_FAILURE);
	}

    msq_klient_ratownik = msgget(key_czas, 0600);
    if (msq_klient_ratownik == -1)
    {
        perror("msgget - dostep do kolejki kom. do obslugi klient/ratownik");
        exit(EXIT_FAILURE);
    }

    // Uzyskanie pamieci wspoldzielonej do obslugi czasu
    int shm_czas_id = shmget(key_czas, sizeof(int), 0600);
    if (shm_czas_id == -1)
    {
        perror("shmget - tworzenie pamieci wspoldzielonej do obługi czasu");
        exit(EXIT_FAILURE);
    }

    shm_czas_adres = (char*)shmat(shm_czas_id, NULL, 0);
    if (shm_czas_adres == (char*)(-1))
    {
        perror("shmat - problem z dolaczeniem pamieci do obslugi czasu");
        exit(EXIT_FAILURE);
    }

    
    pid_ratownik1 = fork();
    if (pid_ratownik1 < 0)
    {
        perror("fork - proces ratownika 1");
        exit(EXIT_FAILURE);
    } else if (pid_ratownik1 == 0)
    {
        // Kod ratownika 1 - olimpijski
        printf("[RATOWNIK 1 PID = %d]\n", getpid());
    }
    else
    {
        pid_ratownik2 = fork();
        if (pid_ratownik2 < 0)
        {
            perror("fork - proces ratownika 2");
            exit(EXIT_FAILURE);
        } else if (pid_ratownik2 == 0)
        {
            // Kod ratownika 2 - rekreacyjny
            printf("[RATOWNIK 2 PID = %d]\n", getpid());
        }
        else
        {
            pid_ratownik3 = fork();
            if (pid_ratownik3 < 0)
            {
                perror("fork - proces ratownika 3");
                exit(EXIT_FAILURE);
            } else if (pid_ratownik3 == 0)
            {
                // Kod ratownika 3 - brodzik
                printf("[RATOWNIK 3 PID = %d]\n", getpid());

                int klienci[X3 + 1];
                klienci[0] = 0;
                for (int i = 1; i <= X3; i++)
                    klienci[i] = -1;    

                pthread_mutex_init(&mutex_brod, NULL);

                if (pthread_create(&t_wpuszczanie_klientow, NULL, &wpuszczanie_klientow_brodzik, klienci) != 0)
                {
                    perror("pthread_create - tworzenie watku do wpuszczania klientow do brodzika");
                    exit(EXIT_FAILURE);
                }

                printf("halo kochani tworzymy watki AUUUUUUUUU\n");

                if (pthread_create(&t_wychodzenie_klientow, NULL, &wychodzenie_klientow_brodzik, klienci) != 0)
                {
                    perror("pthread_create - tworzenie watku do wpuszczania klientow do brodzika");
                    exit(EXIT_FAILURE);
                }

                pthread_join(t_wpuszczanie_klientow, NULL);
                pthread_join(t_wychodzenie_klientow, NULL);
                pthread_mutex_destroy(&mutex_brod);
            }
        }
    }


    waitpid(pid_ratownik1, NULL, 0);
    waitpid(pid_ratownik2, NULL, 0);
    waitpid(pid_ratownik3, NULL, 0);

    if (shmdt(shm_czas_adres) == -1)
	{
		perror("KASJER: shmdt - problem z odlaczeniem pamieci od procesu");
        exit(EXIT_FAILURE);
	}

    exit(0);
}

void signal_handler(int sig)
{
    flag_obsluga_klientow = false;

    if (getpid() == pid_macierzysty)
    {
        kill(pid_ratownik1, SIGINT);
        kill(pid_ratownik2, SIGINT);
        kill(pid_ratownik3, SIGINT);
    }

    pthread_cancel(t_wpuszczanie_klientow);
    pthread_cancel(t_wychodzenie_klientow);
}

void* wpuszczanie_klientow_brodzik(void *arg)
{
    int *klienci = (int *)arg;
    int wiek;
    kom.ktype = KOM_RATOWNIK_3;
    kom.mtype = KOM_RATOWNIK_3;
    while (flag_obsluga_klientow)
    {
        if (msgrcv(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), KOM_RATOWNIK_3, 0) == -1)
        {
            if (errno != EINTR)
            {
                perror("msgrcv - ratownik 3: odbieranie komunikatu do wejscia do brodzika");
                exit(EXIT_FAILURE);
            }
        }

        // Sprawdzanie czy moze wejsc
        wiek = atoi(kom.mtext);
        strcpy(kom.mtext, "ok");
        kom.mtype = kom.ktype;


        pthread_mutex_lock(&mutex_brod);
        if (wiek > 5)
            strcpy(kom.mtext, "wiek za duzy");
        else if (klienci[0] == X3)
            strcpy(kom.mtext, "brodzik pelny");
        else
        {
            klienci[0]++;
            dodaj_do_tablicy(klienci, X3, kom.ktype);

            printf("*****   W brodziku:   *****\n");
            for (int i = 1; i <= X3; i++)
                printf("%d, ", klienci[i]);
            printf("\n***************************\n");
        }
        pthread_mutex_unlock(&mutex_brod);

        if (msgsnd(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), 0) == -1)
        {
            perror("msgsnd - ratownik 3: wysylanie komunikatu do wejscia do brodzika");
            exit(EXIT_FAILURE);
        }
    }

    return NULL;
}

void* wychodzenie_klientow_brodzik(void *arg)
{
    printf("WATEK WYPUSZCZANIA STARUJE\n");
    
    while (flag_obsluga_klientow)
    {
        int *klienci = (int *)arg;
        int pid;

        printf("TU RATOWNIK, CZEKAM NA WYCHODZACYCH\n");

        fifo_fd = open("fifo_basen_3", O_RDONLY);
        if (fifo_fd == -1)
        {
            perror("open - nie mozna otworzyc FIFO (ratownik przy brodziku)");
            exit(EXIT_FAILURE);
        }

        if (read(fifo_fd, &pid, sizeof(pid)) == -1)
        {
            perror("read - nie mozna odczytac FIFO (ratownik przy brodziku)");
            exit(EXIT_FAILURE);
        }

        close(fifo_fd);

        pthread_mutex_lock(&mutex_brod);

        godz_sym(*((int *)shm_czas_adres), godzina);
        printf("[%s RATOWNIK %d] klient PID = %d idzie do domu\n", godzina, getpid(), pid);

        klienci[0]--;
        usun_z_tablicy(klienci, X3, pid);

        printf("*****   W brodziku:   *****\n");
        for (int i = 1; i <= X3; i++)
            printf("%d, ", klienci[i]);
        printf("\n***************************\n");
        pthread_mutex_unlock(&mutex_brod);
        printf("JAK NIE POPIERDOLI TO POJEBIE\n");

        usleep(SEKUNDA * 5);

        printf("NO POPIERDOLI MNIE CHYBA\n");
    }

    printf("@@@@@@@@@@@@@@@@@@@@@@@\n");
    printf("@@@@@@@@BYE BYE@@@@@@@@\n");
    printf("@@@@@@@@@@@@@@@@@@@@@@@\n");

    return NULL;
}