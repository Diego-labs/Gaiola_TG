// Interface da maquina de estados da calibracao automatica.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#ifndef GERENCIADOR_CALIBRACAO_H
#define GERENCIADOR_CALIBRACAO_H

#include <Arduino.h>
#include <math.h>

#include "ConfiguracoesCalibracao.h"
#include "TiposCalibracao.h"
#include "AcionadorDRV8871.h"
#include "SensorINA226.h"
#include "ConversorAD7606.h"
#include "RegistradorDados.h"

class GerenciadorCalibracao
{
public:
    enum Estado
    {
        CALIBRACAO_OCIOSA,
        CALIBRACAO_AGUARDANDO_ZERO,
        CALIBRACAO_AGUARDANDO_PONTO,
        CALIBRACAO_CONCLUIDA,
        CALIBRACAO_FALHA
    };

    GerenciadorCalibracao(
        AcionadorDRV8871 &acionadorX,
        AcionadorDRV8871 &acionadorY,
        AcionadorDRV8871 &acionadorZ,
        SensorINA226 &inaX,
        SensorINA226 &inaY,
        SensorINA226 &inaZ,
        ConversorAD7606 &conversorADC,
        RegistradorDados &registrador
    );

    void definirHardwarePronto(
        bool inaXPronto,
        bool inaYPronto,
        bool inaZPronto,
        bool ad7606Pronto,
        bool sdPronto
    );

    bool configurar(const SolicitacaoCalibracao &solicitacao);
    bool iniciar();
    void atualizar();
    void abortar(const char *motivo);

    bool estaConfigurado() const;
    bool estaExecutando() const;
    bool estaConcluido() const;
    bool estaEmFalha() const;

    Estado obterEstado() const;
    const char *obterNomeEstado() const;

    int16_t obterPWMAtual() const;
    char obterEixoAtivo() const;

    SolicitacaoCalibracao obterSolicitacao() const;
    LimitesCalibracaoEixo obterLimitesEixo(uint8_t indiceEixo) const;

private:
    enum ProximaEtapaAposZero
    {
        ZERO_PARA_BUSCA_POSITIVA,
        ZERO_PARA_BUSCA_NEGATIVA,
        ZERO_PARA_VARREDURA_ZERO,
        ZERO_PARA_VARREDURA_NEGATIVA,
        ZERO_PARA_PROXIMO_EIXO,
        ZERO_PARA_FINALIZAR
    };

    enum FinalidadePonto
    {
        PONTO_SEM_FINALIDADE,
        PONTO_BUSCA,
        PONTO_VARREDURA_ZERO,
        PONTO_VARREDURA_POSITIVA,
        PONTO_VARREDURA_NEGATIVA,
        PONTO_VERIFICACAO_ZERO
    };

    struct MedicaoPonto
    {
        float mediaCorrente_A;
        float correnteAbsolutaMaxima_A;
        float mediaCampo_uT[3];
    };

    AcionadorDRV8871 *acionadores[3];
    SensorINA226 *sensores[3];
    ConversorAD7606 *conversorADC;
    RegistradorDados *registrador;

    bool inaPronto[3];
    bool adcPronto;
    bool sdPronto;

    SolicitacaoCalibracao solicitacao;
    bool configurado;

    Estado estado;
    uint8_t indiceEixoAtivo;
    int16_t pwmAtual;

    LimitesCalibracaoEixo limites[3];

    // Busca do limite
    int8_t direcaoBusca;
    bool buscaFina;
    int16_t pwmCandidatoBusca;
    int16_t pwmSeguroAnterior;
    int16_t pwmOperacionalBusca;

    // Varredura de calibração
    uint16_t indicePontoVarredura;
    uint16_t indiceLadoVarredura;

    // Retorno a zero
    ProximaEtapaAposZero proximaEtapaAposZero;
    uint32_t inicioEsperaZero_ms;

    // Aquisição de um ponto
    FinalidadePonto finalidadePonto;
    bool pontoAtivo;
    uint32_t inicioAcomodacaoPonto_ms;
    uint32_t inicioEsperaAmostra_ms;
    uint8_t amostrasColetadas;
    float somaCorrente_A;
    float correnteAbsolutaMaxima_A;
    float somaCampo_uT[3];

    // Controle de aviso de soft limit durante a varredura
    bool avisoLimiteOperacionalImpresso;

    void pararTodosEixos();
    void definirPWMDoEixoAtivo(int16_t pwm);

    bool selecionarProximoEixo(uint8_t indiceInicial);
    void iniciarEixoAtual();
    void finalizarEixoAtual();
    void finalizarCalibracao();

    void iniciarEsperaZero(ProximaEtapaAposZero proximo);
    void processarMedicaoZero(const MedicaoPonto &medicao);

    void iniciarBusca(int8_t direcao);
    void agendarProximoCandidatoBusca();
    void processarMedicaoBusca(const MedicaoPonto &medicao);
    void finalizarDirecaoBusca();
    void avisarTetoPWMSemAtingirSoftLimit(const MedicaoPonto &medicao);

    void iniciarVarredura();
    void agendarZeroVarredura();
    void agendarProximoPontoPositivo();
    void agendarProximoPontoNegativo();
    void processarMedicaoVarredura(
        const MedicaoPonto &medicao,
        FinalidadePonto finalidade
    );

    void iniciarMedicaoPonto(
        int16_t pwm,
        FinalidadePonto finalidade
    );

    void atualizarMedicaoPonto();
    void zerarAcumuladorPonto();
    void finalizarMedicaoPonto();

    bool lerAmostraCorrente(float &corrente_A);
    bool lerAmostraCampo(float B_uT[3]);


    void tratarResultadoPonto(
        const MedicaoPonto &medicao,
        FinalidadePonto finalidade
    );

    void informarEtapa(const char *nomeEtapa);
    void informarPonto(const RegistroPontoCalibracao &registro);
    void informarLimitesEixo(uint8_t indiceEixo);

    void falhar(const char *motivo);
};

#endif
