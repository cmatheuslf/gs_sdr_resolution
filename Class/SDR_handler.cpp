#include "SDR_handler.hpp"
#include <iostream>
#include <cstring>

extern "C" {
    #include <iio.h>
}

SDR_handler::SDR_handler(long long rx_freq, long long sample_rate, long long tx_freq)
    : rx_freq(rx_freq), tx_freq(tx_freq == 0 ? rx_freq : tx_freq), 
      sample_rate(sample_rate), iq_samples(64000, std::complex<float>(0, 0)) {}

SDR_handler::~SDR_handler() {
    stop();

    if (rx_buf) {
        iio_buffer_destroy(rx_buf);
        rx_buf = nullptr;
    }

    if (tx_buf) {
        iio_buffer_destroy(tx_buf);
        tx_buf = nullptr;
    }

    if (ctx) {
        iio_context_destroy(ctx);
        ctx = nullptr;
    }

    logging("Recursos do PlutoSDR liberados com sucesso!", "INFO");
}

bool SDR_handler::configure() {
    logging("Tentando conectar ao PlutoSDR...", "INFO");

    ctx = iio_create_context_from_uri("usb:");
    if (!ctx) {
        ctx = iio_create_network_context("192.168.2.1");
    }

    if (!ctx) {
        logging("Erro: Nao foi possivel conectar ao PlutoSDR.", "ERROR");
        return false;
    }

    logging("Conectado com sucesso: " + std::string(iio_context_get_name(ctx)) + " (" + std::string(iio_context_get_description(ctx)) + ")", "INFO");

    phy = iio_context_find_device(ctx, "ad9361-phy");
    rx_dev = iio_context_find_device(ctx, "cf-ad9361-lpc");
    tx_dev = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");

    if (!phy || !rx_dev || !tx_dev) {
        logging("Erro: Dispositivos internos nao encontrados no contexto.", "ERROR");
        iio_context_destroy(ctx);
        ctx = nullptr;
        return false;
    }

    // Frequencias LO (RX = altvoltage1, TX = altvoltage0)
    struct iio_channel *rx_lo = iio_device_find_channel(phy, "altvoltage1", true);
    if (rx_lo) {
        iio_channel_attr_write_longlong(rx_lo, "frequency", rx_freq);
    }

    struct iio_channel *tx_lo = iio_device_find_channel(phy, "altvoltage0", true);
    if (tx_lo) {
        iio_channel_attr_write_longlong(tx_lo, "frequency", tx_freq);
    }

    // Taxa de amostragem
    struct iio_channel *phy_chn_rx = iio_device_find_channel(phy, "voltage0", false);
    if (phy_chn_rx) {
        iio_channel_attr_write_longlong(phy_chn_rx, "sampling_frequency", sample_rate);
    }

    // Configura atenuacao de TX (0 dB = potencia maxima, -10 dB = moderada para loopback)
    struct iio_channel *tx_channel_phy = iio_device_find_channel(phy, "voltage0", true);
    if (tx_channel_phy) {
        // -10000 mdB = -10 dB (nivel seguro para loopback com atenuador)
        iio_channel_attr_write_longlong(tx_channel_phy, "hardwaregain", -12500);
    }

    // Configura ganho de RX para modo manual com ganho suficiente
    struct iio_channel *rx_channel_phy = iio_device_find_channel(phy, "voltage0", false);
    if (rx_channel_phy) {
        iio_channel_attr_write(rx_channel_phy, "gain_control_mode", "manual");
        iio_channel_attr_write_longlong(rx_channel_phy, "hardwaregain", 13); // 30 dB de ganho
    }
    // Canais RX (I e Q)
    struct iio_channel *rx_i = iio_device_find_channel(rx_dev, "voltage0", false);
    struct iio_channel *rx_q = iio_device_find_channel(rx_dev, "voltage1", false);
    if (!rx_i || !rx_q) {
        std::cerr << "Erro ao localizar canais RX.\n";
        return false;
    }
    iio_channel_enable(rx_i);
    iio_channel_enable(rx_q);

    // Canais TX (I e Q)
    struct iio_channel *tx_i = iio_device_find_channel(tx_dev, "voltage0", true);
    struct iio_channel *tx_q = iio_device_find_channel(tx_dev, "voltage1", true);
    if (!tx_i || !tx_q) {
        std::cerr << "Erro ao localizar canais TX.\n";
        return false;
    }
    iio_channel_enable(tx_i);
    iio_channel_enable(tx_q);

    // Buffer RX
    size_t num_samples = 64000;
    rx_buf = iio_device_create_buffer(rx_dev, num_samples, false);
    if (!rx_buf) {
        std::cerr << "Erro ao criar buffer de recepcao.\n";
        return false;
    }

    return true;
}

bool SDR_handler::transmit(const std::vector<std::complex<float>>& iq_samples, bool cyclic) {
    if (!tx_dev) {
        logging("Dispositivo TX nao configurado.", "ERROR");
        return false;
    }

    if (iq_samples.empty() || (iq_samples.size() % 2 != 0)) {
        logging("Vetor invalido. Deve conter pares I e Q.", "ERROR");
        return false;
    }

    size_t num_complex_samples = iq_samples.size() / 2;

    if (tx_buf) {
        iio_buffer_destroy(tx_buf);
        tx_buf = nullptr;
    }

    tx_buf = iio_device_create_buffer(tx_dev, num_complex_samples, cyclic);
    if (!tx_buf) {
        logging("Erro ao criar buffer TX.", "ERROR");
        return false;
    }

    // Copia o fluxo direto para o buffer DMA
    int16_t *buf_ptr = static_cast<int16_t*>(iio_buffer_start(tx_buf));
    std::memcpy(buf_ptr, iq_samples.data(), iq_samples.size() * sizeof(int16_t));

    ssize_t bytes_enviados = iio_buffer_push(tx_buf);
    if (bytes_enviados < 0) {
        logging("Erro ao enviar dados para o buffer TX (" + std::to_string(bytes_enviados) + ").", "ERROR");
        return false;
    }

    logging("[TX] Amostras enviadas para transmissao (" 
              + std::to_string(num_complex_samples) + " amostras complexas, ciclico: " 
              + std::to_string(cyclic ? 1 : 0) + ").", "INFO");

    return true;
}

void SDR_handler::run() {
    if (!rx_buf) {
        logging("Buffer nao inicializado. Chame configure() antes.", "ERROR");
        return;
    }

    running = true;
    logging("Pronto para leitura continua...", "INFO");
    while (running) {
        ssize_t bytes_lidos = iio_buffer_refill(rx_buf);
        if (bytes_lidos < 0) {
            logging("Erro ao ler dados do buffer.", "ERROR");
            break;
        }

        int16_t *amostras = static_cast<int16_t*>(iio_buffer_start(rx_buf));

        {
            std::lock_guard<std::mutex> lock(mtx);
            for (size_t i = 0; i < 64000; ++i) {
                float i_val = static_cast<float>(amostras[2 * i]) / 32768.0f;
                float q_val = static_cast<float>(amostras[2 * i + 1]) / 32768.0f;
                iq_samples[i] = std::complex<float>(i_val, q_val);
            }
            samples_ready.store(true);
        }
    }
}

void SDR_handler::stop() {
    running = false;
}

std::vector<std::complex<float>> SDR_handler::get_samples() {
    std::lock_guard<std::mutex> lock(mtx);
    samples_ready.store(false);
    return iq_samples;
}

bool SDR_handler::is_samples_ready() const {
    return samples_ready;
}