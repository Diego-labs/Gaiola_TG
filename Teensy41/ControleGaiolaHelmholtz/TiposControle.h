// Interface e registros historicos mantidos compativeis com MATLAB.
#ifndef TIPOS_CONTROLE_H
#define TIPOS_CONTROLE_H
#include <Arduino.h>

struct ReferenciaCampo { float B_uT[3]; };

struct EstadoControleEixo {
    float referenciaCampo_uT = 0.0f;
    float campoMedido_uT = 0.0f;
    float erroCampo_uT = 0.0f;

    // IREF nao existe no modo direto. Permanece no registro por compatibilidade
    // e recebe NAN. I medida e a CORRENTE DA FONTE, nao da bobina.
    float referenciaCorrente_A = NAN;
    float correnteMedida_A = NAN;
    float pwmAntecipado = 0.0f;
    float integralCampo_PWM = 0.0f;
    int16_t comandoPWM = 0;

    uint32_t instanteCorrente_us = 0;
    uint32_t instanteCampo_us = 0;
    bool correnteValida = false;
    bool campoValido = false;
};

// Formato binario historico (60 bytes), NAO modificar sem migrar MATLAB.
struct RegistroControleManual {
    uint32_t tempoDecorrido_ms;
    float referenciaCampo_uT[3];
    float campoMedido_uT[3];
    float referenciaCorrente_A[3];
    float correnteMedida_A[3];
    int16_t pwm[3];
    uint16_t indicadores;
};
static_assert(sizeof(RegistroControleManual) == 60,
              "RegistroControleManual deve ter 60 bytes");
#endif
