#!/bin/bash

# verifica se tem ao menos 2 argumentos
if [ $# -lt 2 ]; then
    echo "Uso: $0 <N> <TAMANHO_X_MB> [<TAMANHO_Y_MB> ...]"
    exit 1
fi

N=$1
shift # remover o N $1

TAMANHOS=("$@") # arquivos em MB

SRC_DIR="ep4-clientes+servidores"
BIN_DIR="/tmp"

# ================================
# Compilação
# ================================
echo "Compilando ep4-servidor-inet_processos"
gcc "$SRC_DIR/ep4-servidor-inet_processos.c" -o "$BIN_DIR/ep4-servidor-inet_processos"
echo "Compilando ep4-servidor-inet_threads"
gcc "$SRC_DIR/ep4-servidor-inet_threads.c" -o "$BIN_DIR/ep4-servidor-inet_threads" -lpthread
echo "Compilando ep4-servidor-inet_muxes"
gcc "$SRC_DIR/ep4-servidor-inet_muxes.c" -o "$BIN_DIR/ep4-servidor-inet_muxes"
echo "Compilando ep4-servidor-unix_threads"
gcc "$SRC_DIR/ep4-servidor-unix_threads.c" -o "$BIN_DIR/ep4-servidor-unix_threads" -lpthread
echo "Compilando ep4-cliente-inet"
gcc "$SRC_DIR/ep4-cliente-inet.c" -o "$BIN_DIR/ep4-cliente-inet"
echo "Compilando ep4-cliente-unix"
gcc "$SRC_DIR/ep4-cliente-unix.c" -o "$BIN_DIR/ep4-cliente-unix"

# ================================
# Loops
# ================================

# gera arquivos que serao ecoados 
# iniciar servidor
# rodar os N clientes -> esperar terminar
# coletar tempo via journalctl
# finalizar servidor

SERVIDORES=("inet_processos" "inet_threads" "inet_muxes" "unix_threads")
RESULT_FILE="/tmp/ep4-resultados-${N}.data"

for TAM in "${TAMANHOS[@]}"; do
    FMT=$(printf "%02d" "$TAM")
    ARQ="/tmp/arquivo_de_${FMT}MB.txt"
    echo ">>>>>>> Gerando um arquivo texto de: ${TAM}MB..."
    base64 /dev/urandom | head -c $((TAM * 1024 * 1024)) > "$ARQ"
    echo >> "$ARQ"

    # linha para gerar arquivo da forma correta para plotar o gráfico
    LINE_RESULT="$FMT"

    for SERV in "${SERVIDORES[@]}"; do
        echo "Subindo o servidor ep4-servidor-$SERV"
        "$BIN_DIR/ep4-servidor-$SERV" & # executa o servidor em background

        sleep 1

        START=$(date "+%Y-%m-%d %H:%M:%S")

        echo ">>>>>>> Fazendo ${N} clientes ecoarem um arquivo de: ${TAM}MB..."

        for ((i = 0; i < N; i++)); do
            if [[ "$SERV" == "unix_threads" ]]; then
                "$BIN_DIR/ep4-cliente-unix" < "$ARQ" &>/dev/null &
            else
                "$BIN_DIR/ep4-cliente-inet" 127.0.0.1 < "$ARQ" &>/dev/null &
            fi
        done

        echo "Esperando os clientes terminarem..."
        while ps aux | grep "[e]p4-cliente" > /dev/null; do
            sleep 1
        done

        echo "Verificando os instantes de tempo no journald..."
        LOGS=$(journalctl -q --since "$START" | grep "ep4-servidor-$SERV")

        ### confirmar que a quantidade correta de clientes de fato foi servida pelo servidor ###
        COUNT=$(echo "$LOGS" | grep "provavelmente enviou um exit" | wc -l)

        if [[ "$COUNT" -ne "$N" ]]; then
            ps aux | grep "[e]p4-servidor-$SERV" | awk '{print $2}' | xargs -r kill -15
            exit 1
        fi

        ### calcular o instante de tempo em que o primeiro cliente comecou a ser atendidopelo servidor ### 
        ### e o instante de tempo que o ultimo cliente terminou de ser atendido pelo servidor          ###

        START_LINE=$(echo "$LOGS" | grep "Passou pelo accept" | head -n 1)
        END_LINE=$(echo "$LOGS" | grep "provavelmente enviou um exit" | tail -n 1)

        # extrair o timestamp da linha do log (data + hora) de quando servidor iniciou e encerrou        
        START_TIMESTAMP=$(echo "$START_LINE" | awk '{print $1, $2, $3}')
        END_TIMESTAMP=$(echo "$END_LINE" | awk '{print $1, $2, $3}')

        # converte de timestamp de string para formato padrao do ddiff
        START_DDIFF=$(date -d "$START_TIMESTAMP" "+%Y-%m-%d %H:%M:%S")
        END_DDIFF=$(date -d "$END_TIMESTAMP" "+%Y-%m-%d %H:%M:%S")

        DURACAO=$(dateutils.ddiff "$START_DDIFF" "$END_DDIFF" -f "%0M:%0S")

        echo ">>>>>>> ${N} clientes encerraram a conexão"
        echo ">>>>>>> Tempo para servir os ${N} clientes com o ep4-servidor-$SERV: $DURACAO"

        LINE_RESULT="$LINE_RESULT $DURACAO"

        echo "Enviando um sinal 15 para o servidor ep4-servidor-$SERV..."
        ps aux | grep "[e]p4-servidor-$SERV" | awk '{print $2}' | xargs -r kill -15

    done

    echo "$LINE_RESULT" >> "$RESULT_FILE"
done

# ================================
# Geração de gráfico 
# ================================
GPI_FILE="/tmp/ep4-plot-${N}.gpi"
PDF_FILE="ep4-resultados-${N}.pdf"
DATA_FILE="/tmp/ep4-resultados-${N}.data"

# vamos criar um arquivo .gpi com o conteudo abaixo
cat > "$GPI_FILE" <<EOF
set ydata time
set timefmt "%M:%S"
set format y "%M:%S"

set xlabel 'Dados transferidos por cliente (MB)'
set ylabel 'Tempo para atender ${N} clientes concorrentes'

set term pdfcairo
set output "${PDF_FILE}"

set grid
set key top left

plot "${DATA_FILE}" using 1:4 with linespoints title "Sockets da Internet: Mux de E/S", \\
     "${DATA_FILE}" using 1:3 with linespoints title "Sockets da Internet: Threads", \\
     "${DATA_FILE}" using 1:2 with linespoints title "Sockets da Internet: Processos", \\
     "${DATA_FILE}" using 1:5 with linespoints title "Sockets Unix: Threads"
EOF

echo ">>>>>>> Gerando o gráfico de ${N} clientes com arquivos de: $(printf "%sMB " "${TAMANHOS[@]}")"
gnuplot "$GPI_FILE"

# ================================
# Limpeza final
# ================================
rm -f "$GPI_FILE"
rm -f /tmp/arquivo_*MB.txt
rm -f /tmp/ep4-*
rm -f "$RESULT_FILE"

exit 0