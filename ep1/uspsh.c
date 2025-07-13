#include "uspsh.h"

/* Variável global para herdar variáveis de ambiente */
extern char **environ;

/**
 * Função show_prompt
 * Retorna uma string alocada dinamicamente com o prompt no formato:
 * [hostname:diretorio_atual]$
 */

char *show_prompt() {
    char hostname[256];
    char directory[256];

    gethostname(hostname, sizeof(hostname));
    getcwd(directory, sizeof(directory));
    
    size_t s = strlen(hostname) + strlen(directory) + 6; // "[ : ]$ \0"
    char *prompt = malloc(s);
    snprintf(prompt, s, "[%s:%s]$ ", hostname, directory);
    return prompt;
}

/**
 * Função read_command
 * Lê uma linha de comando do usuário usando readline,
 * separa o comando (ex: "cd") e os argumentos (args[0], args[1], ...)
 * Retorna o ponteiro da linha original alocada dinamicamente para ser liberada depois.
 */

char *read_command(char **command, char **args, char *prompt) {
    
    char *line = readline(prompt);
    add_history(line);
    *command = strtok(line, " ");

    int i = 0;
    char *aux = strtok(NULL, " ");
    while (aux != NULL) {
        args[i++] = aux;
        aux = strtok(NULL, " ");
    };
    args[i] = NULL;

    return line;
}

/**
 * Função principal 
 */

int main() {
    char *prompt, *command, *args[20], *line;
     
    while(1) {

        prompt = show_prompt();
        line = read_command(&command, args, prompt);
        free(prompt);

        if(command == NULL) {
            free(line);
            continue; 
        }

        // ----------------------------------------
        // Comandos internos
        // ----------------------------------------

        // Comando "cd <diretorio>"
        if (strcmp(command, "cd") == 0 && args[0]) chdir(args[0]); 

        // Comando "whoami"
        else if(strcmp(command, "whoami") == 0) {
            struct passwd *pw = getpwuid(getuid()); 
            printf("%s\n", pw->pw_name);
        }

        // Comando "chmod <permissao> <arquivo>"
        else if(strcmp(command, "chmod") == 0 && args[0] && args[1]) {
            mode_t perm = strtol(args[0], NULL, 8); // converte a string para int
            chmod(args[1], perm); 
        }
        
        // ----------------------------------------
        // Comandos externos permitidos
        // ----------------------------------------

        else {

            // ----------------------------------------
            // Montando o vetor de argumentos incluindo o próprio comando na posição 0
        
            char *argv_local[21]; 
            int i;
            argv_local[0] = command;
            for (i = 0; args[i] != NULL; i++) {
                argv_local[i+1] = args[i];
            }
            argv_local[i+1] = NULL;
            // ----------------------------------------

            int status;
            if(fork() != 0) {
                waitpid(-1, &status, 0); // espera filho terminar de executar
            }
            else {
                //executa o command
                execve(argv_local[0], argv_local, environ);
            }
        }
        
        // Libera a memória da linha lida -> command e args apontam para line
        free(line); 
    } 
    
    exit(0);
}