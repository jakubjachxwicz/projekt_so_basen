#include "utils.c"
#include "header.h"

#define MAX_KLIENCI 15

void signal_handler(int sig);
void sigusr_handler(int sig, siginfo_t *info, void *context);
void *usuwanie_procesow();
void opuszczenie_basenu();
void odlaczeni_pamieci();

char godzina[9];
char* shm_czas_adres;
struct dane_klienta* shm_adres;
int semafor, ktory_basen, zakaz_wejscia[3];

pthread_t t_usuwanie_procesow;
volatile bool flag_usuwanie, flag_opuszczony_semafor;

int main(int argc, char *argv[])
{
    if (setpgid(0, 0) == -1)
    {
        perror("setpgid - glowny proces klientow");
        exit(EXIT_FAILURE);
    }
    
    signal(SIGINT, signal_handler);

    struct sigaction sa;
    sa.sa_sigaction = sigusr_handler;
    sa.sa_flags = SA_SIGINFO;
    if (sigemptyset(&sa.sa_mask) == -1)
    {
        perror("sigemptyset - sighandler dla klientow");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGUSR1, &sa, NULL) == -1 || sigaction(SIGUSR2, &sa, NULL) == -1)
    {
        perror("sigaction - odbieranie sygnalow przez klienta");
        exit(EXIT_FAILURE);
    }

    flag_usuwanie = true;
    flag_opuszczony_semafor = false;
    memset(zakaz_wejscia, 0, sizeof(zakaz_wejscia));

    // Inicjowanie kluczy i semaforow
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

    semafor = semget(key, 8, 0600);
    if (semafor == -1)
	{
		perror("semget - dostep do zbioru semaforow");
		exit(EXIT_FAILURE);
	}

    // Uzyskanie pamieci wspoldzielonej do wymiany danych klient / kasjer
    int shm_id = shmget(key, sizeof(struct dane_klienta), 0600);
    if (shm_id == -1)
    {
        perror("shmget - dostep do pamieci wspoldzielonej");
        exit(EXIT_FAILURE);
    }
    shm_adres = (struct dane_klienta*)shmat(shm_id, NULL, 0);
    if (shm_adres == (struct dane_klienta*)(-1))
    {
        perror("shmat - problem z dolaczeniem pamieci");
        exit(EXIT_FAILURE);
    }

    // Uzyskanie pamieci wspoldzielonej do obslugi czasu
    int shm_czas_id = shmget(key_czas, sizeof(int), 0600);
    if (shm_czas_id == -1)
    {
        perror("shmget - dolaczenie pamieci wspoldzielonej do obługi czasu");
        exit(EXIT_FAILURE);
    }
    shm_czas_adres = (char*)shmat(shm_czas_id, NULL, 0);
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

    int status = pthread_create(&t_usuwanie_procesow, NULL, &usuwanie_procesow, NULL);
    simple_error_handler(status, "pthread_create - usuwanie procesow zombie");

    // Tworzenie klientow
    while (*((int*)(shm_czas_adres)) < (DOBA - 3600))
    {        
        if (licz_procesy_uzytkownika() > 40000) continue;

        // Okresowe zamkniecie kompleksu - semafor jest opuszczony i nie pojawiaja sie nowi klienci
        semafor_p(semafor, 4);
        pid_t pid = fork();

        if (pid < 0)
        {
            if (errno == EAGAIN)
                perror("fork - nowy klient, za duzo procesow");
            else if (errno == ENOMEM)
                perror("fork - brak pamieci w systemie");
            else
                perror("fork - nowy klient");

            kill(-getpid(), SIGINT);
            exit(EXIT_FAILURE);
        } else if (pid == 0)
        {
            srand(getpid());
            signal(SIGTSTP, SIG_DFL);
            semafor_v(semafor, 4);
            if (setpgid(0, getppid()) == -1)
            {
                perror("setpgid - klient");
                exit(EXIT_FAILURE);
            }

            semafor_p(semafor, 6);

            if (*((int*)(shm_czas_adres)) > DOBA - 3600)
            {
                semafor_v(semafor, 6);
                odlaczeni_pamieci();
                exit(0);
            }
            
            // Kod dzialania klienta
            struct dane_klienta klient;
            klient.PID = getpid();
            klient.wiek = (rand() % 70) + 1;
            klient.wiek_opiekuna = (klient.wiek < 10) ? ((rand() % 53) + 18) : 0;
            klient.pampers = (klient.wiek <= 3) ? true : false;
            klient.czepek = rand() % 2;
            klient.pieniadze = rand() % 100;
            klient.VIP = (rand() % 7 == 1 ) ? true : false;

            if (klient.VIP)
            {
                // Wymiana danych klient VIP / kasjer
                semafor_p(semafor, 5);
                godz_sym(*((int *)shm_czas_adres), godzina);
                set_color(BLUE);
                printf("[%s VIP PID = %d, wiek: %d] podchodzi do kasy\n", godzina, getpid(), klient.wiek);

                struct kom_kolejka_vip kom;
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
                    klient.godz_wyjscia = kom.czas_wyjscia;
                }
                else
                    klient.wpuszczony = false;
                semafor_v(semafor, 5);
            } else
            {
                // Wymiana danych klient zwykly / kasjer
                godz_sym(*((int *)shm_czas_adres), godzina);
                set_color(BLUE);
                printf("[%s KLIENT PID = %d, wiek: %d] w kolejce na basen\n", godzina, getpid(), klient.wiek);

                semafor_p(semafor, 1);
                memcpy(shm_adres, &klient, sizeof(struct dane_klienta));
                semafor_v(semafor, 2);

                semafor_p(semafor, 3);
                memcpy(&klient, shm_adres, sizeof(struct dane_klienta));
                semafor_v(semafor, 1);
            }
                
            if (klient.wpuszczony)
            {
                godz_sym(*((int *)shm_czas_adres), godzina);
                set_color(BLUE);
                printf("[%s KLIENT PID = %d] wchodze do kompleksu basenow\n", godzina, klient.PID);
                
                ktory_basen = 0;
                struct kom_ratownik kom;
                kom.ktype = klient.PID;
                kom.wiek = klient.wiek;
                kom.wiek_opiekuna = klient.wiek_opiekuna;

                int choice = (rand() % 3) + 1;

                while (true)
                {
                    // Klient przekroczyl swoj czas i wychodzi z basenu
                    if (*((int *)shm_czas_adres) > klient.godz_wyjscia)
                    {
                        if (ktory_basen)
                            opuszczenie_basenu();

                        godz_sym(*((int *)shm_czas_adres), godzina);
                        set_color(BLUE);
                        printf("[%s KLIENT PID = %d] ide do domu\n", godzina, getpid());
                        break;
                    }

                    if (!ktory_basen)
                    {
                        // Klient wybiera, na ktory basen chce wejsc
                        choice = (rand() % 3) + 1;
                        while (zakaz_wejscia[choice - 1])
                            choice = (rand() % 3) + 1;
                        godz_sym(*((int *)shm_czas_adres), godzina);
                        set_color(YELLOW);
                        printf("[%s KLIENT PID = %d] chce wejsc na basen: %d\n", godzina, klient.PID, choice);

                        // Wysyla komunikat do ratownika
                        kom.mtype = choice + 2;
                        if (msgsnd(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), 0) == -1)
                        {
                            perror("msgsnd - wysylanie komunikatu do wejscia do basenu");
                            exit(EXIT_FAILURE);
                        }
                        if (msgrcv(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), kom.ktype, 0) == -1)
                        {
                            perror("msgrcv - odbieranie komunikatu do wejscia do basenu");
                            exit(EXIT_FAILURE);
                        }

                        // Sprawdza wiadomosc zwrotna
                        if (strcmp(kom.mtext, "ok") == 0)
                        {
                            ktory_basen = choice;
                            godz_sym(*((int *)shm_czas_adres), godzina);
                            set_color(GREEN);
                            printf("[%s KLIENT PID = %d] wchodze do basenu nr %d\n", godzina, klient.PID, ktory_basen);
                        }

                        if (!ktory_basen)
                        {
                            set_color(RED);
                            printf("KLIENT PID = %d, odpowiedz: %s\n", klient.PID, kom.mtext);
                        }
                    }
                    my_sleep(SEKUNDA * 120);
                }
            } else
            {
                godz_sym(*((int *)shm_czas_adres), godzina);
                set_color(RED);
                printf("[%s KLIENT PID = %d] nie wpuszczono mnie do kompleksu basenow\n", godzina, getpid());
            }

            odlaczeni_pamieci();
            semafor_v(semafor, 6);
            exit(0);
        }

        // my_sleep(SEKUNDA * ((rand() % 360) + 120));
        my_sleep(SEKUNDA * ((rand() % 180) + 60));
        // my_sleep(SEKUNDA * ((rand() % 6) + 5));
    }

    flag_usuwanie = false;
    status = pthread_join(t_usuwanie_procesow, NULL);
    simple_error_handler(status, "pthread_join - watek usuwajacy procesy zombie");

    while (wait(NULL) != -1) { }
    odlaczeni_pamieci();
    exit(0);
}

void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        while (wait(NULL) != -1) { }
        exit(0);
    }
}

void sigusr_handler(int sig, siginfo_t *info, void *context)
{
    if (sig == SIGUSR1)
    {
        set_color(BLUE);
        printf("KLIENT PID = %d otrzymalem SIGUSR1 na basen %d\n", getpid(), info->si_value.sival_int);

        // Blokuje wybor danego basenu i z niego wychodzi
        zakaz_wejscia[info->si_value.sival_int - 1] = 1;
        ktory_basen = 0;
    } else if (sig == SIGUSR2)
    {
        set_color(BLUE);
        printf("KLIENT PID = %d otrzymalem SIGUSR2 na basen %d\n", getpid(), info->si_value.sival_int);

        // Odblokowuje wybor danego basenu
        zakaz_wejscia[info->si_value.sival_int - 1] = 0;
    }
}

void *usuwanie_procesow()
{
    while (flag_usuwanie)
    {
        if (wait(NULL) == -1) 
        {
            if (errno != ECHILD)
            {
                perror("wait - usuwanie procesow zombie");
                exit(EXIT_FAILURE);
            }
        }
    }

    return 0;
}

// Wysylanie przez FIFO swojego PID do ratownika, aby ten usunal go z listy klientow na basenie
void opuszczenie_basenu()
{
    semafor_p(semafor, 0);
    godz_sym(*((int *)shm_czas_adres), godzina);
    set_color(BLUE);
    printf("[%s KLIENT PID = %d] wychodze z basenu\n", godzina, getpid());
    
    char file_name[13];
    strcpy(file_name, "fifo_basen_");
    sprintf(file_name + strlen(file_name), "%d", ktory_basen);
    int fd = open(file_name, O_WRONLY);
    if (fd < 0)
    {
        perror("open - nie mozna otworzyc FIFO (klient)");
        exit(EXIT_FAILURE);
    }

    pid_t pid = getpid();
    if (write(fd, &pid, sizeof(pid)) == -1)
    {
        perror("write - pisanie do FIFO (klient)");
        exit(EXIT_FAILURE);
    }
    if (close(fd) == -1)
    {
        perror("close - zamkniecie fifo (klient)");
        exit(EXIT_FAILURE);
    }
    semafor_v(semafor, 0);
}

void odlaczeni_pamieci()
{
    if (shmdt(shm_adres) == -1 || shmdt(shm_czas_adres) == -1)
    {
        perror("shmdt - problem z odlaczeniem pamieci od procesu");
        exit(EXIT_FAILURE);
    }
}
