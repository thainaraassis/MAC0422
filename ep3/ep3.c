#include "ep3.h"

/* ---------- Variáveis Globais ---------- */
int nfPos; // ultima posição do next fit
int falhas;

int lePGM(FILE *f, int i) {
    
    long pos = offset + (long)i * 4; // onde começa o pixel que queremos olhar
 
    if (fseek(f, pos, SEEK_SET) != 0) { // colocamos o ponteiro no lugar certo
        perror("lePGM: fseek");
        exit(1);
    }
    
    // 1 ua = 4 bytes
    // vamos ler 3 bytes por vez, já que podemos ignorar o último: " " ou "\n"
    char buf[4] = {0};
    if (fread(buf, 1, 3, f) != 3) {
        fprintf(stderr, "lePGM: falha lendo 3 bytes em i=%d\n", i);
        exit(1);
    }
    
    return atoi(buf);
}

void escrevePGM(FILE *f, int i, int val){

    long pos = offset + (long)i * 4;

    if (fseek(f, pos, SEEK_SET) != 0) {
        perror("escrevePGM: fseek");
        exit(1);
    }

    // escreve os 3 caracteres do número
    char buf[4];
    snprintf(buf, sizeof(buf), "%3d", val);

    if (fwrite(buf, 1, 3, f) != 3) {
        fprintf(stderr, "escrevePGM: falha ao escrever pixel %d\n", i);
        exit(1);
    }

}

int firstFit(FILE *f, int m) {

    int livres = 0; // unidades de alocação livres consecutivas
    int pos = -1; // primeira posição do espaço livre que cabe o processo

    for(int i = 0; i < uaTotal; i++) {
        if(lePGM(f, i) == 0) livres = 0;
        else {
            if(livres == 0) pos = i;
            livres++;
        }

        if(livres == m) return pos;
    }
    return -1;
}

int nextFit(FILE *f, int m) {
    
    int livres = 0; 
    int pos = -1; 

    // varredura circular de total elementos, a partir de "start"
    int i = nfPos; // começa da onte parou

    for (int j = 0; j < uaTotal; j++) {
        if(lePGM(f, i) == 0) livres = 0;
        else {
            if(livres == 0) pos = i;
            livres++;
        }

        if(livres == m) {
            nfPos = (pos + m) % uaTotal; // atualiza nfPos para a próxima posição após este bloco
            return pos;
        }

        i = (i + 1) % uaTotal;
    }
    return -1;
}

int bestFit(FILE *f, int m) {
    
    int livres = 0; 
    int pos = -1; 

    int bestPos = -1;
    int bestTam = INF; // para guardarmos o menor tamanho possível do bloco do bloco que cabe o processo

    for(int i = 0; i < uaTotal; i++) {
        if(lePGM(f, i) == 0) {

            if (livres >= m && livres < bestTam) {
                bestTam = livres;
                bestPos  = pos;
            }

            livres = 0;
        }
        else {
            if(livres == 0) pos = i;
            livres++;
        }

    }
    
    if (livres >= m && livres < bestTam) bestPos = pos;
    return bestPos;
}

int worstFit(FILE *f, int m) {
    
    int livres = 0; 
    int pos = -1; 

    int worstPos = -1;
    int worstTam = -1; // para guardarmos o maior tamanho possível do bloco que cabe o processo

    for(int i = 0; i < uaTotal; i++) {
        if(lePGM(f, i) == 0) {

            if (livres >= m && livres > worstTam) {
                worstTam = livres;
                worstPos  = pos;
            }

            livres = 0;
        }
        else {
            if(livres == 0) pos = i;
            livres++;
        }

    }
    
    if (livres >= m && livres > worstTam) worstPos = pos;
    return worstPos;
}

void compactaMemo(FILE *f) {
     
    int w = 0; // marca a próxima posição que tenho que escrever o processo para compactar a memória

    // percorremos a memoria para encontrar as UA ocupadas e escrevê-las, matendo a ordem relativa, no começo
    for (int r = 0; r < uaTotal; r++) {
        if (lePGM(f, r) == 0) { // se a posição ta ocupada, passamos ela para o mais inicio possível
            if (w < r) escrevePGM(f, w, 0);
            w++;
        }
    }

    for (int i = w; i < uaTotal; i++) escrevePGM(f, i, 255);
    
}

int main(int argc, char *argv[]) {

    if (argc != 5) {
        fprintf(stderr, "uso: ./ep3 n <nome do arquivo PGM de entrada> <nome do arquivo de trace> <nome do arquivo PGM de saida>\n");
        exit(1);
    }

    int n = atoi(argv[1]);
    char *pgm_in  = argv[2];
    char *trace = argv[3];
    char *pgm_out = argv[4];

    // abrindo e criando arquivo de saida
    FILE *fin = fopen(pgm_in, "rb");
    if (!fin) {
        fprintf(stderr, "erro ao abrir PGM de entrada\n");
        exit(1);
    }

    FILE *fout = fopen(pgm_out, "wb");
    if (!fout) {
        fprintf(stderr,"erro ao criar PGM de saída\n");
        fclose(fin);
        exit(1);
    }

    /****************************************************************************/
    /* copiando o arquivo de entrada para um novo de saída para poder trabalhar */

    char buffer[4096];
    size_t b;

    while ((b = fread(buffer, 1, sizeof(buffer), fin)) > 0) {
        fwrite(buffer, 1, b, fout);
    }

    /****************************************************************************/

    fclose(fin);
    fclose(fout);

    fout = fopen(pgm_out, "r+");
    if (!fout) {
        fprintf(stderr,"Erro ao reabrir PGM de saída");
        exit(1);
    }

    FILE *ftrace = fopen(trace, "r");
    if (!ftrace) {
        fprintf(stderr, "erro ao abrir arquivo de trace\n");
        fclose(fout);
        exit(1);
    }

    int l;            
    char op[10];      
    int m;   

    nfPos = 0;   
    falhas = 0;   
    
    while (fscanf(ftrace, "%d %s", &l, op) == 2) {
        if (strcmp(op, "COMPACTAR") == 0) compactaMemo(fout);
        else {
            m = atoi(op);
            int p;

            if(n == 1) p = firstFit(fout, m);
            else if(n == 2) p = nextFit(fout, m);
            else if(n == 3) p = bestFit(fout, m);
            else p = worstFit(fout, m);

            if(p == -1) {
                printf("%d %d\n", l, m);
                falhas++;
            }
            else {
                for (int j = 0; j < m; j++) escrevePGM(fout, p + j, 0);
            }
        }
    }

    printf("%d\n", falhas);

    fclose(ftrace);
    fclose(fout);
    exit(0);
}
