#pragma once
#include <vector>
#include <complex>
#include <cstdint>
#include "rrc_filter.hpp"

class CostasLoop;
class GardnerClockRecovery;

class BPSK_engine {
private:
    uint32_t freq;
    uint32_t sampleRate;
    uint32_t bitRate;
    
    std::vector<float> rrc_taps; // Coeficientes persistentes do filtro
    CostasLoop* costas;
    GardnerClockRecovery* timing_rec;

public:
    BPSK_engine(uint32_t f, uint32_t sr, uint32_t br);
    ~BPSK_engine();
    std::vector<uint8_t> demodulate(const std::vector<std::complex<float>>& rx_iq);
};