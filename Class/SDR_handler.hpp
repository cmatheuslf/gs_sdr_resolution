#pragma once

#include <vector>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <string>
#include "utils.hpp"
#include <complex>

struct iio_context;
struct iio_device;
struct iio_channel;
struct iio_buffer;

class SDR_handler {
private:
    long long rx_freq;
    long long tx_freq;
    long long sample_rate;
    std::mutex recv_mtx;


    struct iio_context *ctx = nullptr;
    struct iio_device  *rx_dev = nullptr;
    struct iio_device  *tx_dev = nullptr;
    struct iio_device  *phy = nullptr;

    struct iio_buffer  *rx_buf = nullptr;
    struct iio_buffer  *tx_buf = nullptr;



// Altere o tipo do buffer compartilhado para complexo:
    std::vector<std::complex<float>> iq_samples;
    std::mutex mtx;
    std::atomic<bool> running{false};
    std::atomic<bool> samples_ready{false};

public:
    SDR_handler(long long rx_freq, long long sample_rate, long long tx_freq = 0);
    ~SDR_handler();

    bool configure();
    void run();
    void stop();

    // Metodo generico: apenas transmite amostras brutas intercaladas [I0, Q0, I1, Q1, ...]
    bool transmit(const std::vector<std::complex<float>>& iq_samples, bool cyclic = true);

    std::vector<std::complex<float>> get_samples();
    bool is_samples_ready() const;
};