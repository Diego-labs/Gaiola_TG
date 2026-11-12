#include "ControladorCampo.h"
#include <string.h>

ControladorCampo::ControladorCampo() :
    mascaraEixos(ConfiguracoesControle::MASCARA_EIXOS),
    calibracaoCarregada(false), ultimaAtualizacao_us(0) {
    memset(&calibracao, 0, sizeof(calibracao));
    reiniciarDinamica();
}
void ControladorCampo::definirMascaraEixos(uint8_t mascara) {
    mascaraEixos = mascara & 0b111;
}
void ControladorCampo::carregarCalibracao(const ParametrosCalibracao &p) {
    calibracao = p;
    calibracaoCarregada = (p.assinatura == ASSINATURA_PARAMETROS_CALIBRACAO &&
                          p.versao == VERSAO_PARAMETROS_CALIBRACAO);
}
bool ControladorCampo::eixoEstaHabilitado(uint8_t eixo) const {
    return eixo < 3 && ((mascaraEixos & bitEixoCalibracao(eixo)) != 0);
}
float ControladorCampo::limitar(float valor, float minimo, float maximo) {
    return valor < minimo ? minimo : (valor > maximo ? maximo : valor);
}
float ControladorCampo::coefCampoPorPwm(uint8_t eixo,
                                         bool sentidoPositivo) const {
    const ParametrosCalibracaoEixo &p = calibracao.eixo[eixo];
    // Coeficientes experimentais da calibracao direta B(PWM), sem corrente.
    return sentidoPositivo ? p.coefAngularCampoPwmPositivo
                          : p.coefAngularCampoPwmNegativo;
}
bool ControladorCampo::calibracaoValidaParaEixo(uint8_t eixo) const {
    if (!calibracaoCarregada || eixo >= 3 ||
        !(calibracao.mascaraEixosValidos & bitEixoCalibracao(eixo)))
        return false;
    const ParametrosCalibracaoEixo &p = calibracao.eixo[eixo];
    if (!p.valido || !isfinite(p.campoEmZero_uT) ||
        !isfinite(p.limiteOperacional_A) || !isfinite(p.limiteCritico_A) ||
        p.limiteOperacional_A <= 0 || p.limiteCritico_A <= p.limiteOperacional_A ||
        p.pwmOperacionalNegativo >= 0 || p.pwmOperacionalNegativo < -4095 ||
        p.pwmOperacionalPositivo <= 0 || p.pwmOperacionalPositivo > 4095 ||
        p.pwmCalibracaoNegativo >= 0 || p.pwmCalibracaoNegativo < p.pwmOperacionalNegativo ||
        p.pwmCalibracaoPositivo <= 0 || p.pwmCalibracaoPositivo > p.pwmOperacionalPositivo)
        return false;
    const float kPos = coefCampoPorPwm(eixo, true);
    const float kNeg = coefCampoPorPwm(eixo, false);
    return isfinite(kPos) && isfinite(kNeg) && kPos > 1e-8f && kNeg > 1e-8f;
}
bool ControladorCampo::estaPronto() const {
    if (mascaraEixos == 0) return false;
    for (uint8_t eixo=0; eixo<3; ++eixo)
        if (eixoEstaHabilitado(eixo) && !calibracaoValidaParaEixo(eixo))
            return false;
    return true;
}
void ControladorCampo::reiniciarDinamica() {
    for (uint8_t eixo=0; eixo<3; ++eixo) {
        estado[eixo] = EstadoControleEixo{};
        estado[eixo].referenciaCorrente_A = NAN;
        estado[eixo].correnteMedida_A = NAN;
    }
    ultimaAtualizacao_us = 0;
}
void ControladorCampo::definirReferenciaCampo(const ReferenciaCampo &ref) {
    for (uint8_t eixo=0; eixo<3; ++eixo)
        estado[eixo].referenciaCampo_uT = ref.B_uT[eixo];
}
void ControladorCampo::definirMedicaoCorrente(uint8_t eixo,
                                                float correnteFonte_A,
                                                uint32_t instante_us) {
    if (eixo >= 3 || !isfinite(correnteFonte_A)) return;
    estado[eixo].correnteMedida_A = correnteFonte_A;
    estado[eixo].instanteCorrente_us = instante_us;
    estado[eixo].correnteValida = true;
}
void ControladorCampo::definirMedicaoCampo(const float B_uT[3],
                                            uint32_t instante_us) {
    for (uint8_t eixo=0; eixo<3; ++eixo) {
        if (!isfinite(B_uT[eixo])) {
            estado[eixo].campoValido = false;
            continue;
        }
        estado[eixo].campoMedido_uT = B_uT[eixo];
        estado[eixo].instanteCampo_us = instante_us;
        estado[eixo].campoValido = true;
    }
}
bool ControladorCampo::campoEstaAtualizado(uint8_t eixo,
                                            uint32_t agora_us) const {
    return estado[eixo].campoValido &&
           (uint32_t)(agora_us - estado[eixo].instanteCampo_us)
             <= ConfiguracoesControle::TEMPO_LIMITE_CAMPO_DESATUALIZADO_US;
}
bool ControladorCampo::atualizarMalhaCampo(uint32_t agora_us) {
    if (!estaPronto()) return false;
    // Todos os eixos habilitados devem possuir amostra atual, antes de
    // modificar qualquer comando PWM (evita atualizacao parcial).
    for (uint8_t eixo=0; eixo<3; ++eixo) {
        if (eixoEstaHabilitado(eixo) &&
            (!campoEstaAtualizado(eixo, agora_us) ||
             !isfinite(estado[eixo].referenciaCampo_uT) ||
             !isfinite(estado[eixo].campoMedido_uT)))
            return false;
    }

    float dt = ConfiguracoesControle::PERIODO_MALHA_CAMPO_US * 1.0e-6f;
    if (ultimaAtualizacao_us != 0) {
        dt = (uint32_t)(agora_us - ultimaAtualizacao_us) * 1.0e-6f;
        dt = limitar(dt, 0.001f, 0.020f);
    }
    ultimaAtualizacao_us = agora_us;

    for (uint8_t eixo=0; eixo<3; ++eixo) {
        EstadoControleEixo &s = estado[eixo];
        if (!eixoEstaHabilitado(eixo)) {
            s.comandoPWM = 0;
            continue;
        }
        const ParametrosCalibracaoEixo &p = calibracao.eixo[eixo];
        const float dB = s.referenciaCampo_uT - p.campoEmZero_uT;
        const float k = coefCampoPorPwm(eixo, dB >= 0.0f);
        const float minPwm = (float)max((int)p.pwmOperacionalNegativo, (int)p.pwmCalibracaoNegativo);
        const float maxPwm = (float)min((int)p.pwmOperacionalPositivo, (int)p.pwmCalibracaoPositivo);
        // FF CONTINUO em B=B0: nao ha salto +/- 500 PWM do modelo I(PWM).
        const float pwmFF = limitar(dB/k, minPwm, maxPwm);
        s.erroCampo_uT = s.referenciaCampo_uT - s.campoMedido_uT;
        const float erroPWM = s.erroCampo_uT/k;
        const float proporcional = ConfiguracoesControle::KP_CAMPO[eixo]*erroPWM;
        const float incrementoI =
            ConfiguracoesControle::KI_CAMPO_POR_S[eixo]*erroPWM*dt;
        const float candidatoI = s.integralCampo_PWM + incrementoI;
        const float anterior = (float)s.comandoPWM;
        const float rampa = ConfiguracoesControle::PASSO_MAX_PWM;
        const float aplicadoCandidato = limitar(
            limitar(pwmFF+proporcional+candidatoI,minPwm,maxPwm),
            anterior-rampa,anterior+rampa);
        const float brutoCandidato = pwmFF+proporcional+candidatoI;
        // Antiwindup: integra apenas se nao limitado ou se ajuda a sair
        // do limite, seja o limite de PWM ou o de rampa.
        if (fabsf(brutoCandidato-aplicadoCandidato)<1e-4f ||
            (brutoCandidato>aplicadoCandidato && incrementoI<0) ||
            (brutoCandidato<aplicadoCandidato && incrementoI>0)) {
            s.integralCampo_PWM = candidatoI;
        }
        const float comando = limitar(
            limitar(pwmFF+proporcional+s.integralCampo_PWM,minPwm,maxPwm),
            anterior-rampa,anterior+rampa);
        s.pwmAntecipado = pwmFF;
        s.comandoPWM = (int16_t)lroundf(comando);
        // Nao ha I_ref. Formato binario/USB legado preservado.
        s.referenciaCorrente_A = NAN;
    }
    return true;
}
int16_t ControladorCampo::obterPWM(uint8_t eixo) const {
    return eixo<3 ? estado[eixo].comandoPWM : 0;
}
float ControladorCampo::obterLimiteOperacional(uint8_t eixo) const {
    return calibracaoValidaParaEixo(eixo) ?
           calibracao.eixo[eixo].limiteOperacional_A : 0.0f;
}
float ControladorCampo::obterLimiteCritico(uint8_t eixo) const {
    return calibracaoValidaParaEixo(eixo) ?
           calibracao.eixo[eixo].limiteCritico_A : 0.0f;
}
bool ControladorCampo::obterLimitesOperacionaisCampo(
    uint8_t eixo, float &minimo_uT, float &maximo_uT) const {
    if (!calibracaoValidaParaEixo(eixo)) return false;
    const ParametrosCalibracaoEixo &p = calibracao.eixo[eixo];
    // Publica somente referencias com margem para o PI. O teto de PWM
    // aplicado em atualizarMalhaCampo NAO muda. O erro maximo do ajuste
    // reduz a extrapolacao otimista nos extremos de B(PWM); a reserva
    // adicional corresponde a somente 1% do PWM calibrado por ramo.
    const float pwmNeg = (float)max((int)p.pwmOperacionalNegativo,
                                   (int)p.pwmCalibracaoNegativo);
    const float pwmPos = (float)min((int)p.pwmOperacionalPositivo,
                                   (int)p.pwmCalibracaoPositivo);
    const float margem = ConfiguracoesControle::FRACAO_RESERVA_PWM_REFERENCIA;
    const float erroNeg = p.qualidadeCampoPwmNegativo.erroAbsolutoMaximo;
    const float erroPos = p.qualidadeCampoPwmPositivo.erroAbsolutoMaximo;
    // Uma calibracao anterior sem metrica valida nao pode aumentar a faixa;
    // nesse caso usa somente a reserva de PWM para manter compatibilidade.
    const float guardaNeg = (isfinite(erroNeg) && erroNeg >= 0.0f) ? erroNeg : 0.0f;
    const float guardaPos = (isfinite(erroPos) && erroPos >= 0.0f) ? erroPos : 0.0f;
    minimo_uT = p.campoEmZero_uT +
        coefCampoPorPwm(eixo,false) * (pwmNeg * (1.0f - margem)) + guardaNeg;
    maximo_uT = p.campoEmZero_uT +
        coefCampoPorPwm(eixo,true) * (pwmPos * (1.0f - margem)) - guardaPos;
    // Os comandos de referencia trafegam com 6 decimais. Arredondar
    // para dentro a 0,001 uT evita rejeicao numerica no extremo do slider.
    minimo_uT = ceilf(minimo_uT * 1000.0f) / 1000.0f;
    maximo_uT = floorf(maximo_uT * 1000.0f) / 1000.0f;
    return isfinite(minimo_uT) && isfinite(maximo_uT) &&
           minimo_uT < maximo_uT;
}
const EstadoControleEixo &ControladorCampo::obterEstado(uint8_t eixo) const {
    return estado[eixo<3 ? eixo : 0];
}
