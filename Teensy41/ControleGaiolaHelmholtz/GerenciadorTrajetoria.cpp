// Implementacao do envio e reproducao temporizada de trajetorias.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#include "GerenciadorTrajetoria.h"
#include <string.h>

GerenciadorTrajetoria::GerenciadorTrajetoria(RegistradorDados &registradorRef)
{
    registrador = &registradorRef;
    estado = TRAJETORIA_VAZIA;

    memset(&cabecalhoEnvio, 0, sizeof(cabecalhoEnvio));
    memset(&informacoes, 0, sizeof(informacoes));

    pontosRecebidos = 0;
    primeiroPontoEnvio = true;
    tempoPontoAnterior_s = 0.0f;

    aceleracao = 1.0f;

    memset(&pontoAtual, 0, sizeof(pontoAtual));
    memset(&proximoPonto, 0, sizeof(proximoPonto));
    proximoPontoValido = false;
    atrasoTemporizacao = false;

    ancoraTempoReal_ms = 0;
    ancoraTempoSimulado_s = 0.0f;
    tempoSimuladoPausado_s = 0.0f;
}

bool GerenciadorTrajetoria::iniciarEnvio(const CabecalhoTrajetoria &cabecalho)
{
    if (estado == TRAJETORIA_EXECUTANDO || estado == TRAJETORIA_PAUSADA ||
        estado == TRAJETORIA_RECEBENDO)
    {
        return false;
    }

    if (cabecalho.quantidadePontos < 2 || cabecalho.id[0] == '\0')
        return false;

    if (!registrador->iniciarEnvioTrajetoria(cabecalho))
        return false;

    cabecalhoEnvio = cabecalho;
    pontosRecebidos = 0;
    atrasoTemporizacao = false;
    primeiroPontoEnvio = true;
    tempoPontoAnterior_s = 0.0f;

    memset(&informacoes, 0, sizeof(informacoes));
    strncpy(informacoes.id, cabecalho.id, sizeof(informacoes.id) - 1);
    informacoes.id[sizeof(informacoes.id) - 1] = '\0';
    informacoes.quantidadePontos = cabecalho.quantidadePontos;
    informacoes.menorIntervalo_s = INFINITY;

    estado = TRAJETORIA_RECEBENDO;
    return true;
}

bool GerenciadorTrajetoria::adicionarPonto(const PontoTrajetoria &ponto)
{
    if (estado != TRAJETORIA_RECEBENDO)
        return false;

    if (pontosRecebidos >= cabecalhoEnvio.quantidadePontos)
        return false;

    if (ponto.indice != pontosRecebidos)
        return false;

    if (!isfinite(ponto.tempo_s) || ponto.tempo_s < 0.0f)
        return false;

    for (uint8_t eixo = 0; eixo < 3; eixo++)
    {
        if (!isfinite(ponto.B_uT[eixo]))
            return false;
    }

    if (primeiroPontoEnvio)
    {
        informacoes.primeiroTempo_s = ponto.tempo_s;
        primeiroPontoEnvio = false;
    }
    else
    {
        const float dt = ponto.tempo_s - tempoPontoAnterior_s;

        if (!isfinite(dt) || dt <= 0.0f)
            return false;

        if (dt < informacoes.menorIntervalo_s)
            informacoes.menorIntervalo_s = dt;
    }

    if (!registrador->adicionarPontoTrajetoria(ponto))
        return false;

    tempoPontoAnterior_s = ponto.tempo_s;
    informacoes.ultimoTempo_s = ponto.tempo_s;
    pontosRecebidos++;

    return true;
}

bool GerenciadorTrajetoria::finalizarEnvio()
{
    if (estado != TRAJETORIA_RECEBENDO)
        return false;

    if (pontosRecebidos != cabecalhoEnvio.quantidadePontos || primeiroPontoEnvio)
        return false;

    if (!isfinite(informacoes.menorIntervalo_s) || informacoes.menorIntervalo_s <= 0.0f)
        return false;

    if (!registrador->finalizarEnvioTrajetoria(informacoes))
    {
        estado = TRAJETORIA_FALHA;
        return false;
    }

    estado = TRAJETORIA_PRONTA;
    return true;
}

void GerenciadorTrajetoria::abortarEnvio()
{
    if (estado != TRAJETORIA_RECEBENDO)
        return;

    registrador->abortarEnvioTrajetoria();
    pontosRecebidos = 0;
    estado = TRAJETORIA_VAZIA;
}

bool GerenciadorTrajetoria::temporizacaoEhValida(float aceleracaoSolicitada) const
{
    if (!isfinite(aceleracaoSolicitada) || aceleracaoSolicitada <= 0.0f)
        return false;

    if (!estaPronto() || !isfinite(informacoes.menorIntervalo_s) ||
        informacoes.menorIntervalo_s <= 0.0f)
    {
        return false;
    }

    const float taxaHz = aceleracaoSolicitada / informacoes.menorIntervalo_s;

    return taxaHz <=
        (ConfiguracoesSimulacao::TAXA_MAXIMA_REFERENCIA_HZ + 1.0e-5f);
}

bool GerenciadorTrajetoria::iniciar(float aceleracaoSolicitada, uint32_t agora_ms)
{
    if (!temporizacaoEhValida(aceleracaoSolicitada))
        return false;

    if (!registrador->abrirReproducaoTrajetoria())
        return false;

    PontoTrajetoria primeiro;

    if (!registrador->lerProximoPontoTrajetoria(primeiro))
    {
        registrador->fecharReproducaoTrajetoria();
        return false;
    }

    if (primeiro.indice != 0)
    {
        registrador->fecharReproducaoTrajetoria();
        return false;
    }

    pontoAtual = primeiro;
    proximoPontoValido = lerProximoPonto();

    if (!proximoPontoValido)
    {
        registrador->fecharReproducaoTrajetoria();
        return false;
    }

    aceleracao = aceleracaoSolicitada;
    atrasoTemporizacao = false;
    ancoraTempoReal_ms = agora_ms;
    ancoraTempoSimulado_s = pontoAtual.tempo_s;
    tempoSimuladoPausado_s = ancoraTempoSimulado_s;

    estado = TRAJETORIA_EXECUTANDO;
    return true;
}

bool GerenciadorTrajetoria::pausar(uint32_t agora_ms)
{
    if (estado != TRAJETORIA_EXECUTANDO)
        return false;

    tempoSimuladoPausado_s = obterTempoSimulado_s(agora_ms);
    estado = TRAJETORIA_PAUSADA;
    return true;
}

bool GerenciadorTrajetoria::retomar(uint32_t agora_ms)
{
    if (estado != TRAJETORIA_PAUSADA)
        return false;

    ancoraTempoSimulado_s = tempoSimuladoPausado_s;
    ancoraTempoReal_ms = agora_ms;
    estado = TRAJETORIA_EXECUTANDO;
    return true;
}

void GerenciadorTrajetoria::abortarReproducao()
{
    if (estado == TRAJETORIA_EXECUTANDO || estado == TRAJETORIA_PAUSADA ||
        estado == TRAJETORIA_CONCLUIDA)
    {
        registrador->fecharReproducaoTrajetoria();
    }

    if (informacoes.quantidadePontos >= 2)
        estado = TRAJETORIA_PRONTA;
    else
        estado = TRAJETORIA_VAZIA;
}

bool GerenciadorTrajetoria::lerProximoPonto()
{
    PontoTrajetoria ponto;

    if (!registrador->lerProximoPontoTrajetoria(ponto))
        return false;

    if (ponto.indice != pontoAtual.indice + 1)
    {
        estado = TRAJETORIA_FALHA;
        return false;
    }

    if (!isfinite(ponto.tempo_s) || ponto.tempo_s <= pontoAtual.tempo_s)
    {
        estado = TRAJETORIA_FALHA;
        return false;
    }

    proximoPonto = ponto;
    return true;
}

bool GerenciadorTrajetoria::atualizar(uint32_t agora_ms)
{
    if (estado != TRAJETORIA_EXECUTANDO)
        return false;

    const float tempoSimulado = obterTempoSimulado_s(agora_ms);
    bool referenciaMudou = false;
    uint8_t pontosAvancados = 0;

    while (proximoPontoValido && tempoSimulado >= proximoPonto.tempo_s)
    {
        pontoAtual = proximoPonto;
        referenciaMudou = true;
        pontosAvancados++;

        // Se duas ou mais referências já venceram entre duas chamadas,
        // o firmware não conseguiu respeitar a temporização solicitada.
        // Para validação é preferível abortar a pular pontos silenciosamente.
        if (pontosAvancados > 1)
        {
            atrasoTemporizacao = true;
            estado = TRAJETORIA_FALHA;
            registrador->fecharReproducaoTrajetoria();
            return referenciaMudou;
        }

        if (pontoAtual.indice + 1 >= informacoes.quantidadePontos)
        {
            proximoPontoValido = false;
            break;
        }

        if (!lerProximoPonto())
        {
            proximoPontoValido = false;

            if (estado != TRAJETORIA_FALHA)
                estado = TRAJETORIA_FALHA;

            registrador->fecharReproducaoTrajetoria();
            return referenciaMudou;
        }
    }

    if (!proximoPontoValido &&
        pontoAtual.indice + 1 == informacoes.quantidadePontos &&
        tempoSimulado >= informacoes.ultimoTempo_s)
    {
        tempoSimuladoPausado_s = informacoes.ultimoTempo_s;
        estado = TRAJETORIA_CONCLUIDA;
        registrador->fecharReproducaoTrajetoria();
    }

    return referenciaMudou;
}

bool GerenciadorTrajetoria::estaRecebendo() const
{
    return estado == TRAJETORIA_RECEBENDO;
}

bool GerenciadorTrajetoria::estaPronto() const
{
    return estado == TRAJETORIA_PRONTA || estado == TRAJETORIA_CONCLUIDA;
}

bool GerenciadorTrajetoria::estaExecutando() const
{
    return estado == TRAJETORIA_EXECUTANDO;
}

bool GerenciadorTrajetoria::estaPausado() const
{
    return estado == TRAJETORIA_PAUSADA;
}

bool GerenciadorTrajetoria::estaConcluido() const
{
    return estado == TRAJETORIA_CONCLUIDA;
}

bool GerenciadorTrajetoria::estaEmFalha() const
{
    return estado == TRAJETORIA_FALHA;
}

bool GerenciadorTrajetoria::houveAtrasoTemporizacao() const
{
    return atrasoTemporizacao;
}

GerenciadorTrajetoria::Estado GerenciadorTrajetoria::obterEstado() const
{
    return estado;
}

const char *GerenciadorTrajetoria::obterNomeEstado() const
{
    switch (estado)
    {
        case TRAJETORIA_VAZIA: return "EMPTY";
        case TRAJETORIA_RECEBENDO: return "RECEIVING";
        case TRAJETORIA_PRONTA: return "READY";
        case TRAJETORIA_EXECUTANDO: return "RUNNING";
        case TRAJETORIA_PAUSADA: return "PAUSED";
        case TRAJETORIA_CONCLUIDA: return "COMPLETE";
        case TRAJETORIA_FALHA: return "FAULT";
        default: return "UNKNOWN";
    }
}

uint32_t GerenciadorTrajetoria::obterQuantidadePontosRecebidos() const
{
    return pontosRecebidos;
}

uint32_t GerenciadorTrajetoria::obterQuantidadePontosEsperados() const
{
    return cabecalhoEnvio.quantidadePontos;
}

uint32_t GerenciadorTrajetoria::obterIndiceAtual() const
{
    return pontoAtual.indice;
}

float GerenciadorTrajetoria::obterAceleracao() const
{
    return aceleracao;
}

float GerenciadorTrajetoria::obterAceleracaoMaximaPermitida() const
{
    if (!isfinite(informacoes.menorIntervalo_s) || informacoes.menorIntervalo_s <= 0.0f)
        return 0.0f;

    return ConfiguracoesSimulacao::TAXA_MAXIMA_REFERENCIA_HZ *
        informacoes.menorIntervalo_s;
}

float GerenciadorTrajetoria::obterTaxaMaximaReferenciaHz() const
{
    if (!isfinite(informacoes.menorIntervalo_s) || informacoes.menorIntervalo_s <= 0.0f)
        return 0.0f;

    return aceleracao / informacoes.menorIntervalo_s;
}

float GerenciadorTrajetoria::obterTempoSimulado_s(uint32_t agora_ms) const
{
    if (estado == TRAJETORIA_PAUSADA || estado == TRAJETORIA_CONCLUIDA)
        return tempoSimuladoPausado_s;

    if (estado != TRAJETORIA_EXECUTANDO)
        return informacoes.primeiroTempo_s;

    const uint32_t tempoDecorrido_ms = (uint32_t)(agora_ms - ancoraTempoReal_ms);

    float valor = ancoraTempoSimulado_s +
        ((float)tempoDecorrido_ms * 0.001f * aceleracao);

    if (valor > informacoes.ultimoTempo_s)
        valor = informacoes.ultimoTempo_s;

    return valor;
}

uint32_t GerenciadorTrajetoria::obterTempoRealDecorrido_ms(uint32_t agora_ms) const
{
    if (aceleracao <= 0.0f)
        return 0;

    const float tempoSimuladoDecorrido =
        obterTempoSimulado_s(agora_ms) - informacoes.primeiroTempo_s;

    if (tempoSimuladoDecorrido <= 0.0f)
        return 0;

    return (uint32_t)lroundf(
        tempoSimuladoDecorrido * 1000.0f / aceleracao
    );
}

float GerenciadorTrajetoria::obterProgressoPercentual(uint32_t agora_ms) const
{
    const float duracao = informacoes.ultimoTempo_s - informacoes.primeiroTempo_s;

    if (duracao <= 0.0f)
        return 0.0f;

    float progresso = 100.0f *
        (obterTempoSimulado_s(agora_ms) - informacoes.primeiroTempo_s) /
        duracao;

    if (progresso < 0.0f) progresso = 0.0f;
    if (progresso > 100.0f) progresso = 100.0f;

    return progresso;
}

ReferenciaCampo GerenciadorTrajetoria::obterReferenciaAtual() const
{
    ReferenciaCampo referencia;

    for (uint8_t eixo = 0; eixo < 3; eixo++)
        referencia.B_uT[eixo] = pontoAtual.B_uT[eixo];

    return referencia;
}

const InformacoesTrajetoria &GerenciadorTrajetoria::obterInformacoes() const
{
    return informacoes;
}
