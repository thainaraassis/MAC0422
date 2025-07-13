#ifndef EP2_H
#define EP2_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#define MAXFAIXA 10

/* ---------- Estrutura que representa um ciclista ---------- */
typedef struct {
    int id;
    double vel;
    int i; // i posição da pista que está (linha)
    int j; // j faixa que o ciclista está (coluna)
    int voltas; // quantas voltas já completou
    int ativo; // 1- se ainda está correndo | 0 se quebrou ou completou
    int avancar; // assim podemos controlar quando quem está a 30km/h pode avançar para proxima posição
    int mCorridos; // assim conseguimos marcar o quanto ele ja andou da pista e quantas voltas isso daria;
    unsigned int randSeed; // semente para decidir aleatoriamente a velocidade do ciclista
    int quebrado;
    int printaQuebrado;
    int *tCruzamento;          // vetor de tempos por volta
    int  capCruzamento;        // capacidade atual do vetor
    int posEliminado;
} Ciclista;

/* ---------- Estrutura que representa o ranking dos ciclistas ---------- */
typedef struct {
    int id;
    int voltas;
    int ativo;
    int mCorridos;
    int tempoUltimaVolta;
} Ranking;

/* ---------- Barreira ---------- */
typedef struct {
    int active;        // quantas threads ativas
    int arrived;       // quantas já chegaram nesta fase
    pthread_mutex_t m;
    pthread_cond_t coord, worker;
} barrier_t;

void barrier_init(barrier_t *b, int n);
void barrier_remove_thread(barrier_t *b);
void barrier_wait_worker(barrier_t *b);
void barrier_wait_coordinator(barrier_t *b);
void barrier_destroy(barrier_t *b);

/* ---------- Funcoes ---------- */
void mostraPista(void);
bool quebrou(Ciclista *c);
double novaVel(Ciclista *c);
void tentaDescer(Ciclista *c);
void vaiFrente(Ciclista *c, int ni, int nj, int oi, int oj);
void tentaUltrapassar(Ciclista *c, int ni, int oi, int oj);
void *executaCorrida(void *p);
int cmpVoltas(const void *a, const void *b);
int verificaVoltaCompletada(void);
int eliminaCiclista(void);
int cmpCiclistas(const void *a, const void *b);
void imprimeRankingFinal(void); 
void verificaQuebrados(void);
void libera(void);
void loop(void);
void init(void);

#endif 