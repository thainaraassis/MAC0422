#ifndef EP3_H
#define EP3_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* "P2\n"       → 3 bytes
    "256 256\n"  → 8 bytes
    "255\n"      → 4 bytes */
#define offset 15         
#define uaTotal 65536  // total de unidades de alocação

#define INF INT_MAX

int lePGM(FILE *f, int i);
void escrevePGM(FILE *f, int i, int val);

int firstFit(FILE *f, int m);
int nextFit(FILE *f, int m);
int bestFit(FILE *f, int m);
int worstFit(FILE *f, int m);

void compactaMemo(FILE *f);

#endif // EP3_H
