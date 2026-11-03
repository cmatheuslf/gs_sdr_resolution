#!/usr/bin/env bash

# --- CONFIGURAÇÕES ---
ROTCTLD_BIN="/usr/bin/rotctld"
ROT_MODEL="204"
ROT_PORT_DEV="/dev/ttyUSB0"
ROT_BAUD="19200"
ROT_HOST="127.0.0.1"
ROT_PORT="4533"

PWD="/home/nascerrgs/GS"
PYTHON_BIN="$PWD/satTracker/gs_env/bin/python"
TRACKER_SCRIPT="$PWD/satTracker/sandbox.py"
LOG_FILE="$PWD/scripts/log/station_startup.log"

PID_ROTCTLD=""
PID_TRACKER=""

init_log() {
    local log_dir
    log_dir=$(dirname "$LOG_FILE")
    [ ! -d "$log_dir" ] && mkdir -p "$log_dir"
    [ ! -f "$LOG_FILE" ] && touch "$LOG_FILE"
}

log_msg() {
    local level="$1"
    local message="$2"
    local timestamp
    timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local formatted="[$timestamp] [$level] $message"
    
    echo "$formatted" >> "$LOG_FILE"
}

cleanup() {
    log_msg "INFO" "Rotina de encerramento chamada."
    
    if [ -n "$PID_TRACKER" ] && kill -0 "$PID_TRACKER" 2>/dev/null; then
        kill -TERM "$PID_TRACKER" 2>/dev/null
        log_msg "INFO" "Tracker finalizado (PID $PID_TRACKER)."
    fi

    if [ -n "$PID_ROTCTLD" ] && kill -0 "$PID_ROTCTLD" 2>/dev/null; then
        kill -TERM "$PID_ROTCTLD" 2>/dev/null
        log_msg "INFO" "rotctld finalizado (PID $PID_ROTCTLD)."
    fi

    wait 2>/dev/null
    log_msg "INFO" "Processos encerrados."
}
trap cleanup SIGTERM SIGINT

# --- EXECUÇÃO DO FLUXO COMPLETO ---
init_log
log_msg "INFO" "=== Iniciando validações e inicialização da estação ==="

# 1. Checagens do ambiente e hardware
CAN_START_ROTCTLD=true
CAN_START_TRACKER=true

if [ -e "$ROT_PORT_DEV" ]; then
    log_msg "INFO" "Dispositivo serial $ROT_PORT_DEV encontrado com sucesso."
else
    log_msg "ERRO" "Dispositivo serial $ROT_PORT_DEV não foi encontrado! Cabo desconectado?"
    CAN_START_ROTCTLD=false
fi

if [ -f "$ROTCTLD_BIN" ]; then
    log_msg "INFO" "Binário rotctld encontrado em: $ROTCTLD_BIN."
else
    log_msg "ERRO" "Binário rotctld não encontrado em: $ROTCTLD_BIN."
    CAN_START_ROTCTLD=false
fi

if [ -f "$PYTHON_BIN" ]; then
    log_msg "INFO" "Interpretador Python venv encontrado em: $PYTHON_BIN."
else
    log_msg "ERRO" "Interpretador Python venv não encontrado em: $PYTHON_BIN."
    CAN_START_TRACKER=false
fi

if [ -f "$TRACKER_SCRIPT" ]; then
    log_msg "INFO" "Script do tracker encontrado em: $TRACKER_SCRIPT."
else
    log_msg "ERRO" "Script do tracker não encontrado em: $TRACKER_SCRIPT."
    CAN_START_TRACKER=false
fi

# 2. Inicialização do rotctld
if [ "$CAN_START_ROTCTLD" = true ]; then
    log_msg "INFO" "Iniciando rotctld (Modelo: $ROT_MODEL, Porta: $ROT_PORT_DEV, Baud: $ROT_BAUD)..."
    $ROTCTLD_BIN -m "$ROT_MODEL" -r "$ROT_PORT_DEV" -s "$ROT_BAUD" -T "$ROT_HOST" -t "$ROT_PORT" &
    PID_ROTCTLD=$!

    # Aguarda a porta TCP responder (timeout de 10s)
    ROT_READY=false
    for i in {1..10}; do
        if nc -z "$ROT_HOST" "$ROT_PORT" 2>/dev/null; then
            ROT_READY=true
            break
        fi
        sleep 1
    done

    if [ "$ROT_READY" = true ]; then
        log_msg "INFO" "rotctld respondendo na porta $ROT_PORT com sucesso (PID $PID_ROTCTLD)."
    else
        log_msg "ERRO" "Timeout: rotctld não respondeu na porta $ROT_PORT dentro de 10 segundos."
    fi
else
    log_msg "AVISO" "Inicialização do rotctld ignorada devido aos erros acima."
fi

# 3. Inicialização do Tracker Python
if [ "$CAN_START_TRACKER" = true ]; then
    log_msg "INFO" "Iniciando script do tracker..."
    $PYTHON_BIN "$TRACKER_SCRIPT" >> $PWD/scripts/log/station_startup.log 2>&1 &
    PID_TRACKER=$!

    sleep 1
    if kill -0 "$PID_TRACKER" 2>/dev/null; then
        log_msg "INFO" "Tracker Python iniciado e ativo (PID $PID_TRACKER)."
    else
        log_msg "ERRO" "O Tracker Python falhou logo após a inicialização! Verifique tracker.log."
    fi
else
    log_msg "AVISO" "Inicialização do Tracker Python ignorada devido aos erros acima."
fi

# 4. Monitoramento dos processos iniciados
log_msg "INFO" "=== Resumo da inicialização concluído ==="

# Aguarda os processos que foram de fato iniciados
if [ -n "$PID_ROTCTLD" ] && [ -n "$PID_TRACKER" ]; then
    wait -n "$PID_ROTCTLD" "$PID_TRACKER"
elif [ -n "$PID_ROTCTLD" ]; then
    wait "$PID_ROTCTLD"
elif [ -n "$PID_TRACKER" ]; then
    wait "$PID_TRACKER"
else
    log_msg "AVISO" "Nenhum processo em segundo plano foi iniciado com sucesso."
fi

cleanup