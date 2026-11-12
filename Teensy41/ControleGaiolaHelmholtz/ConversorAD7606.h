// Interface de aquisicao do AD7606 via SPI.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#ifndef CONVERSOR_AD7606_H
#define CONVERSOR_AD7606_H

#include <Arduino.h>
#include <SPI.h>

// ESTRUTURA DE UMA AMOSTRA
// CH1 → raw[0] / voltage[0]
// CH2 → raw[1] / voltage[1]
// CH3 → raw[2] / voltage[2]
// timestamp:
// instante associado à aquisição

struct AmostraAD7606
{
    int16_t valorBruto[3];

    float tensao[3];

    uint32_t instante;
};

// CLASSE AD7606

class ConversorAD7606
{
private:

    // PINOS CONTROLADOS DIRETAMENTE

    uint8_t pinoCS;
    uint8_t pinoBUSY;
    uint8_t pinoRESET;
    uint8_t pinoCONVST;

    uint8_t pinoOS0;
    uint8_t pinoOS1;
    uint8_t pinoOS2;

    // CONFIGURAÇÃO SPI

    uint32_t frequenciaSPI;

    // CONFIGURAÇÃO DO ADC

    // Para faixa ±10 V:
    // V = ADC × 10 / 32768

    float faixaADC;

    // TIMEOUT

    // Tempo máximo para esperar BUSY mudar de estado.
    // 10000 us = 10 ms

    uint32_t tempoLimiteBusy;

    // FUNÇÕES INTERNAS

    void reiniciar();

    void iniciarConversao();

    bool aguardarBusyAlto();

    bool aguardarBusyBaixo();

    int16_t lerCanal();

public:

    // CONSTRUTOR

    ConversorAD7606(
        uint8_t pinoCs,
        uint8_t pinoBusy,
        uint8_t pinoReset,
        uint8_t pinoConvst,
        uint8_t pinoOs0,
        uint8_t pinoOs1,
        uint8_t pinoOs2
    );

    // INICIALIZAÇÃO

    bool iniciar();

    // SOBREAMOSTRAGEM
// 0 → 1×
// 1 → 2×
// 2 → 4×
// 3 → 8×
// 4 → 16×
// 5 → 32×
// 6 → 64×

    bool definirSobreamostragem(uint8_t sobreamostragem);

    // AQUISIÇÃO

    bool lerAmostra(AmostraAD7606 &amostra);

    // CONVERSÃO ADC → VOLTS

    float converterBrutoParaTensao(int16_t valorBruto) const;
};

#endif
