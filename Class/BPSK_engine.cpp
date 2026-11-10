#include "BPSK_engine.hpp"
#include <cmath>

class CostasLoop {
private:
    float phase{0.0f};
    float freq{0.0f};
    float alpha{0.0f};
    float beta{0.0f};

public:
    CostasLoop(float loop_bw = 0.02f, float damping = 0.707f) {
        float denom = (1.0f + 2.0f * damping * loop_bw + loop_bw * loop_bw);
        alpha = (4.0f * damping * loop_bw) / denom;
        beta  = (4.0f * loop_bw * loop_bw) / denom;
    }

    std::vector<std::complex<float>> process(const std::vector<std::complex<float>>& in_samples) {
        std::vector<std::complex<float>> out_samples;
        out_samples.reserve(in_samples.size());

        for (const auto& sample : in_samples) {
            std::complex<float> nco(std::cos(phase), -std::sin(phase));
            std::complex<float> corrected = sample * nco;
            out_samples.push_back(corrected);

            float error = corrected.real() * corrected.imag();
            freq += beta * error;
            phase += freq + alpha * error;

            while (phase > M_PI)  phase -= 2.0f * M_PI;
            while (phase < -M_PI) phase += 2.0f * M_PI;
        }
        return out_samples;
    }
};

class GardnerClockRecovery {
private:
    float sps;
    float mu{0.0f};
    float omega;
    float gain_omega;
    float gain_mu;
    std::complex<float> last_sample{0.0f, 0.0f};
    std::complex<float> mid_sample{0.0f, 0.0f};

public:
    GardnerClockRecovery(float samples_per_symbol, float loop_bw = 0.01f)
        : sps(samples_per_symbol), omega(samples_per_symbol) {
        gain_mu = loop_bw;
        gain_omega = (loop_bw * loop_bw) / 4.0f;
    }

    std::vector<std::complex<float>> process(const std::vector<std::complex<float>>& in_iq) {
        std::vector<std::complex<float>> symbols_out;
        if (in_iq.size() < 4) return symbols_out;

        size_t idx = 0;
        bool sample_mid = true;

        while (idx + 1 < in_iq.size()) {
            // Interpolação linear tanto para o canal I quanto para o canal Q
            std::complex<float> interp_val = in_iq[idx] + mu * (in_iq[idx + 1] - in_iq[idx]);

            if (sample_mid) {
                mid_sample = interp_val;
                sample_mid = false;
            } else {
                std::complex<float> current_sample = interp_val;
                symbols_out.push_back(current_sample);

                // Detector de erro de Gardner no domínio complexo
                float error_i = mid_sample.real() * (current_sample.real() - last_sample.real());
                float error_q = mid_sample.imag() * (current_sample.imag() - last_sample.imag());
                float error = error_i + error_q;

                // Limitador de erro
                if (error > 1.0f) error = 1.0f;
                if (error < -1.0f) error = -1.0f;

                omega += gain_omega * error;
                mu += gain_mu * error;

                last_sample = current_sample;
                sample_mid = true;
            }

            float step = omega / 2.0f;
            mu += step;

            size_t whole_steps = static_cast<size_t>(std::floor(mu));
            idx += whole_steps;
            mu -= whole_steps;
        }

        return symbols_out;
    }
};

#include "BPSK_engine.hpp"

// Construtor
BPSK_engine::BPSK_engine(uint32_t f, uint32_t sr, uint32_t br, bool use_diff_dec)
    : freq(f), sampleRate(sr), bitRate(br), enable_diff_decoding(use_diff_dec) {
    
    float sps = static_cast<float>(sampleRate) / bitRate;
    rrc_taps = DSP::rrc(45, 0.35f, sps, 1.0f);

    timing_rec = new GardnerClockRecovery(sps, 0.01f);
    costas = new CostasLoop(0.005f); // Banda reduzida para operação a 1 sps
    diff_dec = new DifferentialDecoder();
}

BPSK_engine::~BPSK_engine() {
    delete timing_rec;
    delete costas;
    delete diff_dec;
}

std::vector<uint8_t> BPSK_engine::demodulate(const std::vector<std::complex<float>>& rx_iq) {
    // 1. Filtragem Casada RRC (4 sps)
    std::vector<std::complex<float>> filtered_iq = DSP::apply_fir(rx_iq, rrc_taps);

    // 2. Recuperação de Símbolo (Gardner: 4 sps -> 1 sps)
    std::vector<std::complex<float>> timed_symbols = timing_rec->process(filtered_iq);

    // 3. Recuperação de Portadora (Costas Loop a 1 sps)
    std::vector<std::complex<float>> sync_iq = costas->process(timed_symbols);

    // 4. Decisão de bits brutos (Hard Decision / Constellation Slicer)
    std::vector<uint8_t> raw_bits;
    raw_bits.reserve(sync_iq.size());
    for (const auto& s : sync_iq) {
        raw_bits.push_back(s.real() > 0.0f ? 1 : 0);
    }

    // 5. Decodificação Diferencial opcional
    if (enable_diff_decoding) {
        return diff_dec->process(raw_bits);
    }

    return raw_bits;
}

BPSK_engine::~BPSK_engine() {
    delete costas;
    delete timing_rec;
    delete diff_dec;
}