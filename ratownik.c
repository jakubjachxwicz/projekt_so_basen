#include "utils.c"
#include "header.h"


pid_t pid_ratownik1, pid_ratownik2, pid_ratownik3;

int main()
{
    struct komunikat kom;
	char godzina[9];
    
    key_t key_czas = ftok(".", 52);
    if (key_czas == -1)
	{
		perror("ftok - nie udalo sie utworzyc klucza");
		exit(EXIT_FAILURE);
	}

    int msq_klient_ratownik = msgget(key_czas, 0600);
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

    char* shm_czas_adres = (char*)shmat(shm_czas_id, NULL, 0);
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

                int wiek;
                while (*((int*)(shm_czas_adres)) < 43200)
                {
                    if (msgrcv(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), KOM_RATOWNIK_3, 0) == -1)
                    {
                        perror("msgrcv - odbieranie komunikatu do wejscia do brodzika");
                        exit(EXIT_FAILURE);
                    }

                    // Sprawdzanie czy moze wejsc
                    wiek = atoi(kom.mtext);
                    strcpy(kom.mtext, "ok");
                    kom.mtype = kom.ktype;
                    if (wiek > 5)
                        strcpy(kom.mtext, "wiek za duzy");

                    if (msgsnd(msq_klient_ratownik, &kom, sizeof(kom) - sizeof(long), 0) == -1)
                    {
                        perror("msgsnd - wysylanie komunikatu do wejscia do brodzika");
                        exit(EXIT_FAILURE);
                    }
                }
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