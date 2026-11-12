// Controle direto do campo, com feedforward + PI.
// Corrente INA226 em VM e apenas monitoramento, NUNCA feedback de bobina.
#ifndef CONFIGURACOES_CONTROLE_H
#define CONFIGURACOES_CONTROLE_H
#include <Arduino.h>

namespace ConfiguracoesControle {
    constexpr uint8_t MASCARA_EIXOS = 0b111; // X Y Z

    // true: mantem INA226 como monitor de corrente de entrada do DRV8871.
    // false: permite executar controle de campo sem INA instalado.
    // A calibracao automatica LEGADA ainda precisa de INA e nao deve
    // ser executada se os sensores foram retirados.
    constexpr bool MONITORAR_INA_NO_CONTROLE = true;

    // Ajuste de leitura do monitor de corrente da FONTE. Nao e
    // protecao da corrente real no enrolamento durante slow decay.
    // Faixa de calibracao digital do INA226 (nao e protecao de corrente).
    // Deve abranger o HARD informado no MATLAB e ser compativel com
    // a faixa de tensao do shunt (|Vshunt| <= 81,92 mV).
    // A faixa digital permanece em 1,0 A, como na versao ensaiada.
    // Verificar sua adequacao aos limites HARD e aos shunts reais antes
    // de usar limites de corrente diferentes dos ja validados.
    constexpr float CORRENTE_MAXIMA_INA_A = 1.2f;
    constexpr float RESISTENCIA_SHUNT_INA_X_OHM = 0.02465f;
    constexpr float RESISTENCIA_SHUNT_INA_Y_OHM = 0.02183f;
    constexpr float RESISTENCIA_SHUNT_INA_Z_OHM = 0.02106f;
    constexpr uint16_t MEDIA_INA = 4;
    constexpr uint16_t TEMPO_CONVERSAO_SHUNT_INA_US = 140;
    constexpr uint16_t TEMPO_CONVERSAO_BARRAMENTO_INA_US = 140;

    constexpr uint8_t SOBREAMOSTRAGEM_AD7606 = 64;
    constexpr float SENSIBILIDADE_FLUXGATE_V_POR_UT = 0.04f;

    constexpr uint32_t PERIODO_AQUISICAO_CAMPO_US = 4000; // 250 Hz
    constexpr uint32_t PERIODO_MALHA_CAMPO_US = 8000;      // 125 Hz
    constexpr uint32_t PERIODO_REFERENCIA_US = 10000;
    constexpr uint32_t PERIODO_TELEMETRIA_MANUAL_MS = 50;
    constexpr uint32_t TEMPO_LIMITE_CAMPO_DESATUALIZADO_US = 20000;
    constexpr uint32_t TEMPO_TOLERANCIA_INICIAL_US = 30000;
    constexpr uint16_t VERSAO_FORMATO_REGISTRO_CONTROLE_MANUAL = 1;

    // Ganhos de partida para validacao EM BANCADA (nao ganhos finais).
    // O erro de B e dividido pela estimativa de dB/dPWM da calibracao.
    constexpr float KP_CAMPO[3] = {0.40f, 0.40f, 0.40f};
    constexpr float KI_CAMPO_POR_S[3] = {2.0f, 2.0f, 2.0f};

    // A atuacao do PI continua limitada ao PWM varrido em B(PWM).
    // As REFERENCIAS publicadas (CONTROL_LIMITS) sao ligeiramente
    // menores: erro maximo de regressao por ramo + 1% do PWM util.
    // Isso reserva atuacao corretiva sem alterar os hard/soft limits.
    constexpr float FRACAO_RESERVA_PWM_REFERENCIA = 0.01f;

    // Rampa limitada a 350 contagens por atualizacao do PI.
    constexpr float PASSO_MAX_PWM = 350.0f;
}

static_assert(
    ConfiguracoesControle::FRACAO_RESERVA_PWM_REFERENCIA >= 0.0f &&
    ConfiguracoesControle::FRACAO_RESERVA_PWM_REFERENCIA < 0.10f,
    "Reserva de PWM para referencia deve ser pequena"
);

static_assert(
    ConfiguracoesControle::CORRENTE_MAXIMA_INA_A *
    ConfiguracoesControle::RESISTENCIA_SHUNT_INA_X_OHM < 0.08192f &&
    ConfiguracoesControle::CORRENTE_MAXIMA_INA_A *
    ConfiguracoesControle::RESISTENCIA_SHUNT_INA_Y_OHM < 0.08192f &&
    ConfiguracoesControle::CORRENTE_MAXIMA_INA_A *
    ConfiguracoesControle::RESISTENCIA_SHUNT_INA_Z_OHM < 0.08192f,
    "Faixa INA226 exige shunt menor ou corrente maxima menor"
);

static_assert(
    ConfiguracoesControle::RESISTENCIA_SHUNT_INA_X_OHM > 0.0f &&
    ConfiguracoesControle::RESISTENCIA_SHUNT_INA_Y_OHM > 0.0f &&
    ConfiguracoesControle::RESISTENCIA_SHUNT_INA_Z_OHM > 0.0f,
    "Shunts devem ser positivos");
#endif
