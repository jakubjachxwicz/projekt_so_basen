#pragma once

#include <sys/types.h>
#include <stdbool.h>

#define MTEXT_MAX 64
#define KOM_KLIENT 1
#define KOM_KASJER 2
#define KOM_RATOWNIK_1 3
#define KOM_RATOWNIK_2 4
#define KOM_RATOWNIK_3 5

// Ile mikrosekund trwa jedna sekunda w symulacji
#define SEKUNDA 5000

#define GODZINA 3600
#define MINUTA 60
#define DOBA 43200

#define X1 6
#define X2 16
#define X3 5

#define RESET       "\033[0m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define WHITE       "\033[37m"


struct dane_klienta
{
    int PID;
    int wiek;
    int wiek_opiekuna;
    bool pampers;
    bool czepek;
    double pieniadze;
    bool wpuszczony;
    bool VIP;
    int godz_wyjscia;
};

// Struktura komunikatu do wymiany klient / ratownik
struct kom_ratownik
{
    long mtype;
    pid_t ktype;
    char mtext[MTEXT_MAX];
    int wiek;
    int wiek_opiekuna;
};

// Struktura komunikatu do wymiany klient VIP / kasjer
struct kom_kolejka_vip
{
    long mtype;
    pid_t ktype;
    char mtext[MTEXT_MAX];
    int czas_wyjscia;
};
