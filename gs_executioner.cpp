#include "SDR_handler.hpp"
#include "BPSK_engine.hpp"
#include "utils.hpp"
#include <iostream>
#include <iomanip>
#include <csignal>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>
#include <fstream>

// Defines
#define RX_FREQ 433920000
#define SAMPLE_RATE 2000000
#define DEMOD_FREQ 50000
#define DEMOD_BITRATE 5000

static SDR_handler* g_receiver = nullptr;
static std::atomic<bool> g_app_running{true};
std::vector<uint8_t> tx_signal = {1, 0, 1, 0, 1, 0, 1, 0}; // Exemplo de carga útil de 8 bits

void signal_handler(int /*signum*/) {
    logging("A encerrar aplicacao...", "INFO");
    g_app_running.store(false);
    if (g_receiver) {
        g_receiver->stop();
    }
}

void bpsk_recv(SDR_handler& sdr, BPSK_engine& bpsk) {
    logging("[BPSK] Thread de recepcao e medicao de BER ativa.", "INFO");

    // Abre o arquivo em modo binário
    std::ofstream output_file("bits_recebidos.bin", std::ios::binary | std::ios::app);
    if (!output_file.is_open()) {
        logging("[BPSK] Erro ao abrir arquivo para salvar bits!", "ERROR");
        return;
    }

    while (g_app_running.load()) {
        if (sdr.is_samples_ready()) {
            std::vector<std::complex<float>> raw_iq = sdr.get_samples();

            if (!raw_iq.empty()) {
                std::vector<uint8_t> bits = bpsk.demodulate(raw_iq);

                if (!bits.empty()) {
                    // Grava o bloco de bits diretamente no arquivo
                    output_file.write(reinterpret_cast<const char*>(bits.data()), bits.size());
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    output_file.close();
    logging("[BPSK] Arquivo de bits fechado e salvo.", "INFO");
}


int main() {
    // 433.92 MHz, 2 MSPS no RX e TX
    SDR_handler sdr(RX_FREQ, SAMPLE_RATE);
    g_receiver = &sdr;

    BPSK_engine bpsk(DEMOD_FREQ, SAMPLE_RATE, DEMOD_BITRATE);

    std::signal(SIGINT, signal_handler);

    if (!sdr.configure()) {
        logging("Falha ao configurar o PlutoSDR.", "ERROR");
        return 1;
    }


    // 4. Inicia as threads
    std::thread sdr_thread(&SDR_handler::run, &sdr);
    // No main():
    std::thread recv_thread(bpsk_recv, std::ref(sdr), std::ref(bpsk));

    if (sdr_thread.joinable()) sdr_thread.join();
    if (recv_thread.joinable()) recv_thread.join();

    return 0;
}