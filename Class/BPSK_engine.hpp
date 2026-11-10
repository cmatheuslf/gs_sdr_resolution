#pragma once
#include <vector>
#include <complex>
#include <cstdint>
#include "rrc_filter.hpp"
#include "DifferentialDecoder.hpp"

class CostasLoop;
class GardnerClockRecovery;
class DifferentialDecoder;

class BPSK_engine {
private:
    uint32_t freq;
    uint32_t sampleRate;
    uint32_t bitRate;
    bool enable_diff_decoding{true}; // Ativado por padrão (conforme GNU Radio)

    std::vector<float> rrc_taps;
    CostasLoop* costas;
    GardnerClockRecovery* timing_rec;
    DifferentialDecoder* diff_dec;

public:
    BPSK_engine(uint32_t f, uint32_t sr, uint32_t br, bool use_diff_dec = true);
    ~BPSK_engine();

    void set_differential_decoding(bool enable) {
        enable_diff_decoding = enable;
    }

    std::vector<uint8_t> demodulate(const std::vector<std::complex<float>>& rx_iq);
};