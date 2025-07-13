#include "ep2.h"

/* ---------- Variáveis Globais ---------- */
int d, k, debug;
char abord;
int **pista;

pthread_mutex_t mutexPI; // mutex para a pista no modo ingenuo
pthread_mutex_t *mutexPE; // mutex para a pista no modo eficiente -> por coluna
pthread_t *threads;
Ciclista *ciclistas;

int ciclCorrendo;
int simulacao = 0;

int nVoltas;
int minVolta;
int printVoltas;

barrier_t barreira_partida;
barrier_t barreira_chegada;

/* ---------- Barreira ---------- */

void barrier_init(barrier_t *b, int n) {
    b->active  = n;
    b->arrived = 0;
    pthread_mutex_init(&b->m, NULL);
    pthread_cond_init(&b->coord, NULL);
    pthread_cond_init(&b->worker, NULL);
}

void barrier_remove_thread(barrier_t *b) {
    pthread_mutex_lock(&b->m);
    b->active--;
    // se já tiver gente esperando, acorda o coordenador para recalcular
    pthread_cond_signal(&b->coord);
    pthread_mutex_unlock(&b->m);
}

void barrier_wait_worker(barrier_t *b) {
    pthread_mutex_lock(&b->m);
    b->arrived++;
    if (b->arrived == b->active) { // último worker chegou
        pthread_cond_signal(&b->coord);
    }
    // espera o coordenador liberar
    pthread_cond_wait(&b->worker, &b->m);
    pthread_mutex_unlock(&b->m);
}

void barrier_wait_coordinator(barrier_t *b) {
    pthread_mutex_lock(&b->m);
    while (b->arrived < b->active) {
        pthread_cond_wait(&b->coord, &b->m);
    }
    // reset e libera todos, como se fosse o continue dos slides
    b->arrived = 0; 
    pthread_cond_broadcast(&b->worker);
    pthread_mutex_unlock(&b->m);
}

void barrier_destroy(barrier_t *b) {
    pthread_mutex_destroy(&b->m);
    pthread_cond_destroy(&b->coord);
    pthread_cond_destroy(&b->worker);
}

/* ---------- Funcoes Principais ---------- */

void mostraPista() {
    for (int i = 0; i < MAXFAIXA; i++) {
        for (int j = 0; j < d; j++) {
            if (pista[j][i] != -1) printf("%d ", pista[j][i]);
            else printf(". ");
        }
        printf("\n");
    }
}

bool quebrou(Ciclista *c) {
    int r = rand_r(&c->randSeed) % 100;
    if(r < 10) return true;
    return false;
}

double novaVel(Ciclista *c){
    int r = rand_r(&c->randSeed) % 100;
    if(c->vel == 0.5) {
        c->avancar = 0;
        return r < 75 ? 1.0 : 0.5; // 75% → 60 km/h, 25% → 30 km/h
    }
    else return r < 45? 1.0 : 0.5; // 45% → 60 km/h, 55% → 30 km/h

}

void tentaDescer(Ciclista *c) {
    
    int oi = c->i;
    int oj = c->j;

    int nj;
    for(nj = MAXFAIXA-1; nj > oj; nj--) {

        if (pista[oi][nj] == -1) {
            pista[oi][oj] = -1;
            pista[oi][nj] = c->id;
            c->j = nj;
            
            break;
        }
    }
}

void vaiFrente(Ciclista *c, int ni, int nj, int oi, int oj){
    pista[oi][oj] = -1;
    pista[ni][nj] = c->id;
    c->i = ni;
    c->j = nj;
    c->mCorridos += c->vel;
    c->avancar = 0;

    if(oi == 0 && ni == d-1) {

        c->voltas += 1;
        if (c->voltas >= c->capCruzamento) {
            c->capCruzamento *= 2;
            c->tCruzamento = realloc(c->tCruzamento, sizeof(int) * c->capCruzamento);
        }

        c->tCruzamento[c->voltas] = simulacao;

        if(c->voltas > 0 && c->voltas % 5 == 0 && quebrou(c)) {
            c->quebrado = 1;
            c->printaQuebrado = 1;  
        }

        /**calcula nova velocidade**/
        else if(c->voltas > 0) c->vel = novaVel(c);
    }
}

void tentaUltrapassar(Ciclista *c, int ni, int oi, int oj) {
    /* considere que só pode ultrapassar se pistas externas "para cima" do ciclista, estiver livre*/
    // vamos procurar alguma faixa acima dele para ele conseguir ultrapassar

    int nj;
    for(nj = oj - 1; nj >= 0; nj--){
        
        if(pista[c->i][nj] == -1 && pista[ni][nj] == -1) {
            vaiFrente(c, ni, nj, oi, oj);
            return;
        }
    }
}

void *executaCorrida(void *p) {
    Ciclista *c = (Ciclista *)p;

    barrier_wait_worker(&barreira_partida);

    while(c->ativo) {

        int oi = c->i, oj = c->j;

        int ni = (oi - 1 + d) % d;
        int nj = oj;

        if(abord == 'e') {
            pthread_mutex_lock(&mutexPE[oi]);
            pthread_mutex_lock(&mutexPE[ni]);
        }
        else pthread_mutex_lock(&mutexPI);

        // avançou uma volta
        if(c->avancar == 1 || c->vel == 1.0) { // se o de 30km/h já andou 0.5 metros ou se está andando 1m por 60ms (120km/h)

            if(pista[ni][nj] == -1) {
                vaiFrente(c, ni, nj, oi, oj); // mantém sua velocidade normal
            }

            else {
                tentaUltrapassar(c, ni, oi, oj);
            }

        }
        else c->avancar = 1; // o ciclista que está a 60km/h andou 0.5m

        tentaDescer(c); // tenta sempre descer

        if(abord == 'e') {
            pthread_mutex_unlock(&mutexPE[ni]);
            pthread_mutex_unlock(&mutexPE[oi]);
        }
        else pthread_mutex_unlock(&mutexPI);
        
        if(ciclCorrendo <= 1) break;

        barrier_wait_worker(&barreira_chegada);
        barrier_wait_worker(&barreira_partida);
    }


    barrier_remove_thread(&barreira_chegada);
    barrier_remove_thread(&barreira_partida);
    pthread_exit(NULL);
	return NULL;
}

int cmpVoltas(const void *a, const void *b) {
    const Ranking *c1 = (const Ranking *)a;
    const Ranking *c2 = (const Ranking *)b;
    if (c1->voltas != c2->voltas) return c2->voltas - c1->voltas;  // primeiro quem tem mais voltas
    return c1->tempoUltimaVolta - c2->tempoUltimaVolta; // mesmas voltas: quem fez a última volta em menos tempo vem primeiro
}

int verificaVoltaCompletada() {

    Ranking *r;
    r = malloc(k * sizeof(Ranking));

    for(int i = 0; i < k; i++) {
        r[i].id = ciclistas[i].id;
        r[i].voltas = ciclistas[i].voltas;
        r[i].ativo = ciclistas[i].ativo;
        r[i].mCorridos = ciclistas[i].mCorridos;
        r[i].tempoUltimaVolta = ciclistas[i].tCruzamento[ciclistas[i].voltas];
    }

    qsort(r, k, sizeof(Ranking), cmpVoltas); // ordena do que correu mais voltas e mais m para o menos

    if(r[0].voltas > nVoltas) nVoltas = r[0].voltas;

    // calcula a menor volta COMPLETADA por TODOS os ativos
    int v;
    for(v = printVoltas; v <= nVoltas; v++) {

        int rank = 1;
        int completaramV = 0;

        for(int i = 0; i < k; i++) {
            if(ciclistas[i].ativo && ciclistas[i].voltas >= v) completaramV++;
        }
        if(completaramV < ciclCorrendo) break;
        else{
            if(!debug) {

                printf("\n----- %d° VOLTA -----\n", v);
                printf("\nRANKING | CICLISTA\n");

                for(int i = 0; i < k; i++) {
                    if(r[i].voltas >= v) {
                        printf("   %d    |    %d   \n", rank, r[i].id);
                        if(i+1 < k && (r[i].voltas != r[i+1].voltas )) rank++;
                        else if(i+1 < k && (r[i].tempoUltimaVolta != r[i+1].tempoUltimaVolta)) rank++;
                    }
                }
                printf("\n--------------------\n");

            }
            free(r);
            return v+1;
        }
    }
    free(r);
    return v;
}

int eliminaCiclista(){

    // o minVolta, é a primeira volta par que ainda não eliminamos ninguem
    
    int v;
    for(v = minVolta; v <= nVoltas && ciclCorrendo >= 2; v += 2) {

        int completaramV = 0;
        int maxT = -1;
        int eliminado = 0;

        for(int i = 0; i < k; i++) {
            if(ciclistas[i].ativo && ciclistas[i].voltas >= v) {
                completaramV++;
                int t = ciclistas[i].tCruzamento[v];
                if (t > maxT) maxT = t;
            }
        }
        if(completaramV < ciclCorrendo) break;
        else {

            for(int i = 0; i < k; i++) {
                if(ciclistas[i].ativo && ciclistas[i].voltas >= v && ciclistas[i].tCruzamento[v] == maxT) eliminado++;
            }   

            if(eliminado == 0) continue; // quando os dois ciclistas finais quebraram ao mesmo tempo, os eliminados fica 0 e nao pode fazer % por 0

            int r = rand() % eliminado;
            int aux = 0;
            for(int i = 0; i < k; i++) {

                if(ciclistas[i].ativo && ciclistas[i].voltas >= v && ciclistas[i].tCruzamento[v] == maxT) {
                    
                    if(r == aux) {
                        ciclistas[i].ativo = 0;
                        ciclistas[i].posEliminado = ciclCorrendo;
                        ciclCorrendo--;

                        int x = ciclistas[i].i;
                        int y = ciclistas[i].j;
                        pista[x][y] = -1;
                    }
                    aux++;
                } 
            }
        }
    }

    return v;
}

int cmpCiclistas(const void *a, const void *b) {

    const Ciclista *c1 = (const Ciclista *)a;
    const Ciclista *c2 = (const Ciclista *)b;

    return c1->posEliminado - c2->posEliminado;
}

void imprimeRankingFinal() {

    // ordena da menor posição para maior
    qsort(ciclistas, k, sizeof(Ciclista), cmpCiclistas);
    printf("\n---- RANKING FINAL ---- \n");
    printf("\nRANKING | CICLISTA |   TEMPO  | ÚLTIMA VOLTA\n");

    int rank = 1;
    int q = 0; // quebrados
    for(int i = 0; i < k; i++) {
        if(!ciclistas[i].quebrado) {
            printf("   %d    |     %d    |   %d   |      %d     \n", rank, ciclistas[i].id, ciclistas[i].tCruzamento[ciclistas[i].voltas], ciclistas[i].voltas);
            rank++;
        }
        else q++;
    }

    if(q == 0) printf("\n---- NENHUM CICLISTA QUEBROU ---- \n");
    else {
        printf("\n---- CICLISTAS QUEBRADOS ---- \n");
        printf("\nCICLISTA | VOLTA\n");
        for(int i = 0; i < k; i++) {
            if(ciclistas[i].quebrado) printf("   %d    |   %d  \n", ciclistas[i].id, ciclistas[i].voltas);
        }
    }
    
}

void verificaQuebrados(){

    for(int i = 0; i < k; i++) {
        if(ciclistas[i].printaQuebrado) {
            ciclistas[i].ativo = 0;
            ciclistas[i].printaQuebrado = 0;
            ciclCorrendo--;
            pista[ciclistas[i].i][ciclistas[i].j] = -1;
            if(!debug )printf("\n-- Ciclista %d quebrou na sua %d° volta --\n", ciclistas[i].id, ciclistas[i].voltas);
        }
    }
}

void libera() {

    for (int i = 0; i < k; i++) {
        pthread_join(threads[i], NULL);
    }

    barrier_destroy(&barreira_partida);
    barrier_destroy(&barreira_chegada);

    if (abord == 'i') {
        pthread_mutex_destroy(&mutexPI);
    } else {
        for (int i = 0; i < d; i++) pthread_mutex_destroy(&mutexPE[i]);
        free(mutexPE);
    }

    free(threads);

    for (int i = 0; i < k; i++) {
        free(ciclistas[i].tCruzamento);
    }
    free(ciclistas);

    for (int i = 0; i < d; i++) {
        free(pista[i]);
    }
    free(pista);
}

void loop() {

    // mostra como ta a pista inicialmente

    if(debug){
        printf("--------- PISTA ANTES DE COMEÇAR A CORRIDA (0ms) ---------\n");
		mostraPista();
		printf("\n");
	}

    minVolta = 2;
    nVoltas = 0;
    printVoltas = 1;

    barrier_wait_coordinator(&barreira_partida); // como se fosse a largada, libera os ciclistas para avançarem

    while(ciclCorrendo >= 2) {
        // espera que todos os ciclistas completem seu movimento
        barrier_wait_coordinator(&barreira_chegada);

        simulacao += 60;

        if(debug){
            printf("--------- PISTA NO INSTANTE DE TEMPO %dms ---------\n", simulacao);
			mostraPista();
			printf("\n");
		}
        
        verificaQuebrados();

        printVoltas = verificaVoltaCompletada();

        if(nVoltas > 1 && minVolta <= nVoltas && minVolta % 2 == 0 && ciclCorrendo >= 2) {
            minVolta = eliminaCiclista();
        }

        barrier_wait_coordinator(&barreira_partida);
    }

    imprimeRankingFinal();
}

void init(){
    // inicializa a pista
    pista = malloc(d * sizeof(int *));
    for (int i = 0; i < d; i++) {
        pista[i] = malloc(MAXFAIXA * sizeof(int));
        for (int j = 0; j < MAXFAIXA; j++) pista[i][j] = -1; // inicialmente a pista não possui nenhum ciclista
    }

    // caso a abordagem i tenha sido escolhida
    if (abord == 'i') pthread_mutex_init(&mutexPI, NULL);
    else if(abord == 'e') {
        mutexPE = malloc(d * sizeof(pthread_mutex_t));
        for(int i = 0; i < d; i++) pthread_mutex_init(&mutexPE[i], NULL);  
    }

    // inicializa os ciclistas e as barreiras de partida da corrida e chegada
    barrier_init(&barreira_partida, k);
    barrier_init(&barreira_chegada, k);

    threads = malloc(k * sizeof(pthread_t));
    ciclistas = malloc(k * sizeof(Ciclista));

    srand(time(NULL));

    // ordem aleatória de largada
    int *order = malloc(k * sizeof(int));
    for (int i = 0; i < k; ++i) order[i] = i;
    for (int i = k - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
    }

    for(int a = 0; a < k; a++) {
        int i = order[a];
        ciclistas[i].id = i+1;;

        int lin = a/5;
        int col = MAXFAIXA - 1 - (a%5); // 5 pois é o maximo que podemos começar lado a lado
        ciclistas[i].i = lin;
        ciclistas[i].j = col;

        ciclistas[i].id = i+1;
        ciclistas[i].vel = 0.5; // já que simulamos a cada 60ms e inicialmente eles estão a 30km/h -> 0.5m a cada 60ms
        ciclistas[i].voltas = -1;
        ciclistas[i].ativo = 1;
        ciclistas[i].avancar = 0;
        ciclistas[i].mCorridos = 0;
        ciclistas[i].randSeed = rand() + i;
        ciclistas[i].quebrado = 0;
        ciclistas[i].printaQuebrado = 0;

        ciclistas[i].capCruzamento =  8;  // começa pequeno
        ciclistas[i].tCruzamento  = malloc(sizeof(int) * ciclistas[i].capCruzamento);

        ciclistas[i].posEliminado = -1;

        pista[lin][col] = ciclistas[i].id;

        pthread_create(&threads[i], NULL, executaCorrida, &ciclistas[i]);
    }
    free(order);

    ciclCorrendo = k;
}

int main(int argc, char *argv[]) {
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "Uso: ./ep2 d k <i|e> [-debug]\n");
        exit(1);
    }

    d = atoi(argv[1]);
    k = atoi(argv[2]);
    abord = argv[3][0];

    debug = 0;
    if (argc == 5 && (!strcmp(argv[4], "-debug"))) debug = 1;

    init();
    loop();
    libera();

    exit(0);
}