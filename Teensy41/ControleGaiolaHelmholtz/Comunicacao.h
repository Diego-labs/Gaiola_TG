// Interface do protocolo serial entre MATLAB e Teensy.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#ifndef COMUNICACAO_H
#define COMUNICACAO_H

#include <Arduino.h>
#include "ConfiguracaoExperimento.h"
#include "TiposCalibracao.h"
#include "TiposControle.h"
#include "TiposSimulacao.h"

class Comunicacao
{
public:

    Comunicacao();

    void iniciar();
    void atualizar();

    bool possuiConfiguracao();
    ConfiguracaoExperimento obterConfiguracao();

    bool haNovaConfiguracao();
    void limparNovaConfiguracao();

    bool limpezaFoiSolicitada();
    void limparSolicitacaoLimpeza();

    bool inicioFoiSolicitado();
    void limparSolicitacaoInicio();

    bool estadoFoiSolicitado();
    void limparSolicitacaoEstado();

    bool baixarDadosFoiSolicitado();
    void limparSolicitacaoBaixarDados();

    // Calibracao automatica
    bool possuiConfiguracaoCalibracao();
    SolicitacaoCalibracao obterConfiguracaoCalibracao();
    bool haNovaConfiguracaoCalibracao();
    void limparNovaConfiguracaoCalibracao();

    bool inicioCalibracaoFoiSolicitado();
    void limparSolicitacaoInicioCalibracao();

    bool estadoCalibracaoFoiSolicitado();
    void limparSolicitacaoEstadoCalibracao();

    bool abortoCalibracaoFoiSolicitado();
    void limparSolicitacaoAbortoCalibracao();

    bool parametrosCalibracaoEstaoProntos();
    ParametrosCalibracao obterParametrosCalibracao();
    void limparParametrosCalibracaoProntos();

    // Controle direto de campo
    bool inicioControleFoiSolicitado();
    void limparSolicitacaoInicioControle();

    bool paradaControleFoiSolicitada();
    void limparSolicitacaoParadaControle();

    bool estadoControleFoiSolicitado();
    void limparSolicitacaoEstadoControle();

    bool limitesControleForamSolicitados();
    void limparSolicitacaoLimitesControle();


    bool baixarControleFoiSolicitado();
    void limparSolicitacaoBaixarDadosControle();

    bool possuiReferenciaCampo();
    bool haNovaReferenciaCampo();
    ReferenciaCampo obterReferenciaCampo();
    void limparNovaReferenciaCampo();

    // Trajetoria / simulacao orbital
    bool inicioTrajetoriaFoiSolicitado();
    void limparSolicitacaoInicioTrajetoria();
    CabecalhoTrajetoria obterCabecalhoTrajetoria();

    bool pontoTrajetoriaEstaPronto();
    void limparPontoTrajetoriaPronto();
    PontoTrajetoria obterPontoTrajetoria();

    bool fimTrajetoriaFoiSolicitado();
    void limparSolicitacaoFimTrajetoria();

    bool abortoTrajetoriaFoiSolicitado();
    void limparSolicitacaoAbortoTrajetoria();

    bool estadoTrajetoriaFoiSolicitado();
    void limparSolicitacaoEstadoTrajetoria();

    bool inicioSimulacaoFoiSolicitado();
    void limparSolicitacaoInicioSimulacao();
    SolicitacaoInicioSimulacao obterSolicitacaoInicioSimulacao();

    bool pausaSimulacaoFoiSolicitada();
    void limparSolicitacaoPausaSimulacao();

    bool retomadaSimulacaoFoiSolicitada();
    void limparSolicitacaoRetomadaSimulacao();

    bool abortoSimulacaoFoiSolicitado();
    void limparSolicitacaoAbortoSimulacao();

    bool estadoSimulacaoFoiSolicitado();
    void limparSolicitacaoEstadoSimulacao();

    bool baixarSimulacaoFoiSolicitado();
    void limparSolicitacaoBaixarDadosSimulacao();

    bool possuiHeartbeat() const;
    bool heartbeatEstaAtivo(uint32_t tempoLimite_ms) const;
    uint32_t obterUltimoHeartbeatMs() const;

private:

    static const uint16_t TAMANHO_MEMORIA_RECEPCAO = 768;

    char bufferTemporario[TAMANHO_MEMORIA_RECEPCAO];
    uint16_t indiceBuffer;

    ConfiguracaoExperimento configuracao;

    bool configuracaoRecebida;
    bool novaConfiguracaoDisponivel;

    bool limpezaSolicitada;
    bool inicioSolicitado;
    bool estadoSolicitado;

    void processarComando(char* comando);
    void interpretarConfiguracao(char* parametros);

    bool baixarDadosSolicitado;

    SolicitacaoCalibracao configuracaoCalibracao;
    bool configuracaoCalibracaoRecebida;
    bool novaConfiguracaoCalibracao;
    bool inicioCalibracaoSolicitado;
    bool estadoCalibracaoSolicitado;
    bool abortoCalibracaoSolicitado;

    ParametrosCalibracao parametrosCalibracao;
    bool transferenciaParametrosCalibracaoAtiva;
    bool parametrosCalibracaoProntos;
    uint8_t mascaraParametrosEsperados;
    uint8_t mascaraLimitesRecebidos;
    uint8_t mascaraCampoPwmRecebida;

    void interpretarConfiguracaoCalibracao(char *parametros);
    void interpretarInicioParametrosCalibracao(char *parametros);
    void interpretarLimitesParametrosCalibracao(char *parametros);
    void interpretarParametrosCampoPWM(char *parametros);
    void confirmarParametrosCalibracao();

    bool inicioControleSolicitado;
    bool paradaControleSolicitada;
    bool estadoControleSolicitado;
    bool limitesControleSolicitados;
    bool baixarControleSolicitado;

    ReferenciaCampo referenciaCampo;
    bool referenciaCampoRecebida;
    bool novaReferenciaCampo;

    void interpretarReferenciaCampo(char *parametros);

    CabecalhoTrajetoria cabecalhoTrajetoria;
    PontoTrajetoria pontoTrajetoria;
    SolicitacaoInicioSimulacao solicitacaoInicioSimulacao;

    bool inicioTrajetoriaSolicitado;
    bool pontoTrajetoriaPronto;
    bool fimTrajetoriaSolicitado;
    bool abortoTrajetoriaSolicitado;
    bool estadoTrajetoriaSolicitado;

    bool inicioSimulacaoSolicitado;
    bool pausaSimulacaoSolicitada;
    bool retomadaSimulacaoSolicitada;
    bool abortoSimulacaoSolicitado;
    bool estadoSimulacaoSolicitado;
    bool baixarSimulacaoSolicitado;

    bool heartbeatRecebido;
    uint32_t ultimoHeartbeatMs;

    void interpretarInicioTrajetoria(char *parametros);
    void interpretarPontoTrajetoria(char *parametros);
    void interpretarInicioSimulacao(char *parametros);
};

#endif
