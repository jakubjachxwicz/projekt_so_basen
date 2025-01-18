#include "header.h"
#include "utils.c"

char* shm_czas_adres;
volatile bool stop_time;

void *czasomierz();
void czyszczenie();
void signal_handler(int sig);

pid_t pid_klienci, pid_kasjer, pid_ratownicy;
pthread_t t_czasomierz;
int shm_id, shm_czas_id, semafor, msq_kolejka_vip, msq_klient_ratownik;


int main()
{
    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);
    signal(SIGCONT, signal_handler);
    stop_time = false;
    
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

    // Semafory:
    // 0: klienci opuszczajacy basen
    // 1 - 3: obsluga zwyklych klientow przy kasie
    // 4: okresowe zamykanie
    // 5: klienci VIP
    // 6: kolejka do kompleksu basenow
    // 7: wyswietlanie stanu basenu pojedynczo
    semafor = semget(key, 8, 0600|IPC_CREAT);
    if (semafor == -1)
	{
		perror("semget - nie udalo sie utworzyc semafora");
		exit(EXIT_FAILURE);
	}
    if (semctl(semafor, 0, SETVAL, 1) == -1)
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
    if (semctl(semafor, 4, SETVAL, 1) == -1)
    {
        perror("semctl - nie mozna ustawic semafora");
        exit(EXIT_FAILURE);
    }
    if (semctl(semafor, 5, SETVAL, 1) == -1)
    {
        perror("semctl - nie mozna ustawic semafora");
        exit(EXIT_FAILURE);
    }
    if (semctl(semafor, 6, SETVAL, (X1 + X2 + X3) * 3) == -1)
    {
        perror("semctl - nie mozna ustawic semafora");
        exit(EXIT_FAILURE);
    }
    if (semctl(semafor, 7, SETVAL, 1) == -1)
    {
        perror("semctl - nie mozna ustawic semafora");
        exit(EXIT_FAILURE);
    }

    // Kolejka do komunikacji klient VIP / kasjer
    msq_kolejka_vip = msgget(key, IPC_CREAT | 0600);
    if (msq_kolejka_vip == -1)
    {
        perror("msgget - tworzenie kolejki kom. do wymiany klient VIP/kasjer");
        exit(EXIT_FAILURE);
    }

    // Kolejka do komuniakcji klient / ratownik
    msq_klient_ratownik = msgget(key_czas, IPC_CREAT | 0600);
    if (msq_klient_ratownik == -1)
    {
        perror("msgget - tworzenie kolejki kom. do wymiany klient/ratownik");
        exit(EXIT_FAILURE);
    }

    // Inicjowanie pam. wspoldzielonej do wymiany kasjer/klient
    shm_id = shmget(key, sizeof(struct dane_klienta), 0600|IPC_CREAT);
    if (shm_id == -1)
    {
        perror("shmget - tworzenie pamieci wspoldzielonej");
        exit(EXIT_FAILURE);
    }

    // Inicjowanie pam. wspoldzielonej do obslugi czasu
    shm_czas_id = shmget(key_czas, sizeof(int), 0600|IPC_CREAT);
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
    *shm_czas_adres = 0;

    // Inicjowanie kolejek FIFO do obslugi klientow opuszczajacych basen    
    if (mkfifo("fifo_basen_1", 0600) == -1 || mkfifo("fifo_basen_2", 0600) == -1 || mkfifo("fifo_basen_3", 0600) == -1)
    {
        if (errno != EEXIST)
        {
            perror("mkfifo - nie mozna utworzyc FIFO");
            exit(EXIT_FAILURE);
        }
    }
    
    // Tworzenie procesu ratowników
    pid_ratownicy = fork();
    if (pid_ratownicy < 0)
    {
        perror("fork error - proces ratownikow");
        exit(EXIT_FAILURE);
    } else if (pid_ratownicy == 0)
    {
        if (execl("./ratownik", "ratownik", NULL) == -1)
        {
            perror("execl - ratownik.c");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        // Tworzenie procesu kasjerów
        pid_kasjer = fork();
        if (pid_kasjer < 0)
        {
            perror("fork error - proces kasjera");
            kill(pid_ratownicy, SIGINT);
            exit(EXIT_FAILURE);
        } else if (pid_kasjer == 0)
        {
            if (execl("./kasjer", "kasjer", NULL) == -1)
            {
                perror("execl - kasjer.c");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            // Tworzenie procesu klientów
            pid_klienci = fork();
            if (pid_klienci < 0)
            {
                perror("fork error - proces klientow");
                kill(pid_ratownicy, SIGINT);
                kill(pid_kasjer, SIGINT);
                exit(EXIT_FAILURE);
            }
            else if (pid_klienci == 0)
            {
                if (execl("./klient", "klient", NULL) == -1)
                {
                    perror("execl - klient.c");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    set_color(RESET);
    printf("Klienci PID: %d, kasjer PID: %d, ratownicy PID: %d\n\n", pid_klienci, pid_kasjer, pid_ratownicy);

    if (pthread_create(&t_czasomierz, NULL, &czasomierz, NULL) != 0)
    {
        perror("pthread_create - watek do obslugi czasu");
        exit(EXIT_FAILURE);
    }

    czyszczenie();
    return 0;
}

// Obsługa czasu w symulacji
// Wartość zmiennej całkowitej w pamięci współdzielonej
// oznacza ilość sekund od godziny 9:00 w symulaci
void *czasomierz()
{
    int *jaki_czas = (int *)shm_czas_adres;
    while (*jaki_czas < DOBA + 900 && !stop_time)
    {
        // SEKUNDA - ilość mikrosekund jaką trwa sekunda czasu w symulacji
        if (usleep(SEKUNDA) != 0)
        {
            if (errno != EINTR)
            {
                perror("usleep - czasomierz symulacji");
                exit(EXIT_FAILURE);
            }
        }
        (*jaki_czas)++;
    }

    // O godzinie 21:15 kończę pracę wszystkich procesów
    if (kill(-pid_klienci, SIGINT) != 0)
    {
        if (errno != ESRCH)
        {
            perror("kill - zabijanie procesow klientow po skonczeniu symulacji");
            exit(EXIT_FAILURE);
        }
    }
    if (kill(pid_kasjer, SIGINT) != 0)
    {
        if (errno != ESRCH)
        {
            perror("kill - zabijanie procesu kasjera po skonczeniu symulacji");
            exit(EXIT_FAILURE);
        }
    }
    if (kill(-pid_ratownicy, SIGINT) != 0)
    {
        if (errno != ESRCH)
        {
            perror("kill - zabijanie procesow ratownikow po skonczeniu symulacji");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

// Usuwanie wszystkich strunktur
void czyszczenie()
{
    set_color(RESET);
    int status;
    pid_t finished;
    finished = waitpid(pid_klienci, &status, 0);
    if (finished == -1) perror("wait - klienci");  
    else if (WIFEXITED(status)) 
        printf("Proces klientow (PID: %d) zakonczyl sie z kodem: %d\n", finished, WEXITSTATUS(status));
    else
        printf("Proces klientow (PID: %d) zakonczyl sie w nieoczekiwany sposob, status: %d\n", finished, status);

    finished = waitpid(pid_kasjer, &status, 0);
    if (finished == -1) perror("wait - kasjer");  
    else if (WIFEXITED(status)) 
        printf("Proces kasjera (PID: %d) zakonczyl sie z kodem: %d\n", finished, WEXITSTATUS(status));
    else
        printf("Proces kasjera (PID: %d) zakonczyl sie w nieoczekiwany sposob, status: %d\n", finished, status);

    finished = waitpid(pid_ratownicy, &status, 0);
    if (finished == -1) perror("wait - ratownicy");  
    else if (WIFEXITED(status)) 
        printf("Proces ratownikow (PID: %d) zakonczyl sie z kodem: %d\n", finished, WEXITSTATUS(status));
    else
        printf("Proces ratownikow (PID: %d) zakonczyl sie w nieoczekiwany sposob, status: %d\n", finished, status);

    status = pthread_join(t_czasomierz, NULL);
    simple_error_handler(status, "pthread_join - watek czasomierza");

    if (unlink("fifo_basen_1") == -1 || unlink("fifo_basen_2") == -1 || unlink("fifo_basen_3") == -1) 
    {
        perror("unlink - usuwanie FIFO");
        exit(EXIT_FAILURE);
    }

    if (shmctl(shm_id, IPC_RMID, 0) == -1)
    {
        perror("shmctl - problem z usunieciem pamieci wspoldzielonej");
        exit(EXIT_FAILURE);
    }
    if (shmctl(shm_czas_id, IPC_RMID, 0) == -1)
    {
        perror("shmctl - problem z usunieciem pamieci wspoldzielonej");
        exit(EXIT_FAILURE);
    }

    if (semctl(semafor, 0, IPC_RMID) == -1)
	{
		perror("semctl - nie mozna uzunac semaforow");
		exit(EXIT_FAILURE);
	}

    if (msgctl(msq_kolejka_vip, IPC_RMID, 0) == -1)
    {
        perror("msgctl - problem przy usuwaniu kolejki kom. do obslugi klient VIP/kasjer");
        exit(EXIT_FAILURE);
    }
    if (msgctl(msq_klient_ratownik, IPC_RMID, 0) == -1)
    {
        perror("msgctl - problem przy usuwaniu kolejki kom. do obslugi klient/ratownik");
        exit(EXIT_FAILURE);
    }
}

void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        stop_time = true;
        
        czyszczenie();
        exit(0);
    }
    if (sig == SIGTSTP)
    {
        kill(-pid_ratownicy, SIGTSTP);
        kill(-pid_klienci, SIGTSTP);

        raise(SIGSTOP);
    }
    if (sig == SIGCONT)
    {
        kill(-pid_ratownicy, SIGCONT);
        kill(-pid_klienci, SIGCONT);
    }
}