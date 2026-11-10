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
    float last_sample{0.0f};
    float mid_sample{0.0f};

public:
    GardnerClockRecovery(float samples_per_symbol, float loop_bw = 0.01f)
        : sps(samples_per_symbol), omega(samples_per_symbol) {
        gain_mu = loop_bw;
        gain_omega = (loop_bw * loop_bw) / 4.0f;
    }

    std::vector<float> process(const std::vector<float>& in_i) {
        std::vector<float> symbols_out;
        if (in_i.size() < 4) return symbols_out;

        size_t idx = 0;
        bool sample_mid = true;

        while (idx + 1 < in_i.size()) {
            float interp_val = in_i[idx] + mu * (in_i[idx + 1] - in_i[idx]);

            if (sample_mid) {
                mid_sample = interp_val;
                sample_mid = false;
            } else {
                float current_sample = interp_val;
                symbols_out.push_back(current_sample);

                float error = mid_sample * (current_sample - last_sample);
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

BPSK_engine::BPSK_engine(uint32_t f, uint32_t sr, uint32_t br)
    : freq(f), sampleRate(sr), bitRate(br) {
    
    // Cálculo de amostras por símbolo: 50.000 / 12.500 = 4.0
    float sps = static_cast<float>(sampleRate) / bitRate;
    
    // Geração dos coeficientes: 45 taps, alpha = 0.35, sps = 4.0, ganho = 1.0
    int num_taps = 45;
    float alpha = 0.35f;
    rrc_taps = DSP::rrc(num_taps, alpha, sps, 1.0f);

    costas = new CostasLoop(0.02f);
    timing_rec = new GardnerClockRecovery(sps, 0.01f);
}

BPSK_engine::~BPSK_engine() {
    delete costas;
    delete timing_rec;
}

std::vector<uint8_t> BPSK_engine::demodulate(const std::vector<std::complex<float>>& rx_iq) {
    // 1. Filtragem Casada RRC (Limpeza de ruído e contenção de banda)
    std::vector<std::complex<float>> filtered_iq = DSP::apply_fir(rx_iq, rrc_taps);

    // 2. Recuperação de portadora (Costas Loop) sobre o sinal filtrado
    std::vector<std::complex<float>> sync_iq = costas->process(filtered_iq);

    // 3. Extração da componente em fase (I)
    std::vector<float> i_channel;
    i_channel.reserve(sync_iq.size());
    for (const auto& s : sync_iq) {
        i_channel.push_back(s.real());
    }

    // 4. Recuperação de temporização (Gardner)
    std::vector<float> symbols = timing_rec->process(i_channel);

    // 5. Decisão de bits
    std::vector<uint8_t> bits;
    bits.reserve(symbols.size());
    for (float s : symbols) {
        bits.push_back(s > 0.0f ? 1 : 0);
    }

    return bits;
}

BPSK_engine::~BPSK_engine() {
    delete costas;
    delete timing_rec;
}