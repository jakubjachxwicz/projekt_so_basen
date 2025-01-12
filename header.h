#pragma once

#include <sys/types.h>
#include <stdbool.h>

#define MTEXT_MAX 64
#define KOM_KLIENT 1
#define KOM_KASJER 2
#define KOM_RATOWNIK_1 3
#define KOM_RATOWNIK_2 4
#define KOM_RATOWNIK_3 5

// Ile mikrosekund irl trwa jedna sekunda w symulacji
// 5000 - 3 min 36 s + 7 s
// 2500 - 1 min 48 s + 7 s
// 1250 - 54 s + 7 s
// 1667 - 1 min 12 s + 7 s
#define SEKUNDA 800
#define GODZINA 3600
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

struct komunikat
{
    long mtype;
    pid_t ktype;
    char mtext[MTEXT_MAX];
    int wiek;
    int wiek_opiekuna;
};