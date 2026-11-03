#pragma once
#include <vector>
#include <complex>
#include <cstdint>

class CostasLoop;
class GardnerClockRecovery;

class BPSK_engine {
private:
    uint32_t freq;
    uint32_t sampleRate;
    uint32_t bitRate;

    CostasLoop* costas;
    GardnerClockRecovery* timing_rec;

public:
    BPSK_engine(uint32_t f, uint32_t sr, uint32_t br);
    ~BPSK_engine();

    // Demodula o fluxo complexo I/Q diretamente para bits
    std::vector<uint8_t> demodulate(const std::vector<std::complex<float>>& rx_iq);
};