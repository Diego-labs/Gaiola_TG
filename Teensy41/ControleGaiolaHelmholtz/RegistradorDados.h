// Interface de gravacao e recuperacao dos dados no cartao SD.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#ifndef REGISTRADOR_DADOS_H
#define REGISTRADOR_DADOS_H

#include <Arduino.h>

#include "SdFat.h"
#include "RingBuf.h"

#include "ConfiguracaoExperimento.h"
#include "TiposCalibracao.h"
#include "TiposControle.h"
#include "TiposSimulacao.h"

// DIRETÓRIO

#define DIRETORIO_DADOS "/TG_DATA"
#define DIRETORIO_CONTROLE_MANUAL DIRETORIO_DADOS "/MANUAL_CONTROL"
#define DIRETORIO_SIMULACAO DIRETORIO_DADOS "/SIMULATION"

// REGISTRO INA226
// Tamanho: 24 bytes
// timestamp_us     : 4 bytes
// pwm              : 2 bytes
// axis             : 1 byte
// reserved         : 1 byte
// current_A        : 4 bytes
// busVoltage_V     : 4 bytes
// shuntVoltage_V   : 4 bytes
// power_W          : 4 bytes

struct RegistroINA226
{
    uint32_t instante_us;

    int16_t pwm;

    uint8_t eixo;

    uint8_t reservado;

    float corrente_A;

    float tensaoBarramento_V;

    float tensaoShunt_V;

    float potencia_W;
};

// REGISTRO AD7606
// Tamanho: 24 bytes
// timestamp_us     : 4 bytes
// pwm              : 2 bytes
// raw[3]           : 6 bytes
// voltage[3]       : 12 bytes

struct RegistroAD7606
{
    uint32_t instante_us;

    int16_t pwm;

    int16_t valorBruto[3];

    float tensao[3];
};

// VERIFICAÇÃO DO TAMANHO

static_assert(
    sizeof(RegistroINA226) == 24,
    "Erro: INA226_LogRecord deve possuir 24 bytes"
);

static_assert(
    sizeof(RegistroAD7606) == 24,
    "Erro: AD7606_LogRecord deve possuir 24 bytes"
);

// REGISTRADOR DE DADOS

class RegistradorDados
{
private:

    // SD

    SdFs sd;

    FsFile arquivoINA;
    FsFile arquivoAD;
    FsFile arquivoEventos;

    // Controle manual
    FsFile arquivoControleManual;
    FsFile arquivoEventosControleManual;

    // Arquivos temporarios/finais da calibracao automatica
    FsFile arquivoCalibracaoBruto;
    FsFile arquivoInfoCalibracao;

    // Trajetoria orbital e resultados da simulacao
    FsFile arquivoEnvioTrajetoria;
    FsFile arquivoCsvTrajetoria;
    FsFile arquivoReproducaoTrajetoria;
    FsFile arquivoSimulacao;
    FsFile arquivoEventosSimulacao;

    // RING BUFFERS

    static const size_t TAMANHO_FILA_INA = 16 * 512;

    static const size_t TAMANHO_FILA_AD = 128 * 512;

    RingBuf<FsFile, TAMANHO_FILA_INA> bufferINA;

    RingBuf<FsFile, TAMANHO_FILA_AD> bufferAD;

    static const size_t TAMANHO_FILA_CONTROLE_MANUAL = 128 * 512;
    RingBuf<FsFile, TAMANHO_FILA_CONTROLE_MANUAL> bufferControleManual;

    static const size_t TAMANHO_FILA_SIMULACAO = 128 * 512;
    RingBuf<FsFile, TAMANHO_FILA_SIMULACAO> bufferSimulacao;

    // ESTADO

    bool inicializado;

    bool experimentoAtivo;
    bool controleManualAtivo;
    bool calibracaoAtiva;
    bool envioTrajetoriaAtivo;
    bool reproducaoTrajetoriaAtiva;
    bool simulacaoAtiva;

    // FUNÇÕES INTERNAS

    bool criarDiretorio();
    bool criarDiretorioControleManual();
    bool criarDiretorioSimulacao();

    bool abrirArquivos();

    bool gravarConfiguracao(
        const ConfiguracaoExperimento &configuracao
    );

    bool fecharArquivos();

    bool gravarDadosPendentes();
    bool gravarDadosControleManualPendentes();
    bool gravarDadosSimulacaoPendentes();

    bool baixarConjuntoArquivos(
        Stream &saida,
        const char *const nomesArquivos[],
        const char *const caminhosArquivos[],
        uint8_t quantidadeArquivos
    );

    void fecharArquivoEventos();

    bool fecharArquivosCalibracao();
    bool substituirArquivoComSeguranca(
        const char *caminhoTemporario,
        const char *caminhoFinal,
        const char *caminhoBackup
    );
    uint32_t calcularCRC32Calibracao(
        const uint8_t *dados,
        size_t tamanho
    ) const;

public:

    // CONSTRUTOR

    RegistradorDados();

    // INICIALIZAÇÃO

    bool iniciar();

    // LIMPEZA

    bool limparDados();

    // EXPERIMENTO

    bool iniciarExperimento(
        const ConfiguracaoExperimento &configuracao
    );

    bool finalizarExperimento();

    bool abortarExperimento();

    bool baixarArquivos(Stream &saida);

    // CONTROLE MANUAL

    bool iniciarControleManual(
        const ReferenciaCampo &referenciaInicial
    );

    bool registrarControleManual(
        const RegistroControleManual &registro
    );

    bool registrarEventoControleManual(const char *evento);

    bool finalizarControleManual();
    bool abortarControleManual(const char *motivo);
    bool controleManualEstaAtivo() const;

    bool baixarArquivosControleManual(Stream &saida);

    // TRAJETORIA ORBITAL

    bool iniciarEnvioTrajetoria(
        const CabecalhoTrajetoria &cabecalho
    );

    bool adicionarPontoTrajetoria(
        const PontoTrajetoria &ponto
    );

    bool finalizarEnvioTrajetoria(
        const InformacoesTrajetoria &informacoes
    );

    bool abortarEnvioTrajetoria();

    bool abrirReproducaoTrajetoria();
    bool lerProximoPontoTrajetoria(PontoTrajetoria &ponto);
    bool fecharReproducaoTrajetoria();

    // SIMULACAO ORBITAL

    bool iniciarSimulacao(
        const InformacoesTrajetoria &trajetoria,
        float aceleracao
    );

    bool registrarSimulacao(
        const RegistroSimulacao &registro
    );

    bool registrarEventoSimulacao(const char *evento);

    bool finalizarSimulacao();
    bool abortarSimulacao(const char *motivo);
    bool simulacaoEstaAtiva() const;

    bool baixarArquivosSimulacao(Stream &saida);

    // CALIBRACAO AUTOMATICA

    bool iniciarCalibracao(
        const SolicitacaoCalibracao &solicitacao
    );

    bool registrarPontoCalibracao(
        const RegistroPontoCalibracao &registro
    );

    bool registrarInfoCalibracao(
        const char *linha
    );

    bool finalizarCalibracao();
    bool abortarCalibracao();

    bool salvarParametrosCalibracao(
        const ParametrosCalibracao &parametros
    );

    bool carregarParametrosCalibracao(
        ParametrosCalibracao &parametros
    );

    bool calibracaoEstaAtiva() const;

    // LOG

    bool registrarINA226(
        const RegistroINA226 &registro
    );

    bool registrarAD7606(
        const RegistroAD7606 &registro
    );

    // EVENTOS

    bool registrarEvento(
        const char *evento
    );

    // MANUTENÇÃO

    void atenderGravacao();

    // STATUS

    bool estaPronto() const;

    bool experimentoEstaAtivo() const;
};

#endif
