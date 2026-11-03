#include <iostream>
#include <vector>
#include <cmath>
#include <cstdint>

// Parâmetros de modulação
uint32_t freq = 10; // Frequência da portadora em Hz
uint32_t sampleRate = 20; // Taxa de amostragem em Hz 
uint32_t bitRate = 2; // Taxa de bits em bps

#define PI 3.14159265358979323846
std::vector<int> modulation(){
    // Quantidade de amostras que cada bit "dura" no tempo
    size_t samplesPerBit = static_cast<size_t>(sampleRate / bitRate); // 8000 / 250 = 32 amostras por bit
    // Conversão de modulação BPSK
    std::vector<uint8_t> data = {1, 0, 1, 1, 0, 0, 1, 0}; // Example binary data
    std::vector<double> modulatedSignal;
    for (size_t i = 0; i < data.size(); ++i) {
        double signalValue = 2 * data[i] - 1; // BPSK modulation
        modulatedSignal.push_back(signalValue);
    }

    // Extensão do sinal modulado
    size_t t = data.size() * samplesPerBit; // 8 bits * 32 amostras = 256 amostras no total
    std::vector<double> extendedSignal; 
    for(size_t i = 0; i < modulatedSignal.size(); ++i) {
        for (int j = 0; j < samplesPerBit; ++j) {
            extendedSignal.push_back(modulatedSignal[i]);
        }
    }

    // Portadora
    std::vector<double> carrierSignal;
    for(size_t i = 0; i < t; ++i) {
        double carrierValue = cos(2 * PI * freq * (i / static_cast<double>(sampleRate)));
        carrierSignal.push_back(carrierValue);
    }

    // Convolução do sinal modulado com a portadora
    std::vector<int> modulatedCarrierSignal;
    for(size_t i = 0; i < extendedSignal.size(); ++i) {
        modulatedCarrierSignal.push_back(extendedSignal[i] * carrierSignal[i]);
    }

    std::cout << "Sinal modulado com portadora (BPSK):" << std::endl;
    for (size_t i = 0; i < modulatedCarrierSignal.size(); ++i) {
        std::cout << modulatedCarrierSignal[i] << " ";
    }
    std::cout << std::endl;



    return modulatedCarrierSignal;
}

std::vector<int> demodulateBPSK(const std::vector<int>& modulatedSignal) {
    
    size_t samplesPerBit = static_cast<size_t>(sampleRate / bitRate); // 8000 / 250 = 32 amostras por bit

    // Extensão do sinal modulado
    size_t t = modulatedSignal.size() * samplesPerBit;
    // Portadora
    std::vector<double> carrierSignal;
    for(size_t i = 0; i < t; ++i) {
        double carrierValue = cos(2 * PI * freq * (i / static_cast<double>(sampleRate)));
        carrierSignal.push_back(carrierValue);
    }

    std::vector<int> demodulatedSignal;
    for(size_t i = 0; i < modulatedSignal.size(); ++i){
        uint8_t bitValue = (modulatedSignal[i] * carrierSignal[i]) > 0 ? 1 : 0;
        demodulatedSignal.push_back(bitValue);
    }
    std::cout << "Sinal demodulado (BPSK):" << std::endl;
    for (size_t i = 0; i < demodulatedSignal.size(); ++i) {
        std::cout << demodulatedSignal[i] << " ";
    }
    std::cout << std::endl;

    return demodulatedSignal;
}

int main() {
    std::vector<int> modulatedSignal = modulation();
    std::vector<int> demodulatedSignal = demodulateBPSK(modulatedSignal);
    return 0;
}