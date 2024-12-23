#pragma once

#include <sys/types.h>
#include <stdbool.h>

#define MTEXT_MAX 64
#define KOM_KLIENT 1
#define KOM_KASJER 2


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
};

struct komunikat
{
    long mtype;
    pid_t ktype;
    char mtext[MTEXT_MAX];
};