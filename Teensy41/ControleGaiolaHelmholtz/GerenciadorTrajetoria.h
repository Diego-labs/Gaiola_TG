// Interface do gerenciador de trajetorias orbitais.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#ifndef GERENCIADOR_TRAJETORIA_H
#define GERENCIADOR_TRAJETORIA_H

#include <Arduino.h>
#include <math.h>

#include "RegistradorDados.h"
#include "ConfiguracoesSimulacao.h"
#include "TiposSimulacao.h"
#include "TiposControle.h"

class GerenciadorTrajetoria
{
public:
    enum Estado
    {
        TRAJETORIA_VAZIA,
        TRAJETORIA_RECEBENDO,
        TRAJETORIA_PRONTA,
        TRAJETORIA_EXECUTANDO,
        TRAJETORIA_PAUSADA,
        TRAJETORIA_CONCLUIDA,
        TRAJETORIA_FALHA
    };

    explicit GerenciadorTrajetoria(RegistradorDados &registradorRef);

    bool iniciarEnvio(const CabecalhoTrajetoria &cabecalho);
    bool adicionarPonto(const PontoTrajetoria &ponto);
    bool finalizarEnvio();
    void abortarEnvio();

    bool iniciar(float aceleracao, uint32_t agora_ms);
    bool pausar(uint32_t agora_ms);
    bool retomar(uint32_t agora_ms);
    void abortarReproducao();

    // Retorna true quando a referência ativa mudou nesta chamada.
    bool atualizar(uint32_t agora_ms);

    bool estaRecebendo() const;
    bool estaPronto() const;
    bool estaExecutando() const;
    bool estaPausado() const;
    bool estaConcluido() const;
    bool estaEmFalha() const;
    bool houveAtrasoTemporizacao() const;

    Estado obterEstado() const;
    const char *obterNomeEstado() const;

    uint32_t obterQuantidadePontosRecebidos() const;
    uint32_t obterQuantidadePontosEsperados() const;
    uint32_t obterIndiceAtual() const;

    float obterAceleracao() const;
    float obterAceleracaoMaximaPermitida() const;
    float obterTaxaMaximaReferenciaHz() const;
    float obterTempoSimulado_s(uint32_t agora_ms) const;
    uint32_t obterTempoRealDecorrido_ms(uint32_t agora_ms) const;
    float obterProgressoPercentual(uint32_t agora_ms) const;

    ReferenciaCampo obterReferenciaAtual() const;
    const InformacoesTrajetoria &obterInformacoes() const;

private:
    RegistradorDados *registrador;
    Estado estado;

    CabecalhoTrajetoria cabecalhoEnvio;
    InformacoesTrajetoria informacoes;

    uint32_t pontosRecebidos;
    bool primeiroPontoEnvio;
    float tempoPontoAnterior_s;

    float aceleracao;

    PontoTrajetoria pontoAtual;
    PontoTrajetoria proximoPonto;
    bool proximoPontoValido;
    bool atrasoTemporizacao;

    uint32_t ancoraTempoReal_ms;
    float ancoraTempoSimulado_s;
    float tempoSimuladoPausado_s;

    bool lerProximoPonto();
    bool temporizacaoEhValida(float aceleracaoSolicitada) const;
};

#endif
