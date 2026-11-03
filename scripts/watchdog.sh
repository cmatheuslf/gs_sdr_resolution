#!/usr/bin/env bash

# --- CONFIGURAÇÕES ---
PWD="/home/nascerrgs/GS"
PYTHON_BIN="$PWD/satTracker/gs_env/bin/python"
TRACKER_FILE="$PWD/satTracker/sandbox.py"
TRACKER_PROC_NAME="sandbox.py"
ROT_HOST="127.0.0.1"
ROT_PORT="4533"
LOG_FILE="$PWD/scripts/log/watchdog.log"

init_log_file() {
    local log_dir
    log_dir=$(dirname "$LOG_FILE")
    [ ! -d "$log_dir" ] && mkdir -p "$log_dir"
    [ ! -f "$LOG_FILE" ] && touch "$LOG_FILE"
}

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$LOG_FILE"
}

init_log_file

CAN_START=true

# 1. Checagem do arquivo Python
if [ ! -f "$TRACKER_FILE" ]; then
    log "ERRO: Arquivo do tracker não encontrado em: $TRACKER_FILE"
    CAN_START=false
else
    log "OK: Arquivo do tracker existe."
fi

# 2. Checagem do binário Python
if [ ! -f "$PYTHON_BIN" ]; then
    log "ERRO: Interpretador Python não encontrado em: $PYTHON_BIN"
    CAN_START=false
else
    log "OK: Interpretador Python encontrado."
fi

# 3. Checagem de comunicação com o rotor
check_rotctld() {
    response=$(echo "p" | nc -w 3 "$ROT_HOST" "$ROT_PORT" 2>/dev/null)
    [ -z "$response" ] && return 1
    return 0
}

check_tracker_process() {
    pgrep -f "$TRACKER_PROC_NAME" > /dev/null 2>&1
    return $?
}

if ! check_rotctld; then
    log "ERRO: rotctld em $ROT_HOST:$ROT_PORT não respondeu!"
    CAN_START=false
    # Se o rotor caiu, garante que o processo do tracker seja parado
    if check_tracker_process; then
        log "Finalizando tracker até o rotor restabelecer..."
        pkill -f "$TRACKER_PROC_NAME"
    fi
else
    log "OK: Comunicação com rotctld funcionando."
fi

# 4. Checagem e inicialização do processo
if check_tracker_process; then
    log "OK: Processo $TRACKER_PROC_NAME já está em execução."
else
    log "ALERTA: Processo $TRACKER_PROC_NAME está inativo."
    
    # Só tenta iniciar se todas as checagens anteriores tiverem dado OK
    if [ "$CAN_START" = false ]; then
        nohup "$PYTHON_BIN" "$TRACKER_FILE" >> $PWD/scripts/log/tracker.log 2>&1 &
        log "Processo iniciado com PID $!"
        $PWD/scripts/start_gs.sh
    else
        log "AVISO: O tracker não foi iniciado devido aos erros identificados acima."
    fi
fi