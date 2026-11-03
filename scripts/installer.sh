# 1. Atualização e instalação de dependências do sistema
PWD="/home/nascerrgs/GS"
sudo apt update -y
sudo apt install libhamlib-utils libhamlib-dev netcat-openbsd -y
sudo usermod -aG dialout $USER

# 2. Criação do ambiente virtual Python e instalação do Skyfield
python3 -m venv $PWD/satTracker/gs_env
source $PWD/satTracker/gs_env/bin/activate
pip install --upgrade pip
pip install -r $PWD/satTracker/requirements.txt
deactivate

# 3. Permissões de execução para os scripts da pasta Documents
chmod +x $PWD/scripts/start_sat.sh 2>/dev/null || true
chmod +x $PWD/scripts/watchdog.sh 2>/dev/null || true

sudo cp $PWD/scripts/services/tracker.service /etc/systemd/system/tracker.service
# 5. Recarregamento, habilitação e inicialização do serviço no systemd
sudo systemctl daemon-reload
sudo systemctl enable tracker.service
sudo systemctl start tracker.service

# 6. Adição do watchdog ao crontab (roda a cada minuto sem duplicar entradas)
CRON_CMD="* * * * * $PWD/scripts/watchdog.sh"
(crontab -l 2>/dev/null | grep -Fv "$CRON_CMD" ; echo "$CRON_CMD") | crontab -