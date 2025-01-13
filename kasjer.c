#include "header.h"
#include "utils.c"


static void semafor_v(int semafor_id, int numer_semafora);
static void semafor_p(int semafor_id, int numer_semafora);
void czyszczenie();
void signal_handler(int sig);
void* klienci_vip();
void* okresowe_zamkniecie();

char *shm_czas_adres;
char godzina[9];
struct dane_klienta *shm_adres;
pthread_t t_klienci_vip, t_okresowe_zamkniecie;
pthread_mutex_t mutex_czas_wyjscia;
int ostatni_klient_czas_wyjscia, semafor;
bool flag_obsluga_vip;
key_t key;


int main()
{
    signal(SIGINT, signal_handler);
	srand(time(NULL));
	
	struct dane_klienta klient;

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

    semafor = semget(key, 7, 0600|IPC_CREAT);
    if (semafor == -1)
	{
		perror("semget - nie udalo sie dolaczyc do semafora");
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

	if (pthread_mutex_init(&mutex_czas_wyjscia, NULL) != 0)
	{
		perror("pthread_mutex_init - mutex_czas_wyjscia");
		exit(EXIT_FAILURE);
	}

	if (pthread_create(&t_klienci_vip, NULL, &klienci_vip, NULL) != 0)
	{
		perror("pthread_create - tworzenie watku do obslugi klientow VIP");
		exit(EXIT_FAILURE);
	}
	if (pthread_create(&t_okresowe_zamkniecie, NULL, &okresowe_zamkniecie, NULL) != 0)
	{
		perror("pthread_create - tworzenie watku dookresowego zamykania");
		exit(EXIT_FAILURE);
	}
	flag_obsluga_vip = true;

    while (true)
    {
		semafor_p(semafor, 2);
		memcpy(&klient, shm_adres, sizeof(struct dane_klienta));
		//usleep(SEKUNDA * 60);

		if ((klient.wiek > 18 || klient.wiek < 10) && klient.pieniadze >= 60)
		{
			klient.pieniadze -= 60;
			klient.wpuszczony = true;
			klient.godz_wyjscia = (*((int *)shm_czas_adres)) + GODZINA;

			lock_mutex(&mutex_czas_wyjscia);
			ostatni_klient_czas_wyjscia = klient.godz_wyjscia;
			unlock_mutex(&mutex_czas_wyjscia);
		} else if (klient.pieniadze >= 30)
		{
			klient.pieniadze -= 30;
			klient.wpuszczony = true;
			klient.godz_wyjscia = (*((int *)shm_czas_adres)) + GODZINA;

			lock_mutex(&mutex_czas_wyjscia);
			ostatni_klient_czas_wyjscia = klient.godz_wyjscia;
			unlock_mutex(&mutex_czas_wyjscia);
		} else
			klient.wpuszczony = false;

        memcpy(shm_adres, &klient, sizeof(struct dane_klienta));

		godz_sym(*((int *)shm_czas_adres), godzina);
		set_color(CYAN);
        printf("[%s KASJER] Klient o PID = %d obsluzony\n", godzina, klient.PID);

        semafor_v(semafor, 3);
    }

	exit(0);
}

void czyszczenie()
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

	if (pthread_join(t_okresowe_zamkniecie, NULL) != 0)
	{
		perror("pthread_join - t_okresowe_zamkniecie");
		exit(EXIT_FAILURE);
	}

	flag_obsluga_vip = false;
	if (pthread_cancel(t_klienci_vip) != 0)
	{
		perror("pthread_cancel - t_klienci_vip");
		exit(EXIT_FAILURE);
	}
	if (pthread_join(t_klienci_vip, NULL) != 0)
	{
		perror("pthread_join - t_klienci_vip");
		exit(EXIT_FAILURE);
	}

	if (pthread_mutex_destroy(&mutex_czas_wyjscia) != 0)
	{
		perror("pthread_mutex_destroy - mutex_czas_wyjscia");
		exit(EXIT_FAILURE);
	}
}

void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
		czyszczenie();
        exit(0);
    }
}

void* klienci_vip()
{
	printf("Watek oblugi klientow VIP uruchomiony\n");
	char godzina[9];
	int msq_kolejka_vip = msgget(key, 0600);
	if (msq_kolejka_vip == -1)
    {
        perror("msgget - dostep do kolejki kom. do obslugi klient VIP/kasjer");
        exit(EXIT_FAILURE);
    }

	struct kom_kolejka_vip kom;

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
		{
			strcpy(kom.mtext, "zapraszam");
			kom.czas_wyjscia = (*((int *)shm_czas_adres)) + GODZINA;

			lock_mutex(&mutex_czas_wyjscia);
			ostatni_klient_czas_wyjscia = kom.czas_wyjscia;
			unlock_mutex(&mutex_czas_wyjscia);
		}

		if (msgsnd(msq_kolejka_vip, &kom, sizeof(kom) - sizeof(long), 0) == -1)
		{
            perror("msgsnd - kasjer: wysylanie komunikatu do klienta VIP");
            exit(EXIT_FAILURE);
        }

		godz_sym(*((int *)shm_czas_adres), godzina);
		set_color(CYAN);
        printf("[%s KASJER] VIP o PID = %d obsluzony\n", godzina, kom.ktype);
	}

	return NULL;
}

void* okresowe_zamkniecie()
{
	int czas;
	// Watek czeka do godziny 13:00
	while ((czas = *((int *)shm_czas_adres)) < GODZINA * 4)
		my_sleep(SEKUNDA * 10);

	// Blokuje wpuszczanie nowych klientow
	semafor_p(semafor, 4);
	godz_sym(*((int *)shm_czas_adres), godzina);
	set_color(CYAN);
	printf("[%s KASJER] KASA ZAMKNIETA, CZEKAM NA WYJSCIE KLIENTOW\n", godzina);

	// Czeka az ostatni klient wyjdzie
	while ((czas = *((int *)shm_czas_adres)) < ostatni_klient_czas_wyjscia)
		my_sleep(SEKUNDA);

	godz_sym(*((int *)shm_czas_adres), godzina);
	set_color(CYAN);
	printf("[%s KASJER] KOMPLEKS BASENOW ZAMKNIETY, OTWARCIE ZA GODZINE\n", godzina);

	// Czeka godzine od wyjscia ostatniego klienta
	while ((czas = *((int *)shm_czas_adres)) < ostatni_klient_czas_wyjscia + GODZINA)
		my_sleep(SEKUNDA);

	godz_sym(*((int *)shm_czas_adres), godzina);
	set_color(CYAN);
	printf("[%s KASJER] KOMPLEKS BASENOW OTWARTY\n", godzina);

	semafor_v(semafor, 4);
	
	return NULL;
}