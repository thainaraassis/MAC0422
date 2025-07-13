#ifndef EP1_H
#define EP1_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <stdbool.h>

/* ---------- Estrutura que representa um processo ---------- */

typedef struct {
    char name[33];    
    int t0, dt, deadline;     
    int tf, tr, cumpriu;      
    int id;    // para garantir estabilidade durante a ordenação
    int rest; // representa o resto de tempo que falta para um processo executar quando há preempção
    int cpu; // representa a CPU em que o processo está rodando
    int ready; // representa se o processo pode rodar ou nao, por conta de preempção
    int start; // representa se já criamos uma thread para aquele processo, caso haja preempção e ele volte para a fila de ready
    pthread_mutex_t mutex; // mutex e cond para controle quando há preempção
    pthread_cond_t cond;
    int quantum; // quantidade de quantum alocada
    int ran; //para saber o tanto que o processo já rodou
} Process;

/* ----------------- QUEUE ----------------- */
// Adaptado da Queue de Robert Sedgewick e Kevin Wayne
// https://algs4.cs.princeton.edu/code/edu/princeton/cs/algs4/Queue.java.html

typedef struct node {
    Process *p;
    struct node *next;
} QNode;

typedef struct queue {
    QNode *first;
    QNode *last;
    int     n;
} Queue;

void queue_init (Queue *q);        
bool queue_is_empty(Queue *q);
void enqueue(Queue *q, Process *item);  // insere no final da fila
Process *dequeue(Queue *q);             // remove da frente
Process *queue_peek(Queue *q);           // somente vê o da frente

/* ----------------- MINPQ ----------------- */
// Adaptado do MinPQ de Robert Sedgewick e Kevin Wayne
// https://algs4.cs.princeton.edu/61event/MinPQ.java.html 

#define MAX_PQ_SIZE 100

int compare(Process *a, Process *b);
void exch(int i, int j);
void swim(int k);
void sink(int k);

bool isEmpty();
void insert(Process *p);
Process* delMin();
Process *peekMin();

/* ---------- funcoes auxiliares ---------- */
void read_trace(char *trace_file);
void write_output(char *output_file);

int compara_t0(const void *a, const void *b);
int compara_dt(const void *a, const void *b);
double delta_atual(struct timespec *a, struct timespec *b);
int get_quantum(Process *p, int t_now);

/* ---------- escalonadores ---------- */
void *executaFCFS(void *pr);
void FCFS(char *output_file);

void *executaSRTN(void *pr);
void SRTN(char *output_file);

void *executaPS(void *pr);
void priority_scheduling(char *output_file);

#endif 