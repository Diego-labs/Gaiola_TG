// Estruturas de dados usadas na simulacao de trajetorias.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#ifndef TIPOS_SIMULACAO_H
#define TIPOS_SIMULACAO_H

#include <Arduino.h>

constexpr uint8_t TAMANHO_ID_TRAJETORIA = 32;

struct CabecalhoTrajetoria
{
    char id[TAMANHO_ID_TRAJETORIA];
    uint32_t quantidadePontos;
};

// Registro salvo no SD. O índice também é armazenado para permitir
// verificar a integridade/sequência do arquivo posteriormente.
struct PontoTrajetoria
{
    uint32_t indice;
    float tempo_s;
    float B_uT[3];
};

struct InformacoesTrajetoria
{
    char id[TAMANHO_ID_TRAJETORIA];
    uint32_t quantidadePontos;
    float primeiroTempo_s;
    float ultimoTempo_s;
    float menorIntervalo_s;
};

struct SolicitacaoInicioSimulacao
{
    float aceleracao;
};

// Registro científico do ensaio orbital. Ele é gravado no SD na taxa
// de aquisição do campo, enquanto a telemetria USB é deliberadamente
// mais lenta.
struct RegistroSimulacao
{
    uint32_t tempoRealDecorrido_ms;
    uint32_t indiceTrajetoria;
    float tempoSimulado_s;

    float referenciaCampo_uT[3];
    float campoMedido_uT[3];

    float referenciaCorrente_A[3];
    float correnteMedida_A[3];

    int16_t pwm[3];
    uint16_t indicadores;
};

static_assert(
    sizeof(PontoTrajetoria) == 20,
    "TrajectoryPoint deve possuir 20 bytes"
);

static_assert(
    sizeof(RegistroSimulacao) == 68,
    "SimulationLogRecord deve possuir 68 bytes"
);

#endif
