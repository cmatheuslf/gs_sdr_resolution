#pragma once
#include <vector>
#include <cstdint>

class DifferentialDecoder {
private:
    uint8_t last_bit{0};

public:
    DifferentialDecoder() = default;

    void reset() {
        last_bit = 0;
    }

    std::vector<uint8_t> process(const std::vector<uint8_t>& in_bits) {
        std::vector<uint8_t> out_bits;
        out_bits.reserve(in_bits.size());

        for (uint8_t bit : in_bits) {
            // Operação XOR: bit_out = (bit - last_bit) mod 2
            uint8_t decoded = bit ^ last_bit;
            out_bits.push_back(decoded);
            last_bit = bit;
        }

        return out_bits;
    }
};