#!/bin/bash

# Kompilacja programu kasjer
gcc -pthread -o kasjer kasjer.c
if [ $? -ne 0 ]; then
    echo "Błąd kompilacji kasjer.c"
    exit 1
fi

# Kompilacja programu klient
gcc -pthread -o klient klient.c
if [ $? -ne 0 ]; then
    echo "Błąd kompilacji klient.c"
    exit 1
fi

# Kompilacja programu ratownik
gcc -o ratownik ratownik.c
if [ $? -ne 0 ]; then
    echo "Błąd kompilacji ratownik.c"
    exit 1
fi

# Kompilacja programu symulacja z użyciem pthread
gcc -pthread -o symulacja symulacja.c
if [ $? -ne 0 ]; then
    echo "Błąd kompilacji symulacja.c"
    exit 1
fi

echo "Kompilacja zakończona pomyślnie."