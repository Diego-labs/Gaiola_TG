// Controlador direto de campo (feedforward B(PWM) + PI por eixo).
// Os INA226 fornecem monitoramento e limites, nunca PI de corrente.
#ifndef CONTROLADOR_CAMPO_H
#define CONTROLADOR_CAMPO_H
#include <Arduino.h>
#include <math.h>
#include "TiposCalibracao.h"
#include "ConfiguracoesControle.h"
#include "TiposControle.h"

class ControladorCampo {
public:
    ControladorCampo();
    void definirMascaraEixos(uint8_t mascara);
    void carregarCalibracao(const ParametrosCalibracao &parametros);
    bool estaPronto() const;
    void reiniciarDinamica();
    void definirReferenciaCampo(const ReferenciaCampo &referencia);
    void definirMedicaoCorrente(uint8_t eixo, float correnteFonte_A,
                               uint32_t instante_us);
    void definirMedicaoCampo(const float B_uT[3], uint32_t instante_us);
    bool atualizarMalhaCampo(uint32_t agora_us);
    int16_t obterPWM(uint8_t eixo) const;
    float obterLimiteOperacional(uint8_t eixo) const;
    float obterLimiteCritico(uint8_t eixo) const;
    bool obterLimitesOperacionaisCampo(uint8_t eixo, float &minimo_uT,
                                      float &maximo_uT) const;
    bool eixoEstaHabilitado(uint8_t eixo) const;
    const EstadoControleEixo &obterEstado(uint8_t eixo) const;
private:
    uint8_t mascaraEixos;
    ParametrosCalibracao calibracao;
    bool calibracaoCarregada;
    EstadoControleEixo estado[3];
    uint32_t ultimaAtualizacao_us;
    bool calibracaoValidaParaEixo(uint8_t eixo) const;
    bool campoEstaAtualizado(uint8_t eixo, uint32_t agora_us) const;
    float coefCampoPorPwm(uint8_t eixo, bool sentidoPositivo) const;
    static float limitar(float valor, float minimo, float maximo);
};
#endif
