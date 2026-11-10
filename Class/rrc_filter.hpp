#pragma once

#include <vector>
#include <complex>

namespace DSP {

/**
 * @brief Calcula a amplitude analítica pontual da resposta ao impulso contínua do RRC.
 * @param t Tempo normalizado pelo período do símbolo (t = instante / sps).
 * @param alpha Fator de decaimento (roll-off, tipicamente entre 0.2 e 0.5).
 * @return Amplitude pontual calculada com tratamento de singularidades.
 */
float rrc_sample(float t, float alpha);

/**
 * @brief Normaliza os coeficientes para que a energia do filtro seja unitária (ou igual ao ganho).
 * @param taps Vetor de coeficientes a ser normalizado no local.
 * @param target_gain Ganho de energia desejado (padrão 1.0f).
 */
void normalize_taps(std::vector<float>& taps, float target_gain = 1.0f);

/**
 * @brief Método central que gera o vetor completo de coeficientes (taps) do filtro RRC.
 * @param num_taps Quantidade total de coeficientes (recomenda-se ímpar para simetria exata).
 * @param alpha Fator de roll-off (ex.: 0.35f).
 * @param sps Amostras por símbolo (Sample Rate / Symbol Rate).
 * @param gain Ganho de saída desejado.
 * @return std::vector<float> Coeficientes simétricos e normalizados.
 */
std::vector<float> rrc(int num_taps, float alpha, float sps, float gain = 1.0f);

/**
 * @brief Aplica a convolução FIR discreta sobre amostras IQ complexas.
 * @param input Sinal de entrada IQ.
 * @param taps Coeficientes reais gerados pelo método rrc.
 * @return std::vector<std::complex<float>> Sinal IQ filtrado.
 */
std::vector<std::complex<float>> apply_fir(
    const std::vector<std::complex<float>>& input,
    const std::vector<float>& taps
);

} // namespace DSP