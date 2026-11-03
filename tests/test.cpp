#include "SDR_handler.hpp"
#include "BPSK_engine.hpp"
#include <iostream>
#include <iomanip>
#include <csignal>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <fstream>

static SDR_handler* g_receiver = nullptr;
static std::atomic<bool> g_app_running{true};
std::vector<uint8_t> tx_signal = {1, 0, 1, 0, 1, 0, 1, 0}; // Exemplo de carga útil de 8 bits

void signal_handler(int /*signum*/) {
    std::cout << "\nA encerrar aplicacao...\n";
    g_app_running.store(false);
    if (g_receiver) {
        g_receiver->stop();
    }
}

// Funcao auxiliar para desenhar uma barra horizontal tipo osciloscópio simples
std::string barra_ascii(double normalizado) {
    // Normalizado entre -1.0 e 1.0
    normalizado = std::clamp(normalizado, -1.0, 1.0);
    const int largura_centro = 10;
    int pontos = static_cast<int>(std::round(std::abs(normalizado) * largura_centro));

    std::string barra(2 * largura_centro + 1, ' ');
    barra[largura_centro] = '|'; // Linha central de amplitude zero

    if (normalizado > 0) {
        for (int i = 1; i <= pontos; ++i) barra[largura_centro + i] = '#';
    } else if (normalizado < 0) {
        for (int i = 1; i <= pontos; ++i) barra[largura_centro - i] = '#';
    }
    return barra;
}

void bpsk_worker(SDR_handler& sdr, BPSK_engine& bpsk) {
    std::cout << "[BPSK] Thread de recepcao e medicao de BER ativa.\n";

    std::vector<int16_t> barker_template = bpsk.get_barker_template();

    // Calculo direto aqui no proprio gs_executioner:
    // 2.000.000 amostras/s / 50.000 bits/s = 40 amostras por bit
    const size_t samples_per_bit = 2000000 / 50000; 
    
    const size_t barker_samples  = 7 * samples_per_bit; // 280 amostras
    const size_t payload_samples = 8 * samples_per_bit; // 320 amostras

    const std::vector<uint8_t>& TX_GABARITO = tx_signal;

    uint64_t total_quadros = 0;
    uint64_t quadros_perfeitos = 0;
    uint64_t total_bits = 0;
    uint64_t bits_errados = 0;

    // Antes do loop no bpsk_worker:
    std::ofstream csv_file("metricas_ber.csv");
    if (csv_file.is_open()) {
        csv_file << "quadro,bits_totais,bits_errados,ber_percent,taxa_sucesso_percent\n";
    }


    while (g_app_running.load()) {
        if (sdr.is_samples_ready()) {
            std::vector<int16_t> rx_raw = sdr.get_samples();

            if (!rx_raw.empty()) {
                bool is_inverted = false;
                int sync_idx = bpsk.find_frame_start(rx_raw, barker_template, is_inverted, 500000);

                if (sync_idx >= 0) {
                    size_t payload_start = (static_cast<size_t>(sync_idx) + barker_samples) % rx_raw.size();

                    std::vector<int> payload_signal;
                    payload_signal.reserve(payload_samples);

                    for (size_t i = 0; i < payload_samples; ++i) {
                        size_t rx_idx = (payload_start + i) % rx_raw.size();
                        payload_signal.push_back(static_cast<int>(rx_raw[rx_idx]));
                    }

                    std::vector<int> bits = bpsk.demodulate(payload_signal);

                    if (is_inverted) {
                        for (int &b : bits) b ^= 1;
                    }

                    // Contagem de metricas
                    if (bits.size() == TX_GABARITO.size()) {
                        total_quadros++;
                        int erros_no_quadro = 0;

                        for (size_t i = 0; i < TX_GABARITO.size(); ++i) {
                            total_bits++;
                            if (bits[i] != TX_GABARITO[i]) {
                                bits_errados++;
                                erros_no_quadro++;
                            }
                        }

                        if (erros_no_quadro == 0) {
                            quadros_perfeitos++;
                        }

                        std::cout << ">>> [PACOTE OK @ " << sync_idx << "] Bits: ";
                        for (int b : bits) std::cout << b << " ";
                        std::cout << "\n";

                        // Relatorio a cada 25 quadros
                        if (total_quadros % 25 == 0) {
                            double ber = (static_cast<double>(bits_errados) / total_bits) * 100.0;
                            double taxa_acerto = (static_cast<double>(quadros_perfeitos) / total_quadros) * 100.0;

                            // Dentro do bloco "if (total_quadros % 25 == 0)":
                            if (csv_file.is_open()) {
                                csv_file << total_quadros << "," 
                                        << total_bits << "," 
                                        << bits_errados << "," 
                                        << ber << "," 
                                        << taxa_acerto << "\n";
                                csv_file.flush();
                            }

                            std::cout << "\n==================== METRICAS DE ENLACE ====================\n"
                                      << " Quadros Processados : " << total_quadros << "\n"
                                      << " Quadros Perfeitos   : " << quadros_perfeitos << " (" << taxa_acerto << "%)\n"
                                      << " Bits Totais         : " << total_bits << "\n"
                                      << " Bits com Erro       : " << bits_errados << "\n"
                                      << " BER (Bit Error Rate): " << ber << "%\n"
                                      << "============================================================\n\n";
                        }
                    }
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}
int main() {
    // 433.92 MHz, 2 MSPS no RX e TX
    SDR_handler sdr(433920000, 2000000, 433920000);
    g_receiver = &sdr;

    BPSK_engine bpsk(50000, 2000000, 50000);

    std::signal(SIGINT, signal_handler);

    if (!sdr.configure()) {
        std::cerr << "Falha ao configurar o PlutoSDR.\n";
        return 1;
    }

// 1. Modula com o BPSK_engine
    std::vector<int> sinal_modulado = bpsk.modulate(tx_signal);

    // 2. Converte para pares intercalados [I0, Q0, I1, Q1, ...]
    std::vector<int16_t> tx_iq;
    std::vector<int16_t> tx_i_only;
    tx_iq.reserve(sinal_modulado.size() * 2);

    for (int valor : sinal_modulado) {
        int16_t amostra_i = static_cast<int16_t>(valor);
        tx_iq.push_back(amostra_i); // Canal I
        tx_iq.push_back(0);         // Canal Q

        tx_i_only.push_back(amostra_i);
    }

    // 3. Envia o sinal em modo ciclico
    if (!sdr.transmit(tx_iq, true)) {
        std::cerr << "Falha ao transmitir amostras no PlutoSDR.\n";
        return 1;
    }

    // 4. Inicia as threads
    std::thread sdr_thread(&SDR_handler::run, &sdr);
    // No main():
std::thread worker_thread(bpsk_worker, std::ref(sdr), std::ref(bpsk));

    if (sdr_thread.joinable()) sdr_thread.join();
    if (worker_thread.joinable()) worker_thread.join();

    return 0;
}