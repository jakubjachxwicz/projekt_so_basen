#include "header.h"
#include "utils.c"


static void semafor_v(int semafor_id, int numer_semafora);
static void semafor_p(int semafor_id, int numer_semafora);
void odlacz_pamiec();
void signal_handler(int sig);
void* klienci_vip();

char *shm_czas_adres;
struct dane_klienta *shm_adres;
pthread_t t_klienci_vip;
bool flag_obsluga_vip;
key_t key;


int main()
{
    signal(SIGINT, signal_handler);
	srand(time(NULL));
	
	struct dane_klienta klient;
	char godzina[9];

	key = ftok(".", 51);
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
		perror("KASJER: semget - blad dostepu do semaforow");
		exit(EXIT_FAILURE);
	}

	int shm_id = shmget(key, sizeof(struct dane_klienta), 0600);
    if (shm_id == -1)
    {
        perror("shmget - tworzenie pamieci wspoldzielonej");
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
        perror("shmget - tworzenie pamieci wspoldzielonej do obługi czasu");
        exit(EXIT_FAILURE);
    }

    shm_czas_adres = (char*)shmat(shm_czas_id, NULL, 0);
    if (shm_czas_adres == (char*)(-1))
    {
        perror("shmat - problem z dolaczeniem pamieci do obslugi czasu");
        exit(EXIT_FAILURE);
    }

	pthread_create(&t_klienci_vip, NULL, &klienci_vip, NULL);
	flag_obsluga_vip = true;

    while (*((int*)(shm_czas_adres)) < 43200)
    {
		semafor_p(semafor, 2);
        // Procesowanie klienta
		memcpy(&klient, shm_adres, sizeof(struct dane_klienta));
		usleep(SEKUNDA * 60);

		if ((klient.wiek > 18 || klient.wiek < 10) && klient.pieniadze >= 60)
		{
			klient.pieniadze -= 60;
			klient.wpuszczony = true;
			klient.godz_wyjscia = (*((int *)shm_czas_adres)) + 3600;
		} else if (klient.pieniadze >= 30)
		{
			klient.pieniadze -= 30;
			klient.wpuszczony = true;
			klient.godz_wyjscia = (*((int *)shm_czas_adres)) + 3600;
		} else
			klient.wpuszczony = false;

        memcpy(shm_adres, &klient, sizeof(struct dane_klienta));

		godz_sym(*((int *)shm_czas_adres), godzina);
        printf("[%s KASJER] Klient o PID = %d obsluzony\n", godzina, klient.PID);

        semafor_v(semafor, 3);
    }

	odlacz_pamiec();
	flag_obsluga_vip = false;
	pthread_join(t_klienci_vip, NULL);
	exit(0);
}

void odlacz_pamiec()
{
	if (shmdt(shm_adres) == -1)
	{
		perror("KASJER: shmdt - problem z odlaczeniem pamieci od procesu");
        exit(EXIT_FAILURE);
	}
	if (shmdt(shm_czas_adres) == -1)
	{
		perror("KASJER: shmdt - problem z odlaczeniem pamieci od procesu");
        exit(EXIT_FAILURE);
	}
}

void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
		odlacz_pamiec();
        exit(0);
    }
}

void* klienci_vip()
{
	char godzina[9];
	int msq_kolejka_vip = msgget(key, 0600);
	if (msq_kolejka_vip == -1)
    {
        perror("msgget - dostep do kolejki kom. do obslugi klient VIP/kasjer");
        exit(EXIT_FAILURE);
    }

	struct komunikat kom;
	
	while (flag_obsluga_vip)
	{
		kom.mtype = KOM_KASJER;
		if (msgrcv(msq_kolejka_vip, &kom, sizeof(kom) - sizeof(long), kom.mtype, 0) == -1)
        {
            perror("msgrcv -  kasjer: problem przy odbiorze komunikatu");
            exit(EXIT_FAILURE);
        }

		kom.mtype = kom.ktype;
		if (rand() % 30 == 16)
			strcpy(kom.mtext, "karnet nie wazny");
		else
			strcpy(kom.mtext, "zapraszam");

		if (msgsnd(msq_kolejka_vip, &kom, sizeof(kom) - sizeof(long), 0) == -1)
		{
            perror("msgsnd - kasjer: wysylanie komunikatu do klienta VIP");
            exit(EXIT_FAILURE);
        }

		godz_sym(*((int *)shm_czas_adres), godzina);
        printf("[%s KASJER] VIP o PID = %d obsluzony\n", godzina, kom.ktype);
	}

	return 0;
}