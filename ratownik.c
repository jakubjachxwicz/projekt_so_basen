#include "utils.c"
#include "header.h"


pid_t pid_macierzysty, pid_ratownik1, pid_ratownik2, pid_ratownik3;
char godzina[9];
char* shm_czas_adres;
pthread_mutex_t mutex_olimp, mutex_rek, mutex_brod;
pthread_t t_wpuszczanie_klientow, t_wychodzenie_klientow, t_wysylanie_sygnalu;
struct kom_ratownik kom;
int msq_klient_ratownik, fifo_fd, ktory_basen;
bool zakaz_wstepu;
volatile bool flag_obsluga_klientow;

void* wpuszczanie_klientow_olimpijski(void *arg);
void* wpuszczanie_klientow_rekreacyjny(void *arg);
void* wpuszczanie_klientow_brodzik(void *arg);
void* wychodzenie_klientow(void *arg);
void* wysylanie_sygnalu(void *arg);
void signal_handler(int sig);

int main()
{
    signal(SIGINT, signal_handler);
    srand(time(NULL));

    pid_macierzysty = getpid();
    setpgid(0, 0);
    zakaz_wstepu = false;
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
        setpgid(0, getppid());
        ktory_basen = 1;

        int klienci[X1 + 1];
        memset(klienci, 0, sizeof(klienci));  

        pthread_mutex_init(&mutex_olimp, NULL);

        if (pthread_create(&t_wpuszczanie_klientow, NULL, &wpuszczanie_klientow_olimpijski, klienci) != 0)
        {
            perror("pthread_create - tworzenie watku do wpuszczania klientow do brodzika");
            exit(EXIT_FAILURE);
        }

        if (pthread_create(&t_wysylanie_sygnalu, NULL, &wysylanie_sygnalu, klienci) != 0)
        {
            perror("pthread_create - tworzenie watku do wysylania sygnalow");
            exit(EXIT_FAILURE);
        }

        if (pthread_create(&t_wychodzenie_klientow, NULL, &wychodzenie_klientow, klienci) != 0)
        {
            perror("pthread_create - tworzenie watku do wpuszczania klientow do brodzika");
            exit(EXIT_FAILURE);
        }

        if (pthread_join(t_wysylanie_sygnalu, NULL) != 0)
        {
            perror("pthread_join - ratownik 1t_wysylanie_sygnalu");
            exit(EXIT_FAILURE);
        }
        if (pthread_join(t_wpuszczanie_klientow, NULL) != 0)
        {
            perror("pthread_join - ratownik 1 t_wpuszczanie_klientow");
            exit(EXIT_FAILURE);
        }
        if (pthread_join(t_wychodzenie_klientow, NULL) != 0)
        {
            perror("pthread_join - ratownik 1 t_wychodzenie_klientow");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_destroy(&mutex_olimp);

        exit(0);
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
            setpgid(0, getppid());
            ktory_basen = 2;

            int klienci[2][X2 + 1];
            memset(klienci, 0, sizeof(klienci));

            pthread_mutex_init(&mutex_rek, NULL);

            if (pthread_create(&t_wpuszczanie_klientow, NULL, &wpuszczanie_klientow_rekreacyjny, klienci) != 0)
            {
                perror("pthread_create - tworzenie watku do wpuszczania klientow do basenu rekreacyjnego");
                exit(EXIT_FAILURE);
            }

            if (pthread_create(&t_wysylanie_sygnalu, NULL, &wysylanie_sygnalu, klienci) != 0)
            {
                perror("pthread_create - tworzenie watku do wysylania sygnalow");
                exit(EXIT_FAILURE);
            }

            if (pthread_create(&t_wychodzenie_klientow, NULL, &wychodzenie_klientow, klienci) != 0)
            {
                perror("pthread_create - tworzenie watku do wpuszczania klientow do basenu rekreacyjnego");
                exit(EXIT_FAILURE);
            }

            if (pthread_join(t_wysylanie_sygnalu, NULL) != 0)
            {
                perror("pthread_join - ratownik 2 t_wysylanie_sygnalu");
                exit(EXIT_FAILURE);
            }
            if (pthread_join(t_wpuszczanie_klientow, NULL) != 0)
            {
                perror("pthread_join - ratownik 2 t_wpuszczanie_klientow");
                exit(EXIT_FAILURE);
            }
            if (pthread_join(t_wychodzenie_klientow, NULL) != 0)
            {
                perror("pthread_join - ratownik 2 t_wychodzenie_klientow");
                exit(EXIT_FAILURE);
            }

            pthread_mutex_destroy(&mutex_rek);

            exit(0);
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
                setpgid(0, getppid());
                ktory_basen = 3;

                int klienci[X3 + 1];
                memset(klienci, 0, sizeof(klienci));
                pthread_mutex_init(&mutex_brod, NULL);

                if (pthread_create(&t_wpuszczanie_klientow, NULL, &wpuszczanie_klientow_brodzik, klienci) != 0)
                {
                    perror("pthread_create - tworzenie watku do wpuszczania klientow do brodzika");
                    exit(EXIT_FAILURE);
                }

                if (pthread_create(&t_wysylanie_sygnalu, NULL, &wysylanie_sygnalu, klienci) != 0)
                {
                    perror("pthread_create - tworzenie watku do wysylania sygnalow");
                    exit(EXIT_FAILURE);
                }

                if (pthread_create(&t_wychodzenie_klientow, NULL, &wychodzenie_klientow, klienci) != 0)
                {
                    perror("pthread_create - tworzenie watku do wpuszczania klientow do brodzika");
                    exit(EXIT_FAILURE);
                }

                if (pthread_join(t_wysylanie_sygnalu, NULL) != 0)
                {
                    perror("pthread_join - ratownik 3 t_wysylanie_sygnalu");
                    exit(EXIT_FAILURE);
                }
                if (pthread_join(t_wpuszczanie_klientow, NULL) != 0)
                {
                    perror("pthread_join - ratownik 3 t_wpuszczanie_klientow");
                    exit(EXIT_FAILURE);
                }
                if (pthread_join(t_wychodzenie_klientow, NULL) != 0)
                {
                    perror("pthread_join - ratownik 3 t_wychodzenie_klientow");
                    exit(EXIT_FAILURE);
                }

                pthread_mutex_destroy(&mutex_brod);
                exit(0);
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
    int result;
    if ((result = pthread_cancel(t_wpuszczanie_klientow)) != 0)
    {
        if (result != ESRCH)
        {
            perror("pthread_cancel - t_wpuszczanie_klientow");
            exit(EXIT_FAILURE);
        }
    }
    if ((result = pthread_cancel(t_wychodzenie_klientow)) != 0)
    {
        if (result != ESRCH)
        {
            perror("pthread_cancel - t_wychodzenie_klientow");
            exit(EXIT_FAILURE);
        }
    }
    if ((result = pthread_cancel(t_wysylanie_sygnalu)) != 0)
    {
        if (result != ESRCH)
        {
            perror("pthread_cancel - t_wysylanie_sygnalu");
            exit(EXIT_FAILURE);
        }
    }
}

void* wpuszczanie_klientow_olimpijski(void *arg)
{
    int *klienci = (int *)arg;
    int wiek;
    kom.ktype = KOM_RATOWNIK_1;
    kom.mtype = KOM_RATOWNIK_1;
    while (flag_obsluga_klientow)
    {
        if (msgrcv(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), KOM_RATOWNIK_1, 0) == -1)
        {
            if (errno != EINTR)
            {
                perror("msgrcv - ratownik 1: odbieranie komunikatu do wejscia do basenu olimpijskiego");
                exit(EXIT_FAILURE);
            }
        }

        // Sprawdzanie czy moze wejsc
        wiek = kom.wiek;
        strcpy(kom.mtext, "ok");
        kom.mtype = kom.ktype;

        pthread_mutex_lock(&mutex_olimp);
        if (zakaz_wstepu)
            strcpy(kom.mtext, "zakaz wstepu");
        else if (wiek < 18)
            strcpy(kom.mtext, "wiek za niski");
        else if (klienci[0] == X1)
            strcpy(kom.mtext, "basen pelny");
        else
        {
            klienci[0]++;
            dodaj_do_tablicy(klienci, X1, kom.ktype);

            set_color(RESET);
            printf("****   W olimpijskim:   ****\n");
            for (int i = 1; i <= X1; i++)
                (i < X1) ? printf("%d, ", klienci[i]) : printf("%d\n", klienci[i]);
            printf("***************************\n");
        }
        pthread_mutex_unlock(&mutex_olimp);

        if (msgsnd(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), 0) == -1)
        {
            perror("msgsnd - ratownik 1: wysylanie komunikatu do wejscia do basenu olimpijskiego");
            exit(EXIT_FAILURE);
        }
    }

    return NULL;
}

void* wpuszczanie_klientow_rekreacyjny(void *arg)
{
    int (*klienci)[X2 + 1] = (int (*)[X2 + 1])arg;
    int wiek, wiek_opiekuna;
    kom.ktype = KOM_RATOWNIK_2;
    kom.mtype = KOM_RATOWNIK_2;
    while (flag_obsluga_klientow)
    {
        if (msgrcv(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), KOM_RATOWNIK_2, 0) == -1)
        {
            if (errno != EINTR)
            {
                perror("msgrcv - ratownik 2: odbieranie komunikatu do wejscia do basenu rekreacyjnego");
                exit(EXIT_FAILURE);
            }
        }

        // Sprawdzanie czy moze wejsc
        wiek = kom.wiek;
        wiek_opiekuna = kom.wiek_opiekuna;
        strcpy(kom.mtext, "ok");
        kom.mtype = kom.ktype;
        int ile_osob = (wiek < 10) ? 2 : 1;

        pthread_mutex_lock(&mutex_rek);
        double sr = srednia_wieku(klienci[1], X2, wiek + wiek_opiekuna);
        if (zakaz_wstepu)
            strcpy(kom.mtext, "zakaz wstepu");
        else if (klienci[0][0] + ile_osob > X2)
            strcpy(kom.mtext, "basen pelny");
        else if (sr > 40)
            strcpy(kom.mtext, "srednia wieku za wysoka");
        else
        {
            klienci[0][0] += ile_osob;
            dodaj_do_tablicy_X2(klienci, X2, kom.ktype, wiek);
            if (wiek_opiekuna)
                dodaj_do_tablicy_X2(klienci, X2, kom.ktype, wiek_opiekuna);

            set_color(RESET);
            printf("****   W rekreacyjnym:   ****\n");
            for (int i = 1; i <= X2; i++)
                (i < X2) ? printf("%d, ", klienci[0][i]) : printf("%d\n", klienci[0][i]);
            for (int i = 1; i <= X2; i++)
                (i < X2) ? printf("%d, ", klienci[1][i]) : printf("%d\n", klienci[1][i]);
            printf("***************************\n");
        }
        pthread_mutex_unlock(&mutex_rek);

        if (msgsnd(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), 0) == -1)
        {
            perror("msgsnd - ratownik 2: wysylanie komunikatu do wejscia do basenu rekreacyjnego");
            exit(EXIT_FAILURE);
        }
    }

    return NULL;
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
        wiek = kom.wiek;
        strcpy(kom.mtext, "ok");
        kom.mtype = kom.ktype;

        pthread_mutex_lock(&mutex_brod);
        if (zakaz_wstepu)
            strcpy(kom.mtext, "zakaz wstepu");
        else if (wiek > 5)
            strcpy(kom.mtext, "wiek za duzy");
        else if (klienci[0] == X3)
            strcpy(kom.mtext, "brodzik pelny");
        else
        {
            klienci[0]++;
            dodaj_do_tablicy(klienci, X3, kom.ktype);

            set_color(RESET);
            printf("*****   W brodziku:   *****\n");
            for (int i = 1; i <= X3; i++)
                (i < X3) ? printf("%d, ", klienci[i]) : printf("%d\n", klienci[i]);
            printf("***************************\n");
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

void* wychodzenie_klientow(void *arg)
{
    while (flag_obsluga_klientow)
    {
        int *klienci = (int *)arg;
        int pid;

        char file_name[13];
        strcpy(file_name, "fifo_basen_");
        sprintf(file_name + strlen(file_name), "%d", ktory_basen);

        fifo_fd = open(file_name, O_RDONLY);
        if (fifo_fd == -1)
        {
            perror("open - nie mozna otworzyc FIFO (ratownik)");
            exit(EXIT_FAILURE);
        }

        if (read(fifo_fd, &pid, sizeof(pid)) == -1)
        {
            perror("read - nie mozna odczytac FIFO (ratownik)");
            exit(EXIT_FAILURE);
        }

        close(fifo_fd);

        switch (ktory_basen)
        {
            case 1:
                pthread_mutex_lock(&mutex_olimp);

                godz_sym(*((int *)shm_czas_adres), godzina);
                set_color(MAGENTA);
                printf("[%s RATOWNIK %d] klient PID = %d idzie do domu\n", godzina, getpid(), pid);

                klienci[0]--;
                usun_z_tablicy(klienci, X1, pid);

                set_color(RESET);
                printf("****   W olimpijskim:   ****\n");
                for (int i = 1; i <= X1; i++)
                    (i < X1) ? printf("%d, ", klienci[i]) : printf("%d\n", klienci[i]);
                printf("***************************\n");
                pthread_mutex_unlock(&mutex_olimp);
                break;
            case 2:
                pthread_mutex_lock(&mutex_rek);

                godz_sym(*((int *)shm_czas_adres), godzina);
                set_color(MAGENTA);
                printf("[%s RATOWNIK %d] klient PID = %d idzie do domu\n", godzina, getpid(), pid);

                int (*klienci_x2)[X2 + 1] = (int (*)[X2 + 1])klienci;
                int ile = ile_osob(klienci_x2[0], X2, pid);

                klienci_x2[0][0] -= ile;
                for (int j = 0; j < ile; j++)
                    usun_z_tablicy_X2(klienci_x2, X2, pid);

                set_color(RESET);
                printf("****   W rekreacyjnym:   ****\n");
                for (int i = 1; i <= X2; i++)
                    (i < X2) ? printf("%d, ", klienci_x2[0][i]) : printf("%d\n", klienci_x2[0][i]);
                for (int i = 1; i <= X2; i++)
                    (i < X2) ? printf("%d, ", klienci_x2[1][i]) : printf("%d\n", klienci_x2[1][i]);
                printf("***************************\n");
                pthread_mutex_unlock(&mutex_rek);
                break;
            case 3:
                pthread_mutex_lock(&mutex_brod);

                godz_sym(*((int *)shm_czas_adres), godzina);
                set_color(MAGENTA);
                printf("[%s RATOWNIK %d] klient PID = %d idzie do domu\n", godzina, getpid(), pid);

                klienci[0]--;
                usun_z_tablicy(klienci, X3, pid);

                set_color(RESET);
                printf("*****   W brodziku:   *****\n");
                for (int i = 1; i <= X3; i++)
                    (i < X3) ? printf("%d, ", klienci[i]) : printf("%d\n", klienci[i]);
                printf("***************************\n");
                pthread_mutex_unlock(&mutex_brod);
                break;
        }
    }

    return NULL;
}

void* wysylanie_sygnalu(void *arg)
{
    pthread_t tid = pthread_self();  // Pobierz ID wątku
    
    int *klienci = (int *)arg;
    union sigval sig_data;
    sig_data.sival_int = ktory_basen;

    while (*((int*)(shm_czas_adres)) < (DOBA - 7200))
    {
        usleep(SEKUNDA * 1800);
        if (rand() % 6 == ktory_basen)
        {
            godz_sym(*((int *)shm_czas_adres), godzina);
            set_color(MAGENTA);
            printf("[%s RATOWNIK %d] wysylam SIGUSR1 do klientow basenu nr %d\n", godzina, getpid(), ktory_basen);
            
            int wyrzuceni[(ktory_basen == 1 ? X1 : ((ktory_basen == 2) ? X2 : X3))];
            zakaz_wstepu = true;
            switch (ktory_basen)
            {
                case 1:
                    pthread_mutex_lock(&mutex_olimp);
                    memset(wyrzuceni, 0, sizeof(wyrzuceni));
                    for (int i = 1; i <= X1; i++)
                    {
                        if (klienci[i])
                        {
                            wyrzuceni[i - 1] = klienci[i];
                            if (sigqueue(klienci[i], SIGUSR1, sig_data) == -1)
                            {
                                if (errno != ESRCH)
                                {
                                    perror("sigqueue - SIGUSR1 z basenu nr 1");
                                    exit(EXIT_FAILURE);
                                }
                            }
                            klienci[i] = 0;
                        }
                    }
                    klienci[0] = 0;
                    pthread_mutex_unlock(&mutex_olimp);

                    usleep(SEKUNDA * ((rand() % 1200) + 600));
                    for (int i = 0; i < X1; i++)
                        if (wyrzuceni[i])
                        {
                            if (sigqueue(wyrzuceni[i], SIGUSR2, sig_data) == -1)
                            {
                                if (errno != ESRCH)
                                {
                                    perror("sigqueue - SIGUSR2 z basenu nr 1");
                                    exit(EXIT_FAILURE);
                                }
                            }
                        }
                    break;
                case 2:
                    pthread_mutex_lock(&mutex_rek);
                    int (*klienci_x2)[X2 + 1] = (int (*)[X2 + 1])klienci;
                    memset(wyrzuceni, 0, sizeof(wyrzuceni));
                    for (int i = 1; i <= X2; i++)
                    {
                        if (klienci_x2[0][i])
                        {
                            wyrzuceni[i - 1] = klienci_x2[0][i];
                            if (sigqueue(klienci_x2[0][i], SIGUSR1, sig_data) == -1)
                            {
                                if (errno != ESRCH)
                                {
                                    perror("sigqueue - SIGUSR1 z basenu nr 2");
                                    exit(EXIT_FAILURE);
                                }
                            }
                            klienci_x2[0][i] = 0;
                            klienci_x2[1][i] = 0;
                        }
                    }
                    klienci_x2[0][0] = 0;
                    pthread_mutex_unlock(&mutex_rek);

                    usleep(SEKUNDA * ((rand() % 1200) + 600));
                    for (int i = 0; i < X2; i++)
                        if (wyrzuceni[i] != 0)
                        {
                            if (sigqueue(wyrzuceni[i], SIGUSR2, sig_data) == -1)
                            {
                                if (errno != ESRCH)
                                {
                                    perror("sigqueue - SIGUSR2 z basenu nr 2");
                                    exit(EXIT_FAILURE);
                                }
                            }
                        }
                    break;
                case 3:
                    pthread_mutex_lock(&mutex_brod);
                    memset(wyrzuceni, 0, sizeof(wyrzuceni));
                    for (int i = 1; i <= X3; i++)
                    {
                        if (klienci[i])
                        {
                            wyrzuceni[i - 1] = klienci[i];
                            if (sigqueue(klienci[i], SIGUSR1, sig_data) == -1)
                            {
                                if (errno != ESRCH)
                                {
                                    perror("sigqueue - SIGUSR1 z basenu nr 2");
                                    exit(EXIT_FAILURE);
                                }
                            }
                            klienci[i] = 0;
                        }
                    }
                    klienci[0] = 0;
                    pthread_mutex_unlock(&mutex_brod);

                    usleep(SEKUNDA * ((rand() % 1200) + 600));
                    for (int i = 0; i < X3; i++)
                        if (wyrzuceni[i] != 0)
                        {
                            if (sigqueue(wyrzuceni[i], SIGUSR2, sig_data) == -1)
                            {
                                if (errno != ESRCH)
                                {
                                    perror("sigqueue - SIGUSR2 z basenu nr 3");
                                    exit(EXIT_FAILURE);
                                }
                            }
                        }
                    break;
            }

            godz_sym(*((int *)shm_czas_adres), godzina);
            set_color(MAGENTA);
            printf("[%s RATOWNIK %d] wysylam SIGUSR2 do klientow basenu nr %d\n", godzina, getpid(), ktory_basen);
            zakaz_wstepu = false;
        }
    }
    
    return NULL;
}