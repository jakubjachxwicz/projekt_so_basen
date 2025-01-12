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
    stop_time = false;
    
    // Inicjowanie semaforkow
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
    // 1 - 3: klienci zwykli podchodzacy do kasy
    // 4: okresowe zamykanie
    // 5: klienci VIP
    // 6: kolejka do kompleksu basenow
    semafor = semget(key, 7, 0600|IPC_CREAT);
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

    // Inicjowanie kolejek komunikatow
    msq_kolejka_vip = msgget(key, IPC_CREAT | 0600);
    if (msq_kolejka_vip == -1)
    {
        perror("msgget - tworzenie kolejki kom. do wymiany klient VIP/kasjer");
        exit(EXIT_FAILURE);
    }

    msq_klient_ratownik = msgget(key_czas, IPC_CREAT | 0600);
    if (msq_klient_ratownik == -1)
    {
        perror("msgget - tworzenie kolejki kom. do wymiany klient/ratownik");
        exit(EXIT_FAILURE);
    }

    // Inicjowanie pam. wspoldzielonej do wymiany kasjer/klient/ratownik
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
    if (pthread_create(&t_czasomierz, NULL, &czasomierz, NULL) != 0)
    {
        perror("pthread_create - watek do obslugi czasu");
        exit(EXIT_FAILURE);
    }

    // Inicjowanie kolejek FIFO do obslugi klientow opuszczajacych basen    
    if (mkfifo("fifo_basen_1", 0600) == -1 || mkfifo("fifo_basen_2", 0600) == -1 || mkfifo("fifo_basen_3", 0600) == -1)
    {
        if (errno != EEXIST)
        {
            perror("mkfifo - nie mozna utworzyc FIFO");
            exit(EXIT_FAILURE);
        }
    }
    
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
        pid_kasjer = fork();
        if (pid_kasjer < 0)
        {
            perror("fork error - proces kasjera");
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
            pid_klienci = fork();
            if (pid_klienci < 0)
            {
                perror("fork error - proces klientow");
                exit(EXIT_FAILURE);
            }
            else if (pid_klienci == 0)
            {
                char pid_ratownicy_str[10];
                sprintf(pid_ratownicy_str, "%d", pid_ratownicy);
                char pid_kasjer_str[10];
                sprintf(pid_kasjer_str, "%d", pid_kasjer);

                if (execl("./klient", "klient", pid_ratownicy_str, pid_kasjer_str, NULL) == -1)
                {
                    perror("execl - klient.c");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    set_color(RESET);
    printf("Klienci PID: %d, kasjer PID: %d, ratownicy PID: %d\n\n", pid_klienci, pid_kasjer, pid_ratownicy);

    czyszczenie();
    return 0;
}


void *czasomierz()
{
    int *jaki_czas = (int *)shm_czas_adres;
    while (*jaki_czas < 44100 && !stop_time)
    {
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

    if (kill(-pid_klienci, SIGINT) != 0 || kill(-pid_ratownicy, SIGINT) != 0 || kill(-pid_ratownicy, SIGINT) != 0)
    {
        if (errno != ESRCH)
        {
            perror("kill - zabijanie procesow po skonczeniu symulacji");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}


void czyszczenie()
{
    set_color(RESET);
    int status;
    pid_t finished;
    finished = waitpid(pid_klienci, &status, 0);
    if (finished == -1) perror("wait - klienci");  
    else if (WIFEXITED(status)) 
        printf("Proces potomny (PID: %d) zakonczyl sie z kodem: %d\n", finished, WEXITSTATUS(status));
    else
        printf("Proces potomny (PID: %d) zakonczyl sie w nieoczekiwany sposob, status: %d\n", finished, status);

    finished = waitpid(pid_kasjer, &status, 0);
    if (finished == -1) perror("wait - kasjer");  
    else if (WIFEXITED(status)) 
        printf("Proces potomny (PID: %d) zakonczyl sie z kodem: %d\n", finished, WEXITSTATUS(status));
    else
        printf("Proces potomny (PID: %d) zakonczyl sie w nieoczekiwany sposob, status: %d\n", finished, status);

    finished = waitpid(pid_ratownicy, &status, 0);
    if (finished == -1) perror("wait - ratownicy");  
    else if (WIFEXITED(status)) 
        printf("Proces potomny (PID: %d) zakonczyl sie z kodem: %d\n", finished, WEXITSTATUS(status));
    else
        printf("Proces potomny (PID: %d) zakonczyl sie w nieoczekiwany sposob, status: %d\n", finished, status);

    if (pthread_join(t_czasomierz, NULL) != 0)
    {
        perror("pthread_join - watek czasomierza");
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
}