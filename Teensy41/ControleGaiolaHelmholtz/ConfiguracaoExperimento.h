// Parametros recebidos para a varredura experimental em PWM.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#ifndef CONFIGURACAO_EXPERIMENTO_H
#define CONFIGURACAO_EXPERIMENTO_H

#include <Arduino.h>

struct ConfiguracaoExperimento
{
    uint8_t mascaraEixos;

    int16_t pwmInicial;
    int16_t pwmFinal;
    int16_t passoPWM;

    uint32_t tempoPermanencia_ms;

    // Proteções de corrente
    float limiteCorrenteOperacional;
    float limiteCorrenteCritico;

    uint8_t sobreamostragemAD7606;

    uint16_t mediaINA226;
    uint16_t tempoConversaoShuntINA226_us;
    uint16_t tempoConversaoBarramentoINA226_us;
};

#endif
