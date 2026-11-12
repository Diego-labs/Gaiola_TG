// Parametros da rotina automatica de calibracao.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#ifndef CONFIGURACOES_CALIBRACAO_H
#define CONFIGURACOES_CALIBRACAO_H

#include <Arduino.h>
#include "ConfiguracoesControle.h"

namespace ConfiguracoesCalibracao
{
    // PWM

    // Permite que a busca de calibracao percorra toda a faixa PWM de 12 bits.
    // ATENCAO: esta alteracao habilita comandos maiores que 1024.
    // Somente executar apos conferir: faixa/calibracao INA226,
    // corrente REAL dos enrolamentos, limitacao ILIM do DRV8871 e
    // dissipacao termica. INA226 em VM nao mede a corrente do enrolamento.
    // A busca continua incremental (64 e 8 contagens), com parada
    // pelo limite critico fornecido na interface e uso do soft limit.
    constexpr int16_t PWM_MAXIMO = 4095;

    // Busca do limite associado ao soft limit
    constexpr int16_t PASSO_BUSCA_GROSSA = 64;
    constexpr int16_t PASSO_BUSCA_FINA = 8;

    // A busca passa para o passo fino a partir de 85% do soft limit.
    constexpr float FRACAO_INICIO_BUSCA_FINA = 0.85f;

    // A varredura de calibração utiliza como alvo operacional
    // aproximadamente 95% do soft limit.
    constexpr float FRACAO_MARGEM_LIMITE_OPERACIONAL = 0.05f;

    // PONTOS DE CALIBRAÇÃO

    // 100 negativos + zero + 100 positivos.
    constexpr uint16_t PONTOS_POR_LADO = 100;
    constexpr uint16_t TOTAL_PONTOS = 2 * PONTOS_POR_LADO + 1;

    // TEMPORIZAÇÃO / CONFIRMAÇÃO

    // Tempo inicial de acomodação após cada alteração do PWM.
    constexpr uint32_t TEMPO_ACOMODACAO_MS = 50;

    // Número de conversões NOVAS do INA226 utilizadas para confirmar
    // cada ponto. O AD7606 é lido uma vez para cada uma dessas amostras.
    constexpr uint8_t AMOSTRAS_CONFIRMACAO = 5;

    // Tempo limite para aguardar uma nova conversão do INA226.
    constexpr uint32_t TEMPO_LIMITE_AMOSTRA_INA_MS = 250;

    // Tempo máximo para a corrente retornar à região de zero antes
    // de inverter o sentido ou iniciar uma nova etapa.
    constexpr uint32_t TEMPO_LIMITE_RETORNO_ZERO_MS = 3000;

    // Corrente considerada zero = esta fração do soft limit.
    constexpr float FRACAO_TOLERANCIA_ZERO = 0.01f;

    // FLUXGATE / AD7606

    // Sensibilidade nominal utilizada atualmente no tratamento:
    // B [uT] = V / 0.04.
    constexpr float SENSIBILIDADE_FLUXGATE_V_POR_UT =
        ConfiguracoesControle::SENSIBILIDADE_FLUXGATE_V_POR_UT;

    // INA226 conectado antes do DRV8871, em VM: a corrente lida e
    // a da fonte. Nao inferir sinal de corrente de bobina pelo PWM.

    // CONFIGURAÇÕES FIXAS DOS SENSORES
    // Nesta versão, estes parâmetros NÃO são enviados pelo MATLAB.
    // Eles ficam centralizados aqui para serem alterados diretamente
    // no firmware quando necessário.
    // Os valores abaixo vêm de ConfiguracoesControle.h. O shunt é
    // individual por eixo, pois pequenas diferenças entre resistores
    // alteram diretamente a corrente calculada pelo INA226.

    constexpr float CORRENTE_MAXIMA_INA_A =
        ConfiguracoesControle::CORRENTE_MAXIMA_INA_A;

    constexpr float RESISTENCIA_SHUNT_INA_X_OHM =
        ConfiguracoesControle::RESISTENCIA_SHUNT_INA_X_OHM;
    constexpr float RESISTENCIA_SHUNT_INA_Y_OHM =
        ConfiguracoesControle::RESISTENCIA_SHUNT_INA_Y_OHM;
    constexpr float RESISTENCIA_SHUNT_INA_Z_OHM =
        ConfiguracoesControle::RESISTENCIA_SHUNT_INA_Z_OHM;

    constexpr uint16_t MEDIA_INA =
        ConfiguracoesControle::MEDIA_INA;
    constexpr uint16_t TEMPO_CONVERSAO_SHUNT_INA_US =
        ConfiguracoesControle::TEMPO_CONVERSAO_SHUNT_INA_US;
    constexpr uint16_t TEMPO_CONVERSAO_BARRAMENTO_INA_US =
        ConfiguracoesControle::TEMPO_CONVERSAO_BARRAMENTO_INA_US;

    constexpr uint8_t SOBREAMOSTRAGEM_AD7606 =
        ConfiguracoesControle::SOBREAMOSTRAGEM_AD7606;
}

static_assert(
    ConfiguracoesCalibracao::PWM_MAXIMO > 0 &&
    ConfiguracoesCalibracao::PWM_MAXIMO <= 4095,
    "O teto de PWM deve respeitar os 12 bits do acionador"
);

static_assert(
    ConfiguracoesCalibracao::TOTAL_PONTOS == 201,
    "A calibracao inicial deve possuir 201 pontos por eixo"
);

#endif
