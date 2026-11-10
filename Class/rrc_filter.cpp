#include "rrc_filter.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace DSP {

float rrc_sample(float t, float alpha) {
    // 1. Limite quando t tende a 0 (evita 0/0)
    if (std::abs(t) < 1e-7f) {
        return 1.0f - alpha + (4.0f * alpha / M_PI);
    }

    // 2. Limite nos polos: |4 * alpha * t| == 1 (evita divisão por zero no denominador)
    float denom_factor = 4.0f * alpha * t;
    if (std::abs(std::abs(denom_factor) - 1.0f) < 1e-5f) {
        float factor = alpha / std::sqrt(2.0f);
        float term_sin = (1.0f + 2.0f / M_PI) * std::sin(M_PI / (4.0f * alpha));
        float term_cos = (1.0f - 2.0f / M_PI) * std::cos(M_PI / (4.0f * alpha));
        return factor * (term_sin + term_cos);
    }

    // 3. Resposta analítica geral do filtro
    float numerator = std::sin(M_PI * t * (1.0f - alpha)) +
                      denom_factor * std::cos(M_PI * t * (1.0f + alpha));
    float denominator = M_PI * t * (1.0f - (denom_factor * denom_factor));

    return numerator / denominator;
}

void normalize_taps(std::vector<float>& taps, float target_gain) {
    float energy = 0.0f;
    for (float h : taps) {
        energy += h * h;
    }

    if (energy > 1e-12f) {
        float scale = target_gain / std::sqrt(energy);
        for (float& h : taps) {
            h *= scale;
        }
    }
}

std::vector<float> rrc(int num_taps, float alpha, float sps, float gain) {
    std::vector<float> taps(num_taps);
    float center = (num_taps - 1) / 2.0f;

    for (int i = 0; i < num_taps; ++i) {
        float t = (static_cast<float>(i) - center) / sps;
        taps[i] = rrc_sample(t, alpha);
    }

    normalize_taps(taps, gain);
    return taps;
}

std::vector<std::complex<float>> apply_fir(
    const std::vector<std::complex<float>>& input,
    const std::vector<float>& taps
) {
    size_t in_len = input.size();
    size_t num_taps = taps.size();
    if (in_len == 0 || num_taps == 0) return {};

    std::vector<std::complex<float>> output(in_len, {0.0f, 0.0f});

    for (size_t n = 0; n < in_len; ++n) {
        std::complex<float> acc(0.0f, 0.0f);
        for (size_t k = 0; k < num_taps; ++k) {
            if (n >= k) {
                acc += input[n - k] * taps[k];
            } else {
                break; // Amostras anteriores a 0 são consideradas nulas
            }
        }
        output[n] = acc;
    }

    return output;
}

} // namespace DSP