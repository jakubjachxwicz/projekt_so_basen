#include "utils.c"
#include "header.h"

#define MAX_KLIENCI 15

static void semafor_v(int semafor_id, int numer_semafora);
static void semafor_p(int semafor_id, int numer_semafora);
void signal_handler(int sig);
void *usuwanie_procesow();

pthread_t t_usuwanie_procesow;

volatile bool flag_usuwanie;


int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);
    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);

    pid_t pid_ratownicy = atoi(argv[1]);
    pid_t pid_kasjer = atoi(argv[2]);

    printf("Ja: %d, kasjer: %d, ratownicy: %d\n", getpid(), pid_kasjer, pid_ratownicy);

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

    struct dane_klienta* shm_adres = (struct dane_klienta*)shmat(shm_id, NULL, 0);
    if (shm_adres == (struct dane_klienta*)(-1))
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

    int msq_kolejka_vip = msgget(key, 0600);
    if (msq_kolejka_vip == -1)
    {
        perror("msgget - dostep do kolejki kom. do obslugi klient VIP/kasjer");
        exit(EXIT_FAILURE);
    }
    int msq_klient_ratownik = msgget(key_czas, 0600);
    if (msq_klient_ratownik == -1)
    {
        perror("msgget - dostep do kolejki kom. do obslugi klient/ratownik");
        exit(EXIT_FAILURE);
    }

    pthread_create(&t_usuwanie_procesow, NULL, &usuwanie_procesow, NULL);

    // Tworzenie klientow
    while (*((int*)(shm_czas_adres)) < (43200 - 3600))
    {        
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
            // klient.wiek = (rand() % 70) + 1;
            klient.wiek = (rand() % 70) + 1;
            klient.wiek_opiekuna = (klient.wiek < 10) ? ((rand() % 53) + 18) : 0;
            klient.pampers = (klient.wiek <= 3) ? true : false;
            klient.czepek = rand() % 2;
            klient.pieniadze = rand() % 100;
            klient.VIP = (rand() % 7 == 1 ) ? true : false;

            if (klient.VIP)
            {
                godz_sym(*((int *)shm_czas_adres), godzina);
                printf("[%s VIP PID = %d, wiek: %d] podchodzi do kasy\n", godzina, getpid(), klient.wiek);
                
                struct komunikat kom;
                kom.mtype = KOM_KASJER;
                kom.ktype = klient.PID;
                strcpy(kom.mtext, "pokazuje karnet VIP");

                if (msgsnd(msq_kolejka_vip, &kom, sizeof(kom) - sizeof(long), 0) == -1)
                {
                    perror("msgsnd - wysylanie komunikatu klienta VIP");
                    exit(EXIT_FAILURE);
                }

                if (msgrcv(msq_kolejka_vip, &kom, sizeof(kom) - sizeof(long), kom.ktype, 0) == -1)
                {
                    perror("msgrcv - odbieranie komunikatu klienta VIP");
                    exit(EXIT_FAILURE);
                }

                if (strcmp(kom.mtext, "zapraszam") == 0)
                {
                    klient.wpuszczony = true;
                    klient.godz_wyjscia = (*((int *)shm_czas_adres)) + 3600;
                }
                else
                    klient.wpuszczony = false;
            } else
            {
                godz_sym(*((int *)shm_czas_adres), godzina);
                printf("[%s KLIENT PID = %d, wiek: %d] w kolejce na basen\n", godzina, getpid(), klient.wiek);

                semafor_p(semafor, 1);
                memcpy(shm_adres, &klient, sizeof(struct dane_klienta));
                usleep(1000);
                semafor_v(semafor, 2);

                semafor_p(semafor, 3);
                memcpy(&klient, shm_adres, sizeof(struct dane_klienta));
                semafor_v(semafor, 1);
            }
                
            if (klient.wpuszczony)
            {
                godz_sym(*((int *)shm_czas_adres), godzina);
                printf("[%s KLIENT PID = %d] wchodze do szatni\n", godzina, klient.PID);
                
                int ktory_basen = 0;
                struct komunikat kom;
                kom.ktype = klient.PID;
                kom.wiek = klient.wiek;
                kom.wiek_opiekuna = klient.wiek_opiekuna;

                int fd, choice = (rand() % 3) + 1;

                while (true)
                {
                    if (*((int *)shm_czas_adres) > klient.godz_wyjscia)
                    {
                        godz_sym(*((int *)shm_czas_adres), godzina);
                        printf("[%s KLIENT PID = %d] pora wyjscia\n", godzina, klient.PID);

                        if (ktory_basen)
                        {
                            char file_name[13];
                            strcpy(file_name, "fifo_basen_");
                            sprintf(file_name + strlen(file_name), "%d", ktory_basen);
                            semafor_p(semafor, 0);
                            fd = open(file_name, O_WRONLY);
                            if (fd < 0)
                            {
                                perror("open - nie mozna otworzyc FIFO (klient)");
                                exit(EXIT_FAILURE);
                            }


                            pid_t pid = klient.PID;
                            if (write(fd, &pid, sizeof(pid)) == -1)
                            {
                                perror("write - pisanie do FIFO (klient)");
                                exit(EXIT_FAILURE);
                            }
                            close(fd);
                            semafor_v(semafor, 0);
                        }

                        break;
                    }

                    if (!ktory_basen)
                    {
                        choice = (rand() % 3) + 1;
                        printf("KLIENT PID = %d, CHCE WEJSC NA BASEN: %d\n", klient.PID, choice);
                        if (choice == 1)
                        {
                            kom.mtype = KOM_RATOWNIK_1;
                            if (msgsnd(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), 0) == -1)
                            {
                                perror("msgsnd - wysylanie komunikatu do wejscia do basenu olimpijskiego");
                                exit(EXIT_FAILURE);
                            }
                            if (msgrcv(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), kom.ktype, 0) == -1)
                            {
                                perror("msgrcv - odbieranie komunikatu do wejscia do basenu olimpijskiego");
                                exit(EXIT_FAILURE);
                            }

                            if (strcmp(kom.mtext, "ok") == 0)
                            {
                                ktory_basen = 1;
                                godz_sym(*((int *)shm_czas_adres), godzina);
                                printf("[%s KLIENT PID = %d] wchodze do basenu olimpijskiego\n", godzina, klient.PID);
                            }
                        } else if (choice == 2)
                        {
                            kom.mtype = KOM_RATOWNIK_2;
                            if (msgsnd(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), 0) == -1)
                            {
                                perror("msgsnd - wysylanie komunikatu do wejscia do basenu rekreacyjnego");
                                exit(EXIT_FAILURE);
                            }
                            if (msgrcv(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), kom.ktype, 0) == -1)
                            {
                                perror("msgrcv - odbieranie komunikatu do wejscia do basenu rekreacyjnego");
                                exit(EXIT_FAILURE);
                            }

                            if (strcmp(kom.mtext, "ok") == 0)
                            {
                                ktory_basen = 2;
                                godz_sym(*((int *)shm_czas_adres), godzina);
                                printf("[%s KLIENT PID = %d] wchodze do basenu rekreacyjnego\n", godzina, klient.PID);
                            }
                        } else if (choice == 3)
                        {
                            kom.mtype = KOM_RATOWNIK_3;
                            if (msgsnd(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), 0) == -1)
                            {
                                perror("msgsnd - wysylanie komunikatu do wejscia do brodzika");
                                exit(EXIT_FAILURE);
                            }
                            if (msgrcv(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), kom.ktype, 0) == -1)
                            {
                                perror("msgrcv - odbieranie komunikatu do wejscia do brodzika");
                                exit(EXIT_FAILURE);
                            }

                            if (strcmp(kom.mtext, "ok") == 0)
                            {
                                ktory_basen = 3;
                                godz_sym(*((int *)shm_czas_adres), godzina);
                                printf("[%s KLIENT PID = %d] wchodze do brodzika\n", godzina, klient.PID);
                            }
                        }
                    }

                    usleep(SEKUNDA * 120);
                }
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

        sleep((rand() % 6) + 2);
    }


    while (wait(NULL) != -1) {}
    flag_usuwanie = false;
    pthread_join(t_usuwanie_procesow, NULL);

    kill(pid_ratownicy, SIGINT);
    kill(pid_kasjer, SIGINT);

    exit(0);
}

void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        while (wait(NULL) != -1) {}
        exit(0);
    } else if (sig == SIGUSR1)
    {
        printf("KLIENT PID = %d otrzymalem SIGUSR1\n", getpid());
    } else if (sig == SIGUSR2)
    {
        printf("KLIENT PID = %d otrzymalem SIGUSR2\n", getpid());
    }
}

void *usuwanie_procesow()
{
    while (flag_usuwanie)
        wait(NULL);

    return 0;
}