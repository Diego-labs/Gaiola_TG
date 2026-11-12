// Parametros de temporizacao e comunicacao da simulacao.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#ifndef CONFIGURACOES_SIMULACAO_H
#define CONFIGURACOES_SIMULACAO_H

#include <Arduino.h>

namespace ConfiguracoesSimulacao
{
    // Taxa máxima de troca da referência orbital na gaiola.
    // Para uma trajetória com dt mínimo de 1 s, isso permite
    // aceleração máxima de 100x.
    constexpr float TAXA_MAXIMA_REFERENCIA_HZ = 100.0f;

    // Telemetria enviada ao MATLAB somente para supervisão/gráfico.
    constexpr uint32_t PERIODO_TELEMETRIA_MS = 50; // 20 Hz

    // MATLAB envia HEARTBEAT periodicamente. A perda de comunicação
    // durante EXECUTANDO ou PAUSADO é tratada como falha.
    constexpr uint32_t TEMPO_LIMITE_HEARTBEAT_MS = 1000;

    // Confirmação de progresso durante o upload. O MATLAB pode enviar
    // blocos deste tamanho e aguardar TRAJ_PROGRESS antes de prosseguir.
    constexpr uint32_t INTERVALO_CONFIRMACAO_TRAJETORIA = 64;

    constexpr uint16_t VERSAO_FORMATO_TRAJETORIA = 1;
    constexpr uint16_t VERSAO_FORMATO_REGISTRO_SIMULACAO = 1;
}

#endif
