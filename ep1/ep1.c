#include "ep1.h"          

/* ---------- variáveis globais ---------- */
Process *processes;
int sizeP;

struct timespec time_start;

int nCPU;
int ocupCPU;

int preemp;
Process **procRunning;

Queue readyQ;
Process* heap[MAX_PQ_SIZE];
int sizePQ = 0;

pthread_mutex_t mutexQ;
pthread_mutex_t mutexPQ;
pthread_mutex_t mutexLOOP;

/* ----------------- QUEUE ----------------- */

void queue_init(Queue *q) {
    q->first = q->last = NULL;
    q->n = 0;
}

bool queue_is_empty(Queue *q) {
    return q->first == NULL; 
}

void enqueue(Queue *q, Process *item) {
    QNode *no = malloc(sizeof *no);
    no->p = item;
    no->next = NULL;

    if (queue_is_empty(q))
        q->first = q->last = no;
    else {
        q->last->next = no;
        q->last = no;
    }
    q->n++;
}

Process *dequeue(Queue *q) {
    if (queue_is_empty(q)) return NULL;

    QNode *no = q->first;
    Process *it = no->p;

    q->first = no->next;
    if (!q->first) q->last = NULL;
    free(no);
    q->n--;
    return it;
}

Process *queue_peek(Queue *q){
    return queue_is_empty(q) ? NULL : q->first->p;
}

/* ----------------- MINPQ ----------------- */

int compare(Process *a, Process *b) {
    if (a->dt != b->dt) return a->dt - b->dt; // ordena pelo tempo total de execução
    return a->id - b->id; // desempate por ordem de chegada original
}

void exch(int i, int j) {
    Process* temp = heap[i];
    heap[i] = heap[j];
    heap[j] = temp;
}

void swim(int k) {
    while (k > 1 && compare(heap[k], heap[k/2]) < 0) {
        exch(k, k/2);
        k = k/2;
    }
}

void sink(int k) {
    while (2*k <= sizePQ) {
        int j = 2*k;
        if (j < sizePQ && compare(heap[j+1], heap[j]) < 0) j++;
        if (compare(heap[k], heap[j]) <= 0) break;
        exch(k, j);
        k = j;
    }
}

bool isEmpty() {
    return sizePQ == 0;
}

void insert(Process *p) {
    if (sizePQ + 1 >= MAX_PQ_SIZE) {
        fprintf(stderr, "Heap overflow\n");
        exit(1);
    }
    heap[++sizePQ] = p;
    swim(sizePQ);
}

Process* delMin() {
    if (isEmpty()) return NULL;
    Process *min = heap[1];
    exch(1, sizePQ--);
    sink(1);
    return min;
}

Process *peekMin() {
    if (sizePQ == 0) return NULL;
    return heap[1];
}

/* ------------- funcoes auxiliares ------------- */

/**
 * Lê os processos de um arquivo de entrada e os armazena no vetor global `processes`.
 * Cada linha do arquivo deve conter: nome t0 dt deadline
 */
void read_trace(char *trace) {
    FILE *file = fopen(trace, "r");
    if (!file) {
        perror("Erro ao abrir o arquivo de trace");
        exit(1);
    }

    char line[200];
    sizeP = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        processes = realloc(processes, (sizeP + 1) * sizeof(Process));
        sscanf(line, "%s %d %d %d", 
            processes[sizeP].name, &processes[sizeP].t0, &processes[sizeP].dt, &processes[sizeP].deadline);
            processes[sizeP].id = sizeP;
            processes[sizeP].rest = processes[sizeP].dt;
            processes[sizeP].cpu = -1;
            processes[sizeP].ready = 0;
            processes[sizeP].start = 0;
            pthread_mutex_init(&processes[sizeP].mutex, NULL);
            pthread_cond_init(&processes[sizeP].cond, NULL);
            processes[sizeP].quantum = 0;
            processes[sizeP].ran = 0;
            sizeP++;
    }

    fclose(file);
}

/**
 * Escreve a saída dos resultados da simulação no formato:
 * nome tr tf cumpriu
 * Ao final, escreve o número total de preempções.
 */
void write_output(char *output) {
    FILE *out = fopen(output, "w");
    if (!out) {
        perror("Erro ao abrir o arquivo de saída");
        exit(1); 
    }
    for (int i = 0; i < sizeP; i++) {
        fprintf(out, "%s %d %d %d\n",
                processes[i].name, processes[i].tr, processes[i].tf, processes[i].cumpriu);
    }
    fprintf(out, "%d\n", preemp);
    fclose(out);
}

/**
 * Função de comparação usada para ordenar processos por tempo de chegada (`t0`),
 * com desempate por `id`.
 */
int compara_t0(const void *a, const void *b) {
    Process *pa = (Process *)a;
    Process *pb = (Process *)b;

    if (pa->t0 != pb->t0)
        return pa->t0 - pb->t0;

    return pa->id - pb->id;  // desempate pela ordem original (garantindo estabilidade)
}

/**
 * Função de comparação usada para ordenação considerando `t0`, `dt` e `id`,
 * para casos como SRTN onde priorizamos menor tempo de execução.
 */
int compara_dt(const void *a, const void *b) {
    Process *pa = (Process *)a;
    Process *pb = (Process *)b;

    if (pa->t0 != pb->t0)
        return pa->t0 - pb->t0;

    // desempate por tempo de execução (prioriza o menor)
    if (pa->dt != pb->dt)
        return pa->dt - pb->dt;

    // desempate final por ID para garantir estabilidade
    return pa->id - pb->id;
}

/**
 * Calcula a diferença de tempo entre duas `timespec`, em segundos.
 */
double delta_atual(struct timespec *a, struct timespec *b) {
    return (b->tv_sec - a->tv_sec) + (b->tv_nsec - a->tv_nsec) / 1e9;
}

/**
 * Calcula o quantum (quantidade de segundos que o processo pode rodar)
 * com base na "margem" entre o tempo restante até o deadline e o tempo restante
 * de execução ('rest'). Essa função tenta dar prioridade para processos com menor margem.
 */
int get_quantum(Process *p, int t) {

    // pega o tanto que falta para chegar no deadline, subtrai o tanto que falta para terminar de rodar
    // e da a prioridade de acordo com essa "margem" de tempo que ainda pode rodar sem passar o deadline
    int margem = (p->deadline - t) - p->rest;

    // como nao tem mais chances de cumprir o deadline, da o menor quantum (1s)
    if(margem < 0) return 1;
    // a quantidade de quantum vai ser o que falta para rodar, se nao nao vai cumprir o deadline
    else if (margem == 0) return p->rest; 

    // caso ainda tem uma folguinha
    // o quantum vai ser inversamente proporcional à margem
    double quant = (1 / (double)margem) * 10; // * 10 para termos um tempo discreto
    if(quant < 1) return 1;
    if(quant > p->rest) return p->rest;
    
    return (int)quant;
}

/* --------------- escalonadores --------------- */

void *executaFCFS(void *pr) {
    Process *p = (Process *)pr;

    // decide qual cpu usar 
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(p->cpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    
    // a gente pega o tempo que o processo começou a rodar (tempo inicial processo tip) ---------------------------------
    // e o tempo durante o processo td ------------------------------------------------------------------------------------
    struct timespec tip, td; 
    clock_gettime(CLOCK_REALTIME, &tip); 

    double dt = 0;

    // loop que realize qualquer operação que consuma tempo real
    do {
        clock_gettime(CLOCK_REALTIME, &td);
        dt = delta_atual(&tip, &td);
    } while(dt <= p->dt);

    pthread_exit(NULL);
}

void FCFS(char *output) {

    qsort(processes, sizeP, sizeof(Process), compara_t0);

    queue_init(&readyQ);
    clock_gettime(CLOCK_REALTIME, &time_start);

    pthread_t threads[sizeP+1];

    procRunning = malloc(nCPU * sizeof(Process *));
    for (int i = 0; i < nCPU; i++) procRunning[i] = NULL;

    Process *prox = NULL;
    int i = 0;
    int t = 0;
    while(true) {

        while(i < sizeP && processes[i].t0 <= t) {
            pthread_mutex_lock(&mutexQ);
            Process *p = &processes[i];
            enqueue(&readyQ, p);
            i++;
            pthread_mutex_unlock(&mutexQ);
        }

        pthread_mutex_lock(&mutexQ);   
        // se não há processo rodando em alguma CPU, pega o proximo da fila
        for (int k = 0; k < nCPU && !queue_is_empty(&readyQ); k++) {
            Process *curr = procRunning[k];

            if(curr == NULL) { 
                prox = dequeue(&readyQ);
                prox->cpu = k;
                procRunning[k] = prox;
                
                pthread_create(&threads[prox->id], NULL, executaFCFS, prox);
            }
        }
        pthread_mutex_unlock(&mutexQ);

        sleep(1); // espera 1s  
        t++;

        // decrementa o rest e ve se o processo já terminou    
        pthread_mutex_lock(&mutexQ);
        for (int k = 0; k < nCPU; k++) {
            Process *p = procRunning[k];

            if (procRunning[k]) {
                p->rest--;

                if(p->rest == 0) {
                    struct timespec td;
                    clock_gettime(CLOCK_REALTIME, &td);
                    double tf = delta_atual(&time_start, &td);
                    p->tf = (int) tf;
                    p->tr = p->tf - p->t0;
                    p->cumpriu = (p->tf <= p->deadline);

                    procRunning[k] = NULL;
                }      
            }
        }
        pthread_mutex_unlock(&mutexQ);

        // Verificar se todos os processos terminaram
        int finished = 1;
        for (int j = 0; j < sizeP; j++) {
            if (processes[j].rest > 0) {
                finished = 0;
                break;
            }
        }
        
        if(queue_is_empty(&readyQ) && i >= sizeP && finished) break;
    }
        
    for (int i = 0; i < sizeP; i++) {
        pthread_join(threads[i], NULL);
    }

    preemp = 0; // 0 pois não há preempção no FCFS
    write_output(output); 
}

void *executaSRTN(void *pr) {
    
    Process *p = (Process *)pr;

    // decide qual cpu usar 
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(p->cpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    while(1){
		// controla a thread caso haja preempção, ou seja, 
        // controla quando ela deve parar e liberar a CPU para o novo processo e quando pode rodar
		pthread_mutex_lock(&p->mutex);
        while (p->ready == 0) {
            pthread_cond_wait(&p->cond, &p->mutex);
        }
        pthread_mutex_unlock(&p->mutex);

        // tempo que começa e termina o loop para consumir tempo real
        struct timespec s, n; 
        clock_gettime(CLOCK_REALTIME, &s);

        // loop que realize qualquer operação que consuma tempo real
		do {
            clock_gettime(CLOCK_REALTIME, &n);
        } while (delta_atual(&s, &n) <= 1.0);
        
        // se rest chegou a 0 o escalonador já tratou; loop continua apenas se ainda há o que ser executado                            
        pthread_mutex_lock(&p->mutex);
        int fim = (p->rest == 0);
        pthread_mutex_unlock(&p->mutex);
        if (fim) pthread_exit(NULL);
    }

    return NULL;
}

void SRTN(char *output) {

    // ordenar por ordem de chegada, para saber quais processos chegaram antes e quais depois para simular a preempção
    // e também otimiza a cpu para t = 0;
    qsort(processes, sizeP, sizeof(Process), compara_dt);

    clock_gettime(CLOCK_REALTIME, &time_start);
    preemp = 0;

    pthread_t threads[sizeP+1];

    procRunning = malloc(nCPU * sizeof(Process *));
    for (int i = 0; i < nCPU; i++) procRunning[i] = NULL;

    Process *min = NULL;
    int i = 0;
    int t = 0;
    while(true) {
        while(i < sizeP && processes[i].t0 <= t) {
            Process *p = &processes[i];
        
            pthread_mutex_lock(&mutexPQ);
            insert(p);
                
            // se não há processo rodando em alguma CPU, pega o que tem menor execução
            for (int k = 0; k < nCPU && !isEmpty(); k++) {
                Process *curr = procRunning[k];

                if(curr == NULL) { 
                    min = delMin();
                    min->cpu = k;
                    procRunning[k] = min;

                    pthread_mutex_lock(&mutexLOOP);
                    min->ready = 0;
                    pthread_cond_signal(&min->cond);
                    int first = !min->start;
                    if (first) min->start = 1;
                    pthread_mutex_unlock(&mutexLOOP);
                    
                    if (first) pthread_create(&threads[min->id], NULL, executaSRTN, min);
                    break;
                }
            }

            for (int k = 0; k < nCPU; k++) {
                Process *curr = procRunning[k];
                Process *next = peekMin(); // olha o próximo processo sem remover
                
                // se há processo rodando e ocorre preempção com o processo que acabou de chegar
                if (curr && next) {
                    int curr_rest, next_rest;

                    pthread_mutex_lock(&curr->mutex); curr_rest = curr->rest; pthread_mutex_unlock(&curr->mutex);
                    pthread_mutex_lock(&next->mutex); next_rest = next->rest; pthread_mutex_unlock(&next->mutex);

                    if (next_rest < curr_rest) {
                        // preempção - o processo atual tem tempo restante maior que o próximo
                        next = delMin(); 
                        
                        // pausar o processo atual
                        pthread_mutex_lock(&curr->mutex);
                        curr->ready = 0;
                        pthread_mutex_unlock(&curr->mutex);
                        
                        insert(curr); // insere o que estava rodando de novo na PQ e roda o processo que acabou de chegar
                        
                        next->cpu = k;
                        procRunning[k] = next;
                        
                        pthread_mutex_lock(&next->mutex);
                        next->ready = 0;                    
                        int first = !next->start;
                        if (first) next->start = 1;
                        pthread_mutex_unlock(&next->mutex);

                        if (first) pthread_create(&threads[next->id],NULL,executaSRTN,next);
                        
                        preemp++;
                        break;
                    }
                }
            }
            i++;
            pthread_mutex_unlock(&mutexPQ);
        }

        // caso as CPUs não estavam vazias e também não houve preempção, supondo que alguma tenha acabado nesse meio tempo
        
        pthread_mutex_lock(&mutexPQ);
        for (int k = 0; k < nCPU; k++) {
            if (procRunning[k] == NULL && !isEmpty()) {
                
                min = delMin();
                min->cpu = k;
                procRunning[k] = min;

                pthread_mutex_lock(&mutexLOOP);
                min->ready = 0;
                pthread_cond_signal(&min->cond);
                int first = !min->start;
                if (first) min->start = 1;
                pthread_mutex_unlock(&mutexLOOP);
                
                if (first) pthread_create(&threads[min->id], NULL, executaSRTN, min);   
            }
        }
        pthread_mutex_unlock(&mutexPQ);
        
        pthread_mutex_lock(&mutexPQ);
        for (int k=0; k < nCPU; k++) {
            if (procRunning[k]) {
                // libera para rodar a thread
                pthread_mutex_lock(&procRunning[k]->mutex);
                procRunning[k]->ready = 1;
                pthread_cond_signal(&procRunning[k]->cond);
                pthread_mutex_unlock(&procRunning[k]->mutex);
            }
        }
   
        pthread_mutex_unlock(&mutexPQ);  

        sleep(1); // espera 1s  
        t++;
        
        // decrementa o rest e termina quem concluiu a execução          
        pthread_mutex_lock(&mutexPQ);
        for (int k = 0; k < nCPU; k++) {
            Process *p = procRunning[k];

            if (procRunning[k]) {

                pthread_mutex_lock(&p->mutex);
                p->rest--;
                int acabou = (p->rest == 0);
                pthread_mutex_unlock(&p->mutex);  

                if(acabou) {
                    struct timespec td;
                    clock_gettime(CLOCK_REALTIME, &td);
                    double tf = delta_atual(&time_start, &td);
                    p->tf = (int) tf;
                    p->tr = p->tf - p->t0;
                    p->cumpriu = (p->tf <= p->deadline);

                    procRunning[k] = NULL;
                }      
            }
        }

        pthread_mutex_unlock(&mutexPQ);

        // Verificar se todos os processos terminaram
        int finished = 1;
        for (int j = 0; j < sizeP; j++) {
            if (processes[j].rest > 0) {
                finished = 0;
                break;
            }
        }
        
        if(isEmpty() && i >= sizeP && finished) break;
        
    }

    for (int i = 0; i < sizeP; i++) {
        pthread_join(threads[i], NULL);
    }

    write_output(output); 
}

void *executaPS(void *pr) {

    Process *p = (Process *)pr;

    // decide qual cpu usar 
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(p->cpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    while(1){
		// controla a thread caso haja preempção, ou seja, 
        // controla quando ela deve parar e liberar a CPU para o novo processo e quando pode rodar
		pthread_mutex_lock(&p->mutex);
        while (p->ready == 0) {
            pthread_cond_wait(&p->cond, &p->mutex);
        }
        pthread_mutex_unlock(&p->mutex);

        // tempo que começa e termina o loop para consumir tempo real
        struct timespec s, n; 
        clock_gettime(CLOCK_REALTIME, &s);

        // loop que realize qualquer operação que consuma tempo real
		do {
            clock_gettime(CLOCK_REALTIME, &n);
        } while (delta_atual(&s, &n) < p->quantum);
        
        // se rest chegou a 0 o escalonador já tratou; loop continua apenas se ainda há trabalho                             
        if (p->rest == 0) pthread_exit(NULL);
        
    }
    return NULL;
}

void priority_scheduling(char *output) {

    // ordenar por ordem de chegada, para saber quais processos chegaram antes e quais depois 
    qsort(processes, sizeP, sizeof(Process), compara_t0);

    queue_init(&readyQ);
    clock_gettime(CLOCK_REALTIME, &time_start);
    preemp = 0;

    pthread_t threads[sizeP+1];

    procRunning = malloc(nCPU * sizeof(Process *));
    for (int i = 0; i < nCPU; i++) procRunning[i] = NULL;

    Process *prox = NULL;
    int i = 0;
    int t = 0;
    while(true) {

        while(i < sizeP && processes[i].t0 <= t) {
            pthread_mutex_lock(&mutexQ);
            Process *p = &processes[i];
            enqueue(&readyQ, p);
            i++;
            pthread_mutex_unlock(&mutexQ);
        }

        pthread_mutex_lock(&mutexQ);

        // se não há processo rodando em alguma CPU, pega o primeiro da fila
        for (int k = 0; k < nCPU && !queue_is_empty(&readyQ); k++) {
            Process *curr = procRunning[k];

            if(curr == NULL) { 
                prox = dequeue(&readyQ);
                prox->cpu = k;

                int q = get_quantum(prox, t);   
                prox->quantum = q;

                procRunning[k] = prox;
                
                pthread_mutex_lock(&mutexLOOP);
                int first = !prox->start;
                if (first) prox->start = 1;
                pthread_mutex_unlock(&mutexLOOP);
                
                if (first) pthread_create(&threads[prox->id], NULL, executaPS, prox);

            }
        } 
        pthread_mutex_unlock(&mutexQ);

        pthread_mutex_lock(&mutexQ);
        for (int k = 0; k < nCPU; k++) {
            if (procRunning[k]) {
                // libera para rodar a thread
                pthread_mutex_lock(&procRunning[k]->mutex);
                procRunning[k]->ready = 1;
                pthread_cond_signal(&procRunning[k]->cond);
                pthread_mutex_unlock(&procRunning[k]->mutex);
            }
        }
        pthread_mutex_unlock(&mutexQ); 

        sleep(1); // espera 1s  
        t++;
        
        // decrementa o rest, incrementa quanto tempo rodou e checa quem terminou o quantum    
        pthread_mutex_lock(&mutexQ);
        for (int k = 0; k < nCPU; k++) {
            Process *p = procRunning[k];

            if (procRunning[k]) {
                p->rest--;
                p->ran++;

                if(p->rest == 0) {
                    struct timespec td;
                    clock_gettime(CLOCK_REALTIME, &td);
                    double tf = delta_atual(&time_start, &td);
                    p->tf = (int) tf;
                    p->tr = p->tf - p->t0;
                    p->cumpriu = (p->tf <= p->deadline);

                    procRunning[k] = NULL;;
                }      
                else if(p->ran == p->quantum) {
                    // volta para o fim da fila para pegar outro quantum 
                    preemp++;
                    p->ready = 0;
                    p->ran = 0;
                    enqueue(&readyQ, p);
                    procRunning[k] = NULL;
                }
                else continue; 
            }
        }
        pthread_mutex_unlock(&mutexQ);

        // Verificar se todos os processos terminaram
        int finished = 1;
        for (int j = 0; j < sizeP; j++) {
            if (processes[j].rest > 0) {
                finished = 0;
                break;
            }
        }
        
        if(queue_is_empty(&readyQ) && i >= sizeP && finished) break;
        
    }

    for (int i = 0; i < sizeP; i++) {
        pthread_join(threads[i], NULL);
    }

    write_output(output); 
}

int main(int argc, char *argv[]) {
    
    /**
    * Argumentos:
    *   argv[1] = escalonador (1: FCFS, 2: SRTN, 3: Prioridade)
    *   argv[2] = input file
    *   argv[3] = output file
    */
    if (argc != 4) {
        printf("**Entrada inválida**\n");
        exit(1);
    }

    int escalonador = atoi(argv[1]);
    char *trace = argv[2];
    char *output = argv[3];

    ocupCPU = 0;
    nCPU = sysconf(_SC_NPROCESSORS_ONLN);

    pthread_mutex_init(&mutexQ, NULL);
    pthread_mutex_init(&mutexPQ, NULL);
    pthread_mutex_init(&mutexLOOP, NULL);

    read_trace(trace);

    if(escalonador == 1) FCFS(output);
    else if(escalonador == 2) SRTN(output);
    else if(escalonador == 3) priority_scheduling(output);
    else printf("Escalonador inválido!\n");

    pthread_mutex_destroy(&mutexQ);
    pthread_mutex_destroy(&mutexPQ);
    pthread_mutex_destroy(&mutexLOOP);
    
    free(processes);
    free(procRunning);
    
    exit(0);
}