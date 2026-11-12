// Implementacao da gravacao binaria/CSV, eventos e verificacoes de integridade no SD.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#include "RegistradorDados.h"
#include "ConfiguracoesCalibracao.h"
#include "ConfiguracoesSimulacao.h"
#include <stddef.h>

// CONFIGURAÇÃO DO SD

#define CONFIGURACAO_SD SdioConfig(FIFO_SDIO)

// CONSTRUTOR

RegistradorDados::RegistradorDados()
{
    inicializado = false;

    experimentoAtivo = false;
    controleManualAtivo = false;
    calibracaoAtiva = false;
    envioTrajetoriaAtivo = false;
    reproducaoTrajetoriaAtiva = false;
    simulacaoAtiva = false;
}

// INICIALIZAÇÃO

bool RegistradorDados::iniciar()
{
    // Inicializa o SD interno do Teensy 4.1

    if (!sd.begin(CONFIGURACAO_SD))
    {
        return false;
    }

    inicializado = true;

    return true;
}

// CRIA DIRETÓRIO

bool RegistradorDados::criarDiretorio()
{
    if (sd.exists(DIRETORIO_DADOS))
    {
        return true;
    }

    return sd.mkdir(DIRETORIO_DADOS);
}

// CRIA DIRETORIO DO CONTROLE MANUAL

bool RegistradorDados::criarDiretorioControleManual()
{
    if (!criarDiretorio())
        return false;

    if (sd.exists(DIRETORIO_CONTROLE_MANUAL))
        return true;

    return sd.mkdir(DIRETORIO_CONTROLE_MANUAL);
}

// CRIA DIRETORIO DA SIMULACAO

bool RegistradorDados::criarDiretorioSimulacao()
{
    if (!criarDiretorio())
        return false;

    if (sd.exists(DIRETORIO_SIMULACAO))
        return true;

    return sd.mkdir(DIRETORIO_SIMULACAO);
}

// ABRE ARQUIVOS

bool RegistradorDados::abrirArquivos()
{
    char caminho[64];

    // INA226

    snprintf(
        caminho,
        sizeof(caminho),
        DIRETORIO_DADOS "/ina226.bin"
    );

    if (!arquivoINA.open(
            caminho,
            O_RDWR | O_CREAT | O_TRUNC
        ))
    {
        return false;
    }

    // AD7606

    snprintf(
        caminho,
        sizeof(caminho),
        DIRETORIO_DADOS "/ad7606.bin"
    );

    if (!arquivoAD.open(
            caminho,
            O_RDWR | O_CREAT | O_TRUNC
        ))
    {
        arquivoINA.close();

        return false;
    }

    // EVENTOS

    snprintf(
        caminho,
        sizeof(caminho),
        DIRETORIO_DADOS "/events.txt"
    );

    if (!arquivoEventos.open(
            caminho,
            O_RDWR | O_CREAT | O_TRUNC
        ))
    {
        arquivoINA.close();
        arquivoAD.close();

        return false;
    }

    // Inicializa RingBuf

    bufferINA.begin(
        &arquivoINA
    );

    bufferAD.begin(
        &arquivoAD
    );

    return true;
}

// ESCREVE CONFIGURAÇÃO

bool RegistradorDados::gravarConfiguracao(
    const ConfiguracaoExperimento &configuracao
)
{
    char caminho[64];

    snprintf(
        caminho,
        sizeof(caminho),
        DIRETORIO_DADOS "/config.txt"
    );

    FsFile arquivoConfiguracao;

    if (!arquivoConfiguracao.open(
            caminho,
            O_RDWR | O_CREAT | O_TRUNC
        ))
    {
        return false;
    }

    // CABEÇALHO

    arquivoConfiguracao.println(
        "========================================"
    );

    arquivoConfiguracao.println(
        "HELMHOLTZ CAGE TEST"
    );

    arquivoConfiguracao.println(
        "========================================"
    );

    arquivoConfiguracao.println();

    // FORMATO DOS DADOS

    arquivoConfiguracao.println(
        "Data format version = 1"
    );

    arquivoConfiguracao.print(
        "INA226 record size = "
    );

    arquivoConfiguracao.println(
        sizeof(RegistroINA226)
    );

    arquivoConfiguracao.print(
        "AD7606 record size = "
    );

    arquivoConfiguracao.println(
        sizeof(RegistroAD7606)
    );

    arquivoConfiguracao.println();

    // EIXOS

    arquivoConfiguracao.print(
        "Axis mask = "
    );

    arquivoConfiguracao.println(
        configuracao.mascaraEixos
    );

    // PWM

    arquivoConfiguracao.print(
        "PWM start = "
    );

    arquivoConfiguracao.println(
        configuracao.pwmInicial
    );

    arquivoConfiguracao.print(
        "PWM stop = "
    );

    arquivoConfiguracao.println(
        configuracao.pwmFinal
    );

    arquivoConfiguracao.print(
        "PWM step = "
    );

    arquivoConfiguracao.println(
        configuracao.passoPWM
    );

    // DWELL

    arquivoConfiguracao.print(
        "Dwell = "
    );

    arquivoConfiguracao.print(
        configuracao.tempoPermanencia_ms
    );

    arquivoConfiguracao.println(
        " ms"
    );

    // LIMITES DE CORRENTE

    arquivoConfiguracao.print(
        "Current soft limit = "
    );

    arquivoConfiguracao.print(
        configuracao.limiteCorrenteOperacional,
        6
    );

    arquivoConfiguracao.println(
        " A"
    );

    arquivoConfiguracao.print(
        "Current hard limit = "
    );

    arquivoConfiguracao.print(
        configuracao.limiteCorrenteCritico,
        6
    );

    arquivoConfiguracao.println(
        " A"
    );

    // AD7606

    arquivoConfiguracao.print(
        "AD7606 oversampling = "
    );

    arquivoConfiguracao.println(
        ConfiguracoesCalibracao::SOBREAMOSTRAGEM_AD7606
    );

    arquivoConfiguracao.println(
        "AD7606 range = +/-10 V"
    );

    // INA226

    arquivoConfiguracao.print(
        "INA226 maximum current = "
    );

    arquivoConfiguracao.print(
        ConfiguracoesCalibracao::CORRENTE_MAXIMA_INA_A,
        6
    );

    arquivoConfiguracao.println(
        " A"
    );

    // Salva o valor usado por cada eixo para que o arquivo do ensaio
    // permita reproduzir exatamente a conversão de corrente.

    arquivoConfiguracao.print(
        "INA226 shunt resistance X = "
    );
    arquivoConfiguracao.print(
        ConfiguracoesCalibracao::RESISTENCIA_SHUNT_INA_X_OHM,
        6
    );
    arquivoConfiguracao.println(" ohm");

    arquivoConfiguracao.print(
        "INA226 shunt resistance Y = "
    );
    arquivoConfiguracao.print(
        ConfiguracoesCalibracao::RESISTENCIA_SHUNT_INA_Y_OHM,
        6
    );
    arquivoConfiguracao.println(" ohm");

    arquivoConfiguracao.print(
        "INA226 shunt resistance Z = "
    );
    arquivoConfiguracao.print(
        ConfiguracoesCalibracao::RESISTENCIA_SHUNT_INA_Z_OHM,
        6
    );
    arquivoConfiguracao.println(" ohm");

    arquivoConfiguracao.print(
        "INA226 averaging = "
    );

    arquivoConfiguracao.println(
        ConfiguracoesCalibracao::MEDIA_INA
    );

    arquivoConfiguracao.print(
        "INA226 shunt conversion = "
    );

    arquivoConfiguracao.print(
        ConfiguracoesCalibracao::TEMPO_CONVERSAO_SHUNT_INA_US
    );

    arquivoConfiguracao.println(
        " us"
    );

    arquivoConfiguracao.print(
        "INA226 bus conversion = "
    );

    arquivoConfiguracao.print(
        ConfiguracoesCalibracao::TEMPO_CONVERSAO_BARRAMENTO_INA_US
    );

    arquivoConfiguracao.println(
        " us"
    );

    arquivoConfiguracao.println();

    // TEMPO

    arquivoConfiguracao.print(
        "Start time (micros) = "
    );

    arquivoConfiguracao.println(
        micros()
    );

    arquivoConfiguracao.close();

    return true;
}

// INICIA EXPERIMENTO

bool RegistradorDados::iniciarExperimento(
    const ConfiguracaoExperimento &configuracao
)
{
    if (!inicializado)
    {
        return false;
    }

    if (experimentoAtivo || controleManualAtivo || calibracaoAtiva ||
        simulacaoAtiva || envioTrajetoriaAtivo || reproducaoTrajetoriaAtiva)
    {
        return false;
    }

    // Cria diretório

    if (!criarDiretorio())
    {
        return false;
    }

    // Abre arquivos

    if (!abrirArquivos())
    {
        fecharArquivos();

        return false;
    }

    // Grava configuração

    if (!gravarConfiguracao(configuracao))
    {
        fecharArquivos();

        return false;
    }

    // O experimento passa a ser considerado ativo
    // antes do primeiro evento.

    experimentoAtivo = true;

    // Evento

    if (!registrarEvento(
            "EXPERIMENT_START"
        ))
    {
        fecharArquivos();

        experimentoAtivo = false;

        return false;
    }

    return true;
}

// LOG INA226

bool RegistradorDados::registrarINA226(
    const RegistroINA226 &registro
)
{
    if (!experimentoAtivo)
    {
        return false;
    }

    // Coloca registro no RingBuf

    size_t escritos =
        bufferINA.write(
            &registro,
            sizeof(registro)
        );

    // Verifica overflow

    if (
        escritos != sizeof(registro)
    )
    {
        registrarEvento(
            "INA226_BUFFER_OVERFLOW"
        );

        return false;
    }

    return true;
}

// LOG AD7606

bool RegistradorDados::registrarAD7606(
    const RegistroAD7606 &registro
)
{
    if (!experimentoAtivo)
    {
        return false;
    }

    // Coloca registro no RingBuf

    size_t escritos =
        bufferAD.write(
            &registro,
            sizeof(registro)
        );

    // Verifica overflow

    if (
        escritos != sizeof(registro)
    )
    {
        registrarEvento(
            "AD7606_BUFFER_OVERFLOW"
        );

        return false;
    }

    return true;
}

// ATENDIMENTO DE GRAVACAO
// Esta função deve ser chamada constantemente pelo programa
// principal enquanto o experimento estiver rodando.
// Os dados são acumulados nos RingBufs e enviados ao SD em
// blocos para reduzir o custo de escrita.

void RegistradorDados::atenderGravacao()
{
    if (experimentoAtivo)
    {
        // INA226

        if (bufferINA.bytesUsed() >= 512 && !arquivoINA.isBusy())
            bufferINA.writeOut(512);

        // AD7606

        if (bufferAD.bytesUsed() >= 512 && !arquivoAD.isBusy())
            bufferAD.writeOut(512);
    }

    if (controleManualAtivo)
    {
        if (bufferControleManual.bytesUsed() >= 512 &&
            !arquivoControleManual.isBusy())
        {
            bufferControleManual.writeOut(512);
        }
    }

    if (simulacaoAtiva)
    {
        if (bufferSimulacao.bytesUsed() >= 512 &&
            !arquivoSimulacao.isBusy())
        {
            bufferSimulacao.writeOut(512);
        }
    }
}

// ESCREVE DADOS PENDENTES

bool RegistradorDados::gravarDadosPendentes()
{
    uint32_t inicio =
        micros();

    // Tenta esvaziar os buffers

    while (
        bufferINA.bytesUsed() > 0
        ||
        bufferAD.bytesUsed() > 0
    )
    {
        atenderGravacao();

        // Restante INA226

        if (
            bufferINA.bytesUsed() > 0
            &&
            bufferINA.bytesUsed() < 512
            &&
            !arquivoINA.isBusy()
        )
        {
            bufferINA.writeOut(
                bufferINA.bytesUsed()
            );
        }

        // Restante AD7606

        if (
            bufferAD.bytesUsed() > 0
            &&
            bufferAD.bytesUsed() < 512
            &&
            !arquivoAD.isBusy()
        )
        {
            bufferAD.writeOut(
                bufferAD.bytesUsed()
            );
        }

        // Tempo limite de 5 segundos

        if (
            (uint32_t)(micros() - inicio)
            >
            5000000UL
        )
        {
            return false;
        }
    }

    // Sincroniza RingBuf

    if (!bufferINA.sync())
    {
        return false;
    }

    if (!bufferAD.sync())
    {
        return false;
    }

    // Sincroniza arquivos

    if (!arquivoINA.sync())
    {
        return false;
    }

    if (!arquivoAD.sync())
    {
        return false;
    }

    return true;
}

// ESCREVE DADOS PENDENTES DO CONTROLE MANUAL

bool RegistradorDados::gravarDadosControleManualPendentes()
{
    if (!arquivoControleManual.isOpen())
        return false;

    const uint32_t inicio_ms = millis();

    while (bufferControleManual.bytesUsed() > 0)
    {
        if (!arquivoControleManual.isBusy())
        {
            size_t quantidade = bufferControleManual.bytesUsed();
            if (quantidade > 512) quantidade = 512;

            bufferControleManual.writeOut(quantidade);
        }

        if ((uint32_t)(millis() - inicio_ms) > 5000UL)
            return false;
    }

    if (!bufferControleManual.sync())
        return false;

    return arquivoControleManual.sync();
}

// ESCREVE DADOS PENDENTES DA SIMULACAO

bool RegistradorDados::gravarDadosSimulacaoPendentes()
{
    if (!arquivoSimulacao.isOpen())
        return false;

    const uint32_t inicio_ms = millis();

    while (bufferSimulacao.bytesUsed() > 0)
    {
        if (!arquivoSimulacao.isBusy())
        {
            size_t quantidade = bufferSimulacao.bytesUsed();
            if (quantidade > 512) quantidade = 512;

            bufferSimulacao.writeOut(quantidade);
        }

        if ((uint32_t)(millis() - inicio_ms) > 5000UL)
            return false;
    }

    if (!bufferSimulacao.sync())
        return false;

    return arquivoSimulacao.sync();
}

// FINALIZA EXPERIMENTO

bool RegistradorDados::finalizarExperimento()
{
    if (!experimentoAtivo)
    {
        return false;
    }

    // Esvazia buffers

    if (!gravarDadosPendentes())
    {
        registrarEvento(
            "SD_FLUSH_TIMEOUT"
        );

        fecharArquivos();

        experimentoAtivo = false;

        return false;
    }

    // Evento de conclusão

    if (!registrarEvento(
            "EXPERIMENT_COMPLETE"
        ))
    {
        fecharArquivos();

        experimentoAtivo = false;

        return false;
    }

    // Garante escrita dos arquivos

    arquivoINA.sync();

    arquivoAD.sync();

    arquivoEventos.sync();

    // Fecha arquivos

    fecharArquivos();

    experimentoAtivo = false;

    return true;
}

// ABORTA EXPERIMENTO
// Utilizado em condições de FALHA, por exemplo:
// - sobrecorrente crítica
// - erro de leitura do AD7606
// - falha de gravação
// O experimento não é marcado como CONCLUIDO.

bool RegistradorDados::abortarExperimento()
{
    if (!experimentoAtivo)
    {
        return false;
    }

    // Primeiro registra o evento

    registrarEvento(
        "EXPERIMENT_ABORTED"
    );

    // Tenta salvar os dados que estão no RingBuf

    bool gravacaoOK =
        gravarDadosPendentes();

    // Garante que os eventos sejam gravados

    arquivoEventos.sync();

    // Fecha arquivos

    fecharArquivos();

    experimentoAtivo = false;

    return gravacaoOK;
}

// FECHA ARQUIVOS

bool RegistradorDados::fecharArquivos()
{
    if (arquivoINA.isOpen())
    {
        arquivoINA.close();
    }

    if (arquivoAD.isOpen())
    {
        arquivoAD.close();
    }

    if (arquivoEventos.isOpen())
    {
        arquivoEventos.close();
    }

    return true;
}

// EVENTO

bool RegistradorDados::registrarEvento(
    const char *evento
)
{
    if (!arquivoEventos.isOpen())
    {
        return false;
    }

    arquivoEventos.print(
        micros()
    );

    arquivoEventos.print(
        " : "
    );

    arquivoEventos.println(
        evento
    );

    return true;
}

// LIMPA DADOS
// Remove somente os arquivos do experimento anterior.
// O diretório /TG_DATA permanece.

bool RegistradorDados::limparDados()
{
    if (!inicializado)
    {
        return false;
    }

    if (experimentoAtivo || controleManualAtivo || calibracaoAtiva ||
        simulacaoAtiva || envioTrajetoriaAtivo || reproducaoTrajetoriaAtiva)
    {
        return false;
    }

    char caminho[64];

    // INA226

    snprintf(
        caminho,
        sizeof(caminho),
        DIRETORIO_DADOS "/ina226.bin"
    );

    if (sd.exists(caminho))
    {
        if (!sd.remove(caminho))
        {
            return false;
        }
    }

    // AD7606

    snprintf(
        caminho,
        sizeof(caminho),
        DIRETORIO_DADOS "/ad7606.bin"
    );

    if (sd.exists(caminho))
    {
        if (!sd.remove(caminho))
        {
            return false;
        }
    }

    // CONFIG

    snprintf(
        caminho,
        sizeof(caminho),
        DIRETORIO_DADOS "/config.txt"
    );

    if (sd.exists(caminho))
    {
        if (!sd.remove(caminho))
        {
            return false;
        }
    }

    // EVENTOS

    snprintf(
        caminho,
        sizeof(caminho),
        DIRETORIO_DADOS "/events.txt"
    );

    if (sd.exists(caminho))
    {
        if (!sd.remove(caminho))
        {
            return false;
        }
    }

    return true;
}

// STATUS

bool RegistradorDados::estaPronto() const
{
    return inicializado;
}

bool RegistradorDados::experimentoEstaAtivo() const
{
    return experimentoAtivo;
}

// ENVIO DOS ARQUIVOS AO COMPUTADOR
// Envia um conjunto de arquivos pela interface Serial.
// A transferencia e usada somente com o sistema fora de aquisicao.
// O receptor deve:
// 1. Ler FILE_BEGIN
// 2. Obter o tamanho
// 3. Ler exatamente SIZE bytes
// O conteúdo dos arquivos .bin pode conter qualquer byte.

bool RegistradorDados::baixarConjuntoArquivos(
    Stream &saida,
    const char *const nomesArquivos[],
    const char *const caminhosArquivos[],
    uint8_t quantidadeArquivos
)
{
    // CONFIGURAÇÃO DA TRANSFERÊNCIA

    const uint32_t TAMANHO_BLOCO = 4096;
    const uint32_t TEMPO_LIMITE_CONFIRMACAO_MS = 5000;
    const uint8_t MAXIMO_TENTATIVAS = 5;

    // CRC32
    // Polinômio padrão:
    // 0xEDB88320
    // Mesmo algoritmo será utilizado no MATLAB.

    auto calcularCRC32 =
        [](const uint8_t *dados, size_t tamanho) -> uint32_t
    {
        static uint32_t tabelaCRC[256];
        static bool tabelaCRCInicializada = false;

        if (!tabelaCRCInicializada)
        {
            for (uint32_t i = 0; i < 256; i++)
            {
                uint32_t crc = i;

                for (uint8_t j = 0; j < 8; j++)
                {
                    if (crc & 1)
                    {
                        crc =
                            (crc >> 1) ^
                            0xEDB88320UL;
                    }
                    else
                    {
                        crc >>= 1;
                    }
                }

                tabelaCRC[i] = crc;
            }

            tabelaCRCInicializada = true;
        }

        uint32_t crc = 0xFFFFFFFFUL;

        for (size_t i = 0; i < tamanho; i++)
        {
            uint8_t indice =
                (uint8_t)((crc ^ dados[i]) & 0xFF);

            crc =
                (crc >> 8) ^
                tabelaCRC[indice];
        }

        return crc ^ 0xFFFFFFFFUL;
    };

    // Aguarda ACK/NACK do MATLAB
    // Retorno:
    //   1 = ACK
    //   0 = NACK
    //  -1 = timeout

    auto aguardarConfirmacao =
        [&](uint32_t blocoEsperado) -> int
    {
        char linha[96];
        size_t tamanho = 0;

        uint32_t tempoInicial = millis();

        while ((millis() - tempoInicial) < TEMPO_LIMITE_CONFIRMACAO_MS)
        {
            while (saida.available())
            {
                int valor = saida.read();

                if (valor < 0)
                    break;

                char c = (char)valor;

                if (c == '\n')
                {
                    linha[tamanho] = '\0';

                    String mensagem = String(linha);
                    mensagem.trim();

                    // ACK

                    if (mensagem.startsWith("ACK;INDEX="))
                    {
                        String textoIndice =
                            mensagem.substring(10);

                        uint32_t indice =
                            (uint32_t)textoIndice.toInt();

                        if (indice == blocoEsperado)
                        {
                            return 1;
                        }
                    }

                    // NACK

                    if (mensagem.startsWith("NACK;INDEX="))
                    {
                        String textoIndice =
                            mensagem.substring(11);

                        uint32_t indice =
                            (uint32_t)textoIndice.toInt();

                        if (indice == blocoEsperado)
                        {
                            return 0;
                        }
                    }

                    tamanho = 0;
                }
                else if (c != '\r')
                {
                    if (tamanho < sizeof(linha) - 1)
                    {
                        linha[tamanho++] = c;
                    }
                    else
                    {
                        // Linha inválida/muito grande.
                        tamanho = 0;
                    }
                }
            }

            delay(1);
        }

        return -1;
    };

    // Inicio do envio ao computador

    saida.print("DOWNLOAD_BEGIN;FILES=");
    saida.print(quantidadeArquivos);
    saida.print(";BLOCK_SIZE=");
    saida.println(TAMANHO_BLOCO);

    saida.flush();

    // MEMORIA TEMPORARIA DO ENVIO

    static uint8_t bufferTemporario[TAMANHO_BLOCO];

    // ENVIO DOS ARQUIVOS AO COMPUTADOR

    for (uint8_t indiceArquivo = 0;
         indiceArquivo < quantidadeArquivos;
         indiceArquivo++)
    {
        const char *nomeArquivo =
            nomesArquivos[indiceArquivo];

        const char *caminhoArquivo =
            caminhosArquivos[indiceArquivo];

        // Abre arquivo

        FsFile arquivo = sd.open(
            caminhoArquivo,
            O_RDONLY
        );

        if (!arquivo)
        {
            saida.print("DOWNLOAD_ERROR;FILE=");
            saida.print(nomeArquivo);
            saida.println(";REASON=OPEN_FAILED");
            saida.flush();

            return false;
        }

        uint32_t tamanhoArquivo =
            (uint32_t)arquivo.size();

        uint32_t quantidadeBlocos =
            (tamanhoArquivo + TAMANHO_BLOCO - 1) /
            TAMANHO_BLOCO;

        // Informa início do arquivo

        saida.print("FILE_BEGIN;NAME=");
        saida.print(nomeArquivo);
        saida.print(";SIZE=");
        saida.print(tamanhoArquivo);
        saida.print(";BLOCK_SIZE=");
        saida.print(TAMANHO_BLOCO);
        saida.print(";BLOCKS=");
        saida.println(quantidadeBlocos);

        saida.flush();

        // Envia todos os blocos

        for (uint32_t indiceBloco = 0;
             indiceBloco < quantidadeBlocos;
             indiceBloco++)
        {
            uint32_t offset =
                indiceBloco * TAMANHO_BLOCO;

            uint32_t bytesRestantes =
                tamanhoArquivo - offset;

            uint32_t tamanhoBlocoAtual =
                (bytesRestantes > TAMANHO_BLOCO)
                ? TAMANHO_BLOCO
                : bytesRestantes;

            bool blocoTransferido = false;

            // Retransmissões

            for (uint8_t tentativa = 0;
                 tentativa < MAXIMO_TENTATIVAS;
                 tentativa++)
            {
                // Posiciona arquivo

                if (!arquivo.seek(offset))
                {
                    arquivo.close();

                    saida.print(
                        "DOWNLOAD_ERROR;FILE=");
                    saida.print(nomeArquivo);
                    saida.print(
                        ";BLOCK=");
                    saida.print(indiceBloco);
                    saida.println(
                        ";REASON=SEEK_FAILED");

                    saida.flush();

                    return false;
                }

                // Lê bloco do SD

                int bytesLidos =
                    arquivo.read(
                        bufferTemporario,
                        tamanhoBlocoAtual
                    );

                if (bytesLidos !=
                    (int)tamanhoBlocoAtual)
                {
                    arquivo.close();

                    saida.print(
                        "DOWNLOAD_ERROR;FILE=");
                    saida.print(nomeArquivo);
                    saida.print(
                        ";BLOCK=");
                    saida.print(indiceBloco);
                    saida.println(
                        ";REASON=READ_FAILED");

                    saida.flush();

                    return false;
                }

                // Calcula CRC

                uint32_t crc =
                    calcularCRC32(
                        bufferTemporario,
                        tamanhoBlocoAtual
                    );

                char textoCRC[9];

                snprintf(
                    textoCRC,
                    sizeof(textoCRC),
                    "%08lX",
                    (unsigned long)crc
                );

                // Cabeçalho do bloco

                saida.print(
                    "BLOCK_BEGIN;INDEX=");

                saida.print(
                    indiceBloco);

                saida.print(
                    ";SIZE=");

                saida.print(
                    tamanhoBlocoAtual);

                saida.print(
                    ";CRC=");

                saida.println(
                    textoCRC);

                // Dados binários

                size_t escritos =
                    saida.write(
                        bufferTemporario,
                        tamanhoBlocoAtual
                    );

                if (escritos != tamanhoBlocoAtual)
                {
                    arquivo.close();

                    saida.print(
                        "DOWNLOAD_ERROR;FILE=");
                    saida.print(nomeArquivo);
                    saida.print(
                        ";BLOCK=");
                    saida.print(indiceBloco);
                    saida.println(
                        ";REASON=WRITE_FAILED");

                    saida.flush();

                    return false;
                }

                // Garante que todos os bytes do bloco saíram
                // antes de esperar o ACK.
                saida.flush();

                // Aguarda ACK/NACK

                int confirmacao =
                    aguardarConfirmacao(indiceBloco);

                // ACK

                if (confirmacao == 1)
                {
                    blocoTransferido = true;
                    break;
                }

                // NACK ou timeout
                // O bloco será retransmitido.

                if (confirmacao == 0)
                {
                    // NACK: retransmite imediatamente.
                    continue;
                }

                if (confirmacao < 0)
                {
                    // Tempo limite: retransmite.
                    continue;
                }
            }

            // Falha após MAX_RETRIES

            if (!blocoTransferido)
            {
                arquivo.close();

                saida.print(
                    "DOWNLOAD_ERROR;FILE=");

                saida.print(
                    nomeArquivo);

                saida.print(
                    ";BLOCK=");

                saida.print(
                    indiceBloco);

                saida.println(
                    ";REASON=MAX_RETRIES");

                saida.flush();

                return false;
            }
        }

        // Fecha arquivo no SD

        arquivo.close();

        // Indica fim do arquivo

        saida.print(
            "FILE_END;NAME=");

        saida.println(
            nomeArquivo);

        saida.flush();

        // Aguarda confirmação de que MATLAB terminou de gravar

        bool arquivoConfirmado = false;

        uint32_t inicioConfirmacao =
            millis();

        char linha[128];
        size_t tamanho = 0;

        while (
            (millis() - inicioConfirmacao) <
            TEMPO_LIMITE_CONFIRMACAO_MS)
        {
            while (saida.available())
            {
                int valor =
                    saida.read();

                if (valor < 0)
                    break;

                char c =
                    (char)valor;

                if (c == '\n')
                {
                    linha[tamanho] = '\0';

                    String mensagem =
                        String(linha);

                    mensagem.trim();

                    String mensagemEsperada =
                        "FILE_ACK;NAME=";

                    mensagemEsperada += nomeArquivo;

                    if (mensagem == mensagemEsperada)
                    {
                        arquivoConfirmado = true;
                        break;
                    }

                    tamanho = 0;
                }
                else if (c != '\r')
                {
                    if (tamanho < sizeof(linha) - 1)
                    {
                        linha[tamanho++] = c;
                    }
                    else
                    {
                        tamanho = 0;
                    }
                }
            }

            if (arquivoConfirmado)
                break;

            delay(1);
        }

        if (!arquivoConfirmado)
        {
            saida.print(
                "DOWNLOAD_ERROR;FILE=");

            saida.print(
                nomeArquivo);

            saida.println(
                ";REASON=FILE_ACK_TIMEOUT");

            saida.flush();

            return false;
        }
    }

    // ENVIO AO COMPUTADOR CONCLUIDO

    saida.println("DOWNLOAD_COMPLETE");

    saida.flush();

    return true;
}

bool RegistradorDados::baixarArquivos(Stream &saida)
{
    const char *const nomes[] =
    {
        "config.txt",
        "events.txt",
        "ina226.bin",
        "ad7606.bin"
    };

    const char *const caminhos[] =
    {
        DIRETORIO_DADOS "/config.txt",
        DIRETORIO_DADOS "/events.txt",
        DIRETORIO_DADOS "/ina226.bin",
        DIRETORIO_DADOS "/ad7606.bin"
    };

    return baixarConjuntoArquivos(
        saida,
        nomes,
        caminhos,
        sizeof(nomes) / sizeof(nomes[0])
    );
}

bool RegistradorDados::baixarArquivosControleManual(Stream &saida)
{
    if (controleManualAtivo)
        return false;

    const char *const nomes[] =
    {
        "control_info.txt",
        "control_events.txt",
        "control.bin"
    };

    const char *const caminhos[] =
    {
        DIRETORIO_CONTROLE_MANUAL "/control_info.txt",
        DIRETORIO_CONTROLE_MANUAL "/control_events.txt",
        DIRETORIO_CONTROLE_MANUAL "/control.bin"
    };

    return baixarConjuntoArquivos(
        saida,
        nomes,
        caminhos,
        sizeof(nomes) / sizeof(nomes[0])
    );
}

bool RegistradorDados::baixarArquivosSimulacao(Stream &saida)
{
    if (simulacaoAtiva || envioTrajetoriaAtivo ||
        reproducaoTrajetoriaAtiva)
    {
        return false;
    }

    const char *const nomes[] =
    {
        "trajectory.bin",
        "trajectory.csv",
        "trajectory_info.txt",
        "simulation.bin",
        "simulation_info.txt",
        "simulation_events.txt"
    };

    const char *const caminhos[] =
    {
        DIRETORIO_SIMULACAO "/trajectory.bin",
        DIRETORIO_SIMULACAO "/trajectory.csv",
        DIRETORIO_SIMULACAO "/trajectory_info.txt",
        DIRETORIO_SIMULACAO "/simulation.bin",
        DIRETORIO_SIMULACAO "/simulation_info.txt",
        DIRETORIO_SIMULACAO "/simulation_events.txt"
    };

    return baixarConjuntoArquivos(
        saida,
        nomes,
        caminhos,
        sizeof(nomes) / sizeof(nomes[0])
    );
}

// CONTROLE MANUAL - CAMINHOS

static const char *ARQUIVO_DADOS_CONTROLE_MANUAL =
    DIRETORIO_CONTROLE_MANUAL "/control.bin";
static const char *ARQUIVO_INFO_CONTROLE_MANUAL =
    DIRETORIO_CONTROLE_MANUAL "/control_info.txt";
static const char *ARQUIVO_EVENTOS_CONTROLE_MANUAL =
    DIRETORIO_CONTROLE_MANUAL "/control_events.txt";

// INICIA LOG DO CONTROLE MANUAL

bool RegistradorDados::iniciarControleManual(
    const ReferenciaCampo &referenciaInicial
)
{
    if (!inicializado || experimentoAtivo || controleManualAtivo ||
        calibracaoAtiva || simulacaoAtiva || envioTrajetoriaAtivo ||
        reproducaoTrajetoriaAtiva)
    {
        return false;
    }

    if (!criarDiretorioControleManual())
        return false;

    if (!arquivoControleManual.open(
            ARQUIVO_DADOS_CONTROLE_MANUAL,
            O_RDWR | O_CREAT | O_TRUNC))
    {
        return false;
    }

    if (!arquivoEventosControleManual.open(
            ARQUIVO_EVENTOS_CONTROLE_MANUAL,
            O_RDWR | O_CREAT | O_TRUNC))
    {
        arquivoControleManual.close();
        return false;
    }

    bufferControleManual.begin(&arquivoControleManual);

    FsFile arquivoInfo;

    if (!arquivoInfo.open(
            ARQUIVO_INFO_CONTROLE_MANUAL,
            O_RDWR | O_CREAT | O_TRUNC))
    {
        arquivoControleManual.close();
        arquivoEventosControleManual.close();
        return false;
    }

    arquivoInfo.print("FORMAT_VERSION=");
    arquivoInfo.println(ConfiguracoesControle::VERSAO_FORMATO_REGISTRO_CONTROLE_MANUAL);
    arquivoInfo.print("RECORD_SIZE=");
    arquivoInfo.println(sizeof(RegistroControleManual));
    arquivoInfo.print("CURRENT_LOOP_PERIOD_US=");
    arquivoInfo.println(0); // Malha de corrente desativada
    arquivoInfo.print("FIELD_ACQUISITION_PERIOD_US=");
    arquivoInfo.println(ConfiguracoesControle::PERIODO_AQUISICAO_CAMPO_US);
    arquivoInfo.print("FIELD_LOOP_PERIOD_US=");
    arquivoInfo.println(ConfiguracoesControle::PERIODO_MALHA_CAMPO_US);
    arquivoInfo.print("TELEMETRY_PERIOD_MS=");
    arquivoInfo.println(ConfiguracoesControle::PERIODO_TELEMETRIA_MANUAL_MS);
    arquivoInfo.print("AXIS_MASK=");
    arquivoInfo.println(ConfiguracoesControle::MASCARA_EIXOS);
    arquivoInfo.print("INITIAL_BX_uT=");
    arquivoInfo.println(referenciaInicial.B_uT[0], 8);
    arquivoInfo.print("INITIAL_BY_uT=");
    arquivoInfo.println(referenciaInicial.B_uT[1], 8);
    arquivoInfo.print("INITIAL_BZ_uT=");
    arquivoInfo.println(referenciaInicial.B_uT[2], 8);

    bool sucesso = !arquivoInfo.getWriteError() && arquivoInfo.sync();
    arquivoInfo.close();

    if (!sucesso)
    {
        arquivoControleManual.close();
        arquivoEventosControleManual.close();
        return false;
    }

    controleManualAtivo = true;

    if (!registrarEventoControleManual("CONTROL_START"))
    {
        arquivoControleManual.close();
        arquivoEventosControleManual.close();
        controleManualAtivo = false;
        return false;
    }

    char evento[128];
    snprintf(
        evento,
        sizeof(evento),
        "FIELD_REF;BX=%.6f;BY=%.6f;BZ=%.6f",
        referenciaInicial.B_uT[0],
        referenciaInicial.B_uT[1],
        referenciaInicial.B_uT[2]
    );

    if (!registrarEventoControleManual(evento))
    {
        arquivoControleManual.close();
        arquivoEventosControleManual.close();
        controleManualAtivo = false;
        return false;
    }

    return true;
}

bool RegistradorDados::registrarControleManual(
    const RegistroControleManual &registro
)
{
    if (!controleManualAtivo)
        return false;

    const size_t escritos =
        bufferControleManual.write(&registro, sizeof(registro));

    if (escritos != sizeof(registro))
    {
        registrarEventoControleManual("CONTROL_BUFFER_OVERFLOW");
        return false;
    }

    return true;
}

bool RegistradorDados::registrarEventoControleManual(const char *evento)
{
    if (!controleManualAtivo ||
        !arquivoEventosControleManual.isOpen() ||
        evento == nullptr)
    {
        return false;
    }

    arquivoEventosControleManual.print(millis());
    arquivoEventosControleManual.print(" : ");
    arquivoEventosControleManual.println(evento);

    return !arquivoEventosControleManual.getWriteError();
}

bool RegistradorDados::finalizarControleManual()
{
    if (!controleManualAtivo)
        return false;

    registrarEventoControleManual("CONTROL_COMPLETE");

    const bool gravacaoOK =
        gravarDadosControleManualPendentes();

    bool eventoOK = true;

    if (arquivoEventosControleManual.isOpen())
    {
        eventoOK = arquivoEventosControleManual.sync();
        arquivoEventosControleManual.close();
    }

    if (arquivoControleManual.isOpen())
        arquivoControleManual.close();

    controleManualAtivo = false;
    return gravacaoOK && eventoOK;
}

bool RegistradorDados::abortarControleManual(const char *motivo)
{
    if (!controleManualAtivo)
        return false;

    if (motivo != nullptr)
    {
        char evento[128];
        snprintf(
            evento,
            sizeof(evento),
            "CONTROL_ABORT;REASON=%s",
            motivo
        );
        registrarEventoControleManual(evento);
    }

    const bool gravacaoOK =
        gravarDadosControleManualPendentes();

    bool eventoOK = true;

    if (arquivoEventosControleManual.isOpen())
    {
        eventoOK = arquivoEventosControleManual.sync();
        arquivoEventosControleManual.close();
    }

    if (arquivoControleManual.isOpen())
        arquivoControleManual.close();

    controleManualAtivo = false;
    return gravacaoOK && eventoOK;
}

bool RegistradorDados::controleManualEstaAtivo() const
{
    return controleManualAtivo;
}

// SIMULACAO ORBITAL - CAMINHOS

static const char *ARQUIVO_TRAJETORIA_BIN_TEMP = DIRETORIO_SIMULACAO "/trajectory.tmp";
static const char *ARQUIVO_TRAJETORIA_BIN_FINAL = DIRETORIO_SIMULACAO "/trajectory.bin";
static const char *ARQUIVO_TRAJETORIA_BIN_RESERVA = DIRETORIO_SIMULACAO "/trajectory.bak";

static const char *ARQUIVO_TRAJETORIA_CSV_TEMP = DIRETORIO_SIMULACAO "/trajectory_csv.tmp";
static const char *ARQUIVO_TRAJETORIA_CSV_FINAL = DIRETORIO_SIMULACAO "/trajectory.csv";
static const char *ARQUIVO_TRAJETORIA_CSV_RESERVA = DIRETORIO_SIMULACAO "/trajectory_csv.bak";

static const char *ARQUIVO_INFO_TRAJETORIA = DIRETORIO_SIMULACAO "/trajectory_info.txt";
static const char *ARQUIVO_DADOS_SIMULACAO = DIRETORIO_SIMULACAO "/simulation.bin";
static const char *ARQUIVO_INFO_SIMULACAO = DIRETORIO_SIMULACAO "/simulation_info.txt";
static const char *ARQUIVO_EVENTOS_SIMULACAO = DIRETORIO_SIMULACAO "/simulation_events.txt";

// INICIA ENVIO DA TRAJETORIA

bool RegistradorDados::iniciarEnvioTrajetoria(
    const CabecalhoTrajetoria &cabecalho
)
{
    if (!inicializado || experimentoAtivo || controleManualAtivo || calibracaoAtiva ||
        simulacaoAtiva || envioTrajetoriaAtivo || reproducaoTrajetoriaAtiva)
    {
        return false;
    }

    if (cabecalho.quantidadePontos < 2 || cabecalho.id[0] == '\0')
        return false;

    if (!criarDiretorioSimulacao())
        return false;

    if (sd.exists(ARQUIVO_TRAJETORIA_BIN_TEMP)) sd.remove(ARQUIVO_TRAJETORIA_BIN_TEMP);
    if (sd.exists(ARQUIVO_TRAJETORIA_CSV_TEMP)) sd.remove(ARQUIVO_TRAJETORIA_CSV_TEMP);

    if (!arquivoEnvioTrajetoria.open(
            ARQUIVO_TRAJETORIA_BIN_TEMP,
            O_RDWR | O_CREAT | O_TRUNC))
    {
        return false;
    }

    if (!arquivoCsvTrajetoria.open(
            ARQUIVO_TRAJETORIA_CSV_TEMP,
            O_RDWR | O_CREAT | O_TRUNC))
    {
        arquivoEnvioTrajetoria.close();
        sd.remove(ARQUIVO_TRAJETORIA_BIN_TEMP);
        return false;
    }

    arquivoCsvTrajetoria.println("index,time_s,Bx_ref_uT,By_ref_uT,Bz_ref_uT");

    envioTrajetoriaAtivo = true;
    return true;
}

// GRAVA PONTO DA TRAJETORIA

bool RegistradorDados::adicionarPontoTrajetoria(
    const PontoTrajetoria &ponto
)
{
    if (!envioTrajetoriaAtivo ||
        !arquivoEnvioTrajetoria.isOpen() ||
        !arquivoCsvTrajetoria.isOpen())
    {
        return false;
    }

    if (arquivoEnvioTrajetoria.write(&ponto, sizeof(ponto)) != sizeof(ponto))
        return false;

    arquivoCsvTrajetoria.print(ponto.indice);
    arquivoCsvTrajetoria.print(',');
    arquivoCsvTrajetoria.print(ponto.tempo_s, 6);

    for (uint8_t eixo = 0; eixo < 3; eixo++)
    {
        arquivoCsvTrajetoria.print(',');
        arquivoCsvTrajetoria.print(ponto.B_uT[eixo], 8);
    }

    arquivoCsvTrajetoria.println();

    return !arquivoEnvioTrajetoria.getWriteError() &&
           !arquivoCsvTrajetoria.getWriteError();
}

// FINALIZA ENVIO DA TRAJETORIA

bool RegistradorDados::finalizarEnvioTrajetoria(
    const InformacoesTrajetoria &informacoes
)
{
    if (!envioTrajetoriaAtivo)
        return false;

    bool sucesso = true;

    const uint64_t tamanhoEsperado =
        (uint64_t)informacoes.quantidadePontos * sizeof(PontoTrajetoria);

    if (arquivoEnvioTrajetoria.isOpen())
    {
        if (!arquivoEnvioTrajetoria.sync()) sucesso = false;

        if ((uint64_t)arquivoEnvioTrajetoria.size() != tamanhoEsperado)
            sucesso = false;

        arquivoEnvioTrajetoria.close();
    }
    else
    {
        sucesso = false;
    }

    if (arquivoCsvTrajetoria.isOpen())
    {
        if (!arquivoCsvTrajetoria.sync()) sucesso = false;
        arquivoCsvTrajetoria.close();
    }

    envioTrajetoriaAtivo = false;

    if (!sucesso)
    {
        sd.remove(ARQUIVO_TRAJETORIA_BIN_TEMP);
        sd.remove(ARQUIVO_TRAJETORIA_CSV_TEMP);
        return false;
    }

    if (!substituirArquivoComSeguranca(
            ARQUIVO_TRAJETORIA_BIN_TEMP,
            ARQUIVO_TRAJETORIA_BIN_FINAL,
            ARQUIVO_TRAJETORIA_BIN_RESERVA))
    {
        sd.remove(ARQUIVO_TRAJETORIA_CSV_TEMP);
        return false;
    }

    if (!substituirArquivoComSeguranca(
            ARQUIVO_TRAJETORIA_CSV_TEMP,
            ARQUIVO_TRAJETORIA_CSV_FINAL,
            ARQUIVO_TRAJETORIA_CSV_RESERVA))
    {
        return false;
    }

    FsFile arquivoInfo;

    if (!arquivoInfo.open(
            ARQUIVO_INFO_TRAJETORIA,
            O_RDWR | O_CREAT | O_TRUNC))
    {
        return false;
    }

    arquivoInfo.print("FORMAT_VERSION=");
    arquivoInfo.println(ConfiguracoesSimulacao::VERSAO_FORMATO_TRAJETORIA);
    arquivoInfo.print("ID=");
    arquivoInfo.println(informacoes.id);
    arquivoInfo.print("POINTS=");
    arquivoInfo.println(informacoes.quantidadePontos);
    arquivoInfo.print("FIRST_TIME_S=");
    arquivoInfo.println(informacoes.primeiroTempo_s, 6);
    arquivoInfo.print("LAST_TIME_S=");
    arquivoInfo.println(informacoes.ultimoTempo_s, 6);
    arquivoInfo.print("MIN_DT_S=");
    arquivoInfo.println(informacoes.menorIntervalo_s, 9);
    arquivoInfo.print("MAX_ALLOWED_ACCELERATION=");
    arquivoInfo.println(
        ConfiguracoesSimulacao::TAXA_MAXIMA_REFERENCIA_HZ * informacoes.menorIntervalo_s,
        6
    );
    arquivoInfo.print("MAX_REFERENCE_RATE_HZ=");
    arquivoInfo.println(ConfiguracoesSimulacao::TAXA_MAXIMA_REFERENCIA_HZ, 3);

    sucesso = !arquivoInfo.getWriteError() && arquivoInfo.sync();
    arquivoInfo.close();

    return sucesso;
}

// ABORTA ENVIO DA TRAJETORIA

bool RegistradorDados::abortarEnvioTrajetoria()
{
    if (arquivoEnvioTrajetoria.isOpen())
        arquivoEnvioTrajetoria.close();

    if (arquivoCsvTrajetoria.isOpen())
        arquivoCsvTrajetoria.close();

    if (sd.exists(ARQUIVO_TRAJETORIA_BIN_TEMP)) sd.remove(ARQUIVO_TRAJETORIA_BIN_TEMP);
    if (sd.exists(ARQUIVO_TRAJETORIA_CSV_TEMP)) sd.remove(ARQUIVO_TRAJETORIA_CSV_TEMP);

    envioTrajetoriaAtivo = false;
    return true;
}

// REPRODUCAO DA TRAJETORIA

bool RegistradorDados::abrirReproducaoTrajetoria()
{
    if (!inicializado || controleManualAtivo || envioTrajetoriaAtivo ||
        reproducaoTrajetoriaAtiva)
        return false;

    if (!sd.exists(ARQUIVO_TRAJETORIA_BIN_FINAL))
        return false;

    if (!arquivoReproducaoTrajetoria.open(ARQUIVO_TRAJETORIA_BIN_FINAL, O_RDONLY))
        return false;

    reproducaoTrajetoriaAtiva = true;
    return true;
}

bool RegistradorDados::lerProximoPontoTrajetoria(PontoTrajetoria &ponto)
{
    if (!reproducaoTrajetoriaAtiva || !arquivoReproducaoTrajetoria.isOpen())
        return false;

    return arquivoReproducaoTrajetoria.read(&ponto, sizeof(ponto)) == sizeof(ponto);
}

bool RegistradorDados::fecharReproducaoTrajetoria()
{
    if (arquivoReproducaoTrajetoria.isOpen())
        arquivoReproducaoTrajetoria.close();

    reproducaoTrajetoriaAtiva = false;
    return true;
}

// INICIA LOG DA SIMULACAO

bool RegistradorDados::iniciarSimulacao(
    const InformacoesTrajetoria &trajetoria,
    float aceleracao
)
{
    if (!inicializado || simulacaoAtiva || experimentoAtivo || controleManualAtivo ||
        calibracaoAtiva || envioTrajetoriaAtivo)
    {
        return false;
    }

    if (!criarDiretorioSimulacao())
        return false;

    if (!arquivoSimulacao.open(
            ARQUIVO_DADOS_SIMULACAO,
            O_RDWR | O_CREAT | O_TRUNC))
    {
        return false;
    }

    if (!arquivoEventosSimulacao.open(
            ARQUIVO_EVENTOS_SIMULACAO,
            O_RDWR | O_CREAT | O_TRUNC))
    {
        arquivoSimulacao.close();
        return false;
    }

    bufferSimulacao.begin(&arquivoSimulacao);

    FsFile arquivoInfo;

    if (!arquivoInfo.open(
            ARQUIVO_INFO_SIMULACAO,
            O_RDWR | O_CREAT | O_TRUNC))
    {
        arquivoSimulacao.close();
        arquivoEventosSimulacao.close();
        return false;
    }

    arquivoInfo.print("FORMAT_VERSION=");
    arquivoInfo.println(ConfiguracoesSimulacao::VERSAO_FORMATO_REGISTRO_SIMULACAO);
    arquivoInfo.print("RECORD_SIZE=");
    arquivoInfo.println(sizeof(RegistroSimulacao));
    arquivoInfo.print("TRAJECTORY_ID=");
    arquivoInfo.println(trajetoria.id);
    arquivoInfo.print("POINTS=");
    arquivoInfo.println(trajetoria.quantidadePontos);
    arquivoInfo.print("FIRST_TIME_S=");
    arquivoInfo.println(trajetoria.primeiroTempo_s, 6);
    arquivoInfo.print("LAST_TIME_S=");
    arquivoInfo.println(trajetoria.ultimoTempo_s, 6);
    arquivoInfo.print("MIN_DT_S=");
    arquivoInfo.println(trajetoria.menorIntervalo_s, 9);
    arquivoInfo.print("ACCELERATION=");
    arquivoInfo.println(aceleracao, 6);
    arquivoInfo.print("MAX_REFERENCE_RATE_HZ=");
    arquivoInfo.println(aceleracao / trajetoria.menorIntervalo_s, 6);
    arquivoInfo.print("TELEMETRY_PERIOD_MS=");
    arquivoInfo.println(ConfiguracoesSimulacao::PERIODO_TELEMETRIA_MS);
    arquivoInfo.print("CURRENT_LOOP_PERIOD_US=");
    arquivoInfo.println(0); // Malha de corrente desativada
    arquivoInfo.print("FIELD_ACQUISITION_PERIOD_US=");
    arquivoInfo.println(ConfiguracoesControle::PERIODO_AQUISICAO_CAMPO_US);
    arquivoInfo.print("FIELD_LOOP_PERIOD_US=");
    arquivoInfo.println(ConfiguracoesControle::PERIODO_MALHA_CAMPO_US);
    arquivoInfo.print("AXIS_MASK=");
    arquivoInfo.println(ConfiguracoesControle::MASCARA_EIXOS);
    arquivoInfo.print("AD7606_OVERSAMPLING=");
    arquivoInfo.println(ConfiguracoesControle::SOBREAMOSTRAGEM_AD7606);
    arquivoInfo.print("INA226_AVERAGING=");
    arquivoInfo.println(ConfiguracoesControle::MEDIA_INA);
    arquivoInfo.print("INA226_SHUNT_X_OHM=");
    arquivoInfo.println(ConfiguracoesControle::RESISTENCIA_SHUNT_INA_X_OHM, 8);
    arquivoInfo.print("INA226_SHUNT_Y_OHM=");
    arquivoInfo.println(ConfiguracoesControle::RESISTENCIA_SHUNT_INA_Y_OHM, 8);
    arquivoInfo.print("INA226_SHUNT_Z_OHM=");
    arquivoInfo.println(ConfiguracoesControle::RESISTENCIA_SHUNT_INA_Z_OHM, 8);
    arquivoInfo.print("INA226_SHUNT_CONVERSION_US=");
    arquivoInfo.println(ConfiguracoesControle::TEMPO_CONVERSAO_SHUNT_INA_US);
    arquivoInfo.print("INA226_BUS_CONVERSION_US=");
    arquivoInfo.println(ConfiguracoesControle::TEMPO_CONVERSAO_BARRAMENTO_INA_US);
    arquivoInfo.print("FLUXGATE_SENSITIVITY_V_PER_UT=");
    arquivoInfo.println(ConfiguracoesControle::SENSIBILIDADE_FLUXGATE_V_POR_UT, 8);

    bool sucesso = !arquivoInfo.getWriteError() && arquivoInfo.sync();
    arquivoInfo.close();

    if (!sucesso)
    {
        arquivoSimulacao.close();
        arquivoEventosSimulacao.close();
        return false;
    }

    simulacaoAtiva = true;

    if (!registrarEventoSimulacao("SIMULATION_START"))
    {
        arquivoSimulacao.close();
        arquivoEventosSimulacao.close();
        simulacaoAtiva = false;
        return false;
    }

    return true;
}

// LOG DA SIMULACAO

bool RegistradorDados::registrarSimulacao(
    const RegistroSimulacao &registro
)
{
    if (!simulacaoAtiva)
        return false;

    const size_t escritos = bufferSimulacao.write(
        &registro,
        sizeof(registro)
    );

    if (escritos != sizeof(registro))
    {
        registrarEventoSimulacao("SIMULATION_BUFFER_OVERFLOW");
        return false;
    }

    return true;
}

bool RegistradorDados::registrarEventoSimulacao(const char *evento)
{
    if (!arquivoEventosSimulacao.isOpen() || evento == nullptr)
        return false;

    arquivoEventosSimulacao.print(millis());
    arquivoEventosSimulacao.print(" : ");
    arquivoEventosSimulacao.println(evento);

    return !arquivoEventosSimulacao.getWriteError();
}

// FINALIZA / ABORTA SIMULACAO

bool RegistradorDados::finalizarSimulacao()
{
    if (!simulacaoAtiva)
        return false;

    bool sucesso = gravarDadosSimulacaoPendentes();

    if (sucesso)
        sucesso = registrarEventoSimulacao("SIMULATION_COMPLETE");

    if (arquivoEventosSimulacao.isOpen())
    {
        if (!arquivoEventosSimulacao.sync()) sucesso = false;
        arquivoEventosSimulacao.close();
    }

    if (arquivoSimulacao.isOpen())
        arquivoSimulacao.close();

    simulacaoAtiva = false;
    return sucesso;
}

bool RegistradorDados::abortarSimulacao(const char *motivo)
{
    if (!simulacaoAtiva)
        return false;

    if (motivo != nullptr)
    {
        arquivoEventosSimulacao.print(millis());
        arquivoEventosSimulacao.print(" : SIMULATION_ABORT;REASON=");
        arquivoEventosSimulacao.println(motivo);
    }

    const bool gravacaoOK = gravarDadosSimulacaoPendentes();

    if (arquivoEventosSimulacao.isOpen())
    {
        arquivoEventosSimulacao.sync();
        arquivoEventosSimulacao.close();
    }

    if (arquivoSimulacao.isOpen())
        arquivoSimulacao.close();

    simulacaoAtiva = false;
    return gravacaoOK;
}

bool RegistradorDados::simulacaoEstaAtiva() const
{
    return simulacaoAtiva;
}

// CALIBRACAO AUTOMATICA - CAMINHOS

static const char *ARQUIVO_CALIBRACAO_BRUTA_TEMP = DIRETORIO_DADOS "/calibration_raw.tmp";
static const char *ARQUIVO_CALIBRACAO_BRUTA_FINAL = DIRETORIO_DADOS "/calibration_raw.csv";
static const char *ARQUIVO_CALIBRACAO_BRUTA_RESERVA = DIRETORIO_DADOS "/calibration_raw.bak";

static const char *ARQUIVO_INFO_CALIBRACAO_TEMP = DIRETORIO_DADOS "/calibration_info.tmp";
static const char *ARQUIVO_INFO_CALIBRACAO_FINAL = DIRETORIO_DADOS "/calibration_info.txt";
static const char *ARQUIVO_INFO_CALIBRACAO_RESERVA = DIRETORIO_DADOS "/calibration_info.bak";

static const char *ARQUIVO_PARAMETROS_CALIBRACAO_TEMP = DIRETORIO_DADOS "/calibration_params.tmp";
static const char *ARQUIVO_PARAMETROS_CALIBRACAO_FINAL = DIRETORIO_DADOS "/calibration_params.bin";
static const char *ARQUIVO_PARAMETROS_CALIBRACAO_RESERVA = DIRETORIO_DADOS "/calibration_params.bak";
static const char *ARQUIVO_RESULTADOS_CALIBRACAO = DIRETORIO_DADOS "/calibration_results.txt";

// INICIO DA CALIBRACAO

bool RegistradorDados::iniciarCalibracao(
    const SolicitacaoCalibracao &solicitacao
)
{
    if (!inicializado || experimentoAtivo || controleManualAtivo || calibracaoAtiva ||
        simulacaoAtiva || envioTrajetoriaAtivo || reproducaoTrajetoriaAtiva)
        return false;

    if (!criarDiretorio())
        return false;

    // Arquivos temporarios: uma calibracao incompleta nunca substitui
    // os arquivos finais da calibracao anterior.
    if (sd.exists(ARQUIVO_CALIBRACAO_BRUTA_TEMP))
        sd.remove(ARQUIVO_CALIBRACAO_BRUTA_TEMP);

    if (sd.exists(ARQUIVO_INFO_CALIBRACAO_TEMP))
        sd.remove(ARQUIVO_INFO_CALIBRACAO_TEMP);

    if (!arquivoCalibracaoBruto.open(
            ARQUIVO_CALIBRACAO_BRUTA_TEMP,
            O_RDWR | O_CREAT | O_TRUNC))
    {
        return false;
    }

    if (!arquivoInfoCalibracao.open(
            ARQUIVO_INFO_CALIBRACAO_TEMP,
            O_RDWR | O_CREAT | O_TRUNC))
    {
        arquivoCalibracaoBruto.close();
        sd.remove(ARQUIVO_CALIBRACAO_BRUTA_TEMP);
        return false;
    }

    arquivoCalibracaoBruto.println(
        "axis,index,pwm,current_A,Bx_uT,By_uT,Bz_uT"
    );

    arquivoInfoCalibracao.println("CALIBRATION_FORMAT_VERSION=1");
    arquivoInfoCalibracao.print("AXIS_MASK=");
    arquivoInfoCalibracao.println(solicitacao.mascaraEixos);
    arquivoInfoCalibracao.print("SOFT_LIMIT_A=");
    arquivoInfoCalibracao.println(solicitacao.limiteCorrenteOperacional, 8);
    arquivoInfoCalibracao.print("HARD_LIMIT_A=");
    arquivoInfoCalibracao.println(solicitacao.limiteCorrenteCritico, 8);

    arquivoInfoCalibracao.print("POINTS_PER_AXIS=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::TOTAL_PONTOS);
    arquivoInfoCalibracao.print("SEARCH_COARSE_STEP=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::PASSO_BUSCA_GROSSA);
    arquivoInfoCalibracao.print("SEARCH_FINE_STEP=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::PASSO_BUSCA_FINA);
    arquivoInfoCalibracao.print("FINE_SEARCH_START_FRACTION=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::FRACAO_INICIO_BUSCA_FINA, 6);
    arquivoInfoCalibracao.print("SOFT_MARGIN_FRACTION=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::FRACAO_MARGEM_LIMITE_OPERACIONAL, 6);
    arquivoInfoCalibracao.print("CONFIRM_SAMPLES=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::AMOSTRAS_CONFIRMACAO);
    arquivoInfoCalibracao.print("SETTLE_TIME_MS=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::TEMPO_ACOMODACAO_MS);
    arquivoInfoCalibracao.print("ZERO_TOLERANCE_FRACTION=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::FRACAO_TOLERANCIA_ZERO, 6);
    arquivoInfoCalibracao.print("AD7606_OVERSAMPLING=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::SOBREAMOSTRAGEM_AD7606);
    arquivoInfoCalibracao.print("INA226_AVERAGING=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::MEDIA_INA);
    arquivoInfoCalibracao.print("INA226_SHUNT_X_OHM=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::RESISTENCIA_SHUNT_INA_X_OHM, 8);
    arquivoInfoCalibracao.print("INA226_SHUNT_Y_OHM=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::RESISTENCIA_SHUNT_INA_Y_OHM, 8);
    arquivoInfoCalibracao.print("INA226_SHUNT_Z_OHM=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::RESISTENCIA_SHUNT_INA_Z_OHM, 8);
    arquivoInfoCalibracao.print("INA226_SHUNT_CONVERSION_US=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::TEMPO_CONVERSAO_SHUNT_INA_US);
    arquivoInfoCalibracao.print("INA226_BUS_CONVERSION_US=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::TEMPO_CONVERSAO_BARRAMENTO_INA_US);
    arquivoInfoCalibracao.print("FLUXGATE_SENSITIVITY_V_PER_UT=");
    arquivoInfoCalibracao.println(ConfiguracoesCalibracao::SENSIBILIDADE_FLUXGATE_V_POR_UT, 8);

    arquivoCalibracaoBruto.sync();
    arquivoInfoCalibracao.sync();

    calibracaoAtiva = true;
    return true;
}

// LOG CALIBRATION POINT

bool RegistradorDados::registrarPontoCalibracao(
    const RegistroPontoCalibracao &registro
)
{
    if (!calibracaoAtiva || !arquivoCalibracaoBruto.isOpen())
        return false;

    arquivoCalibracaoBruto.print(registro.eixo);
    arquivoCalibracaoBruto.print(',');
    arquivoCalibracaoBruto.print(registro.indice);
    arquivoCalibracaoBruto.print(',');
    arquivoCalibracaoBruto.print(registro.pwm);
    arquivoCalibracaoBruto.print(',');
    arquivoCalibracaoBruto.print(registro.corrente_A, 8);
    arquivoCalibracaoBruto.print(',');
    arquivoCalibracaoBruto.print(registro.B_uT[0], 8);
    arquivoCalibracaoBruto.print(',');
    arquivoCalibracaoBruto.print(registro.B_uT[1], 8);
    arquivoCalibracaoBruto.print(',');
    arquivoCalibracaoBruto.println(registro.B_uT[2], 8);

    return !arquivoCalibracaoBruto.getWriteError();
}

// LOG CALIBRATION INFO

bool RegistradorDados::registrarInfoCalibracao(const char *linha)
{
    if (!calibracaoAtiva || !arquivoInfoCalibracao.isOpen() || linha == nullptr)
        return false;

    arquivoInfoCalibracao.println(linha);
    return !arquivoInfoCalibracao.getWriteError();
}

// CLOSE CALIBRATION FILES

bool RegistradorDados::fecharArquivosCalibracao()
{
    bool sucesso = true;

    if (arquivoCalibracaoBruto.isOpen())
    {
        if (!arquivoCalibracaoBruto.sync())
            sucesso = false;
        arquivoCalibracaoBruto.close();
    }

    if (arquivoInfoCalibracao.isOpen())
    {
        if (!arquivoInfoCalibracao.sync())
            sucesso = false;
        arquivoInfoCalibracao.close();
    }

    return sucesso;
}

// SUBSTITUICAO SEGURA DO ARQUIVO

bool RegistradorDados::substituirArquivoComSeguranca(
    const char *caminhoTemporario,
    const char *caminhoFinal,
    const char *caminhoBackup
)
{
    if (!sd.exists(caminhoTemporario))
        return false;

    if (sd.exists(caminhoBackup))
        sd.remove(caminhoBackup);

    const bool haviaArquivoFinal = sd.exists(caminhoFinal);

    if (haviaArquivoFinal)
    {
        if (!sd.rename(caminhoFinal, caminhoBackup))
            return false;
    }

    if (!sd.rename(caminhoTemporario, caminhoFinal))
    {
        if (haviaArquivoFinal && sd.exists(caminhoBackup))
            sd.rename(caminhoBackup, caminhoFinal);

        return false;
    }

    if (sd.exists(caminhoBackup))
        sd.remove(caminhoBackup);

    return true;
}

// FINISH CALIBRATION

bool RegistradorDados::finalizarCalibracao()
{
    if (!calibracaoAtiva)
        return false;

    bool sucesso = fecharArquivosCalibracao();

    if (sucesso)
    {
        sucesso = substituirArquivoComSeguranca(
            ARQUIVO_CALIBRACAO_BRUTA_TEMP,
            ARQUIVO_CALIBRACAO_BRUTA_FINAL,
            ARQUIVO_CALIBRACAO_BRUTA_RESERVA
        );
    }

    if (sucesso)
    {
        sucesso = substituirArquivoComSeguranca(
            ARQUIVO_INFO_CALIBRACAO_TEMP,
            ARQUIVO_INFO_CALIBRACAO_FINAL,
            ARQUIVO_INFO_CALIBRACAO_RESERVA
        );
    }

    calibracaoAtiva = false;
    return sucesso;
}

// ABORT CALIBRATION

bool RegistradorDados::abortarCalibracao()
{
    if (!calibracaoAtiva)
        return false;

    fecharArquivosCalibracao();

    if (sd.exists(ARQUIVO_CALIBRACAO_BRUTA_TEMP))
        sd.remove(ARQUIVO_CALIBRACAO_BRUTA_TEMP);

    if (sd.exists(ARQUIVO_INFO_CALIBRACAO_TEMP))
        sd.remove(ARQUIVO_INFO_CALIBRACAO_TEMP);

    calibracaoAtiva = false;
    return true;
}

// CRC32

uint32_t RegistradorDados::calcularCRC32Calibracao(
    const uint8_t *dados,
    size_t tamanho
) const
{
    uint32_t crc = 0xFFFFFFFFUL;

    for (size_t i = 0; i < tamanho; i++)
    {
        crc ^= dados[i];

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            const uint32_t mascara = -(crc & 1UL);
            crc = (crc >> 1) ^ (0xEDB88320UL & mascara);
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}

// SAVE CALIBRATION PARAMETERS

bool RegistradorDados::salvarParametrosCalibracao(
    const ParametrosCalibracao &input
)
{
    if (!inicializado || experimentoAtivo || controleManualAtivo || calibracaoAtiva ||
        simulacaoAtiva || envioTrajetoriaAtivo || reproducaoTrajetoriaAtiva)
        return false;

    if (!criarDiretorio())
        return false;

    ParametrosCalibracao parametros = input;

    parametros.assinatura = ASSINATURA_PARAMETROS_CALIBRACAO;
    parametros.versao = VERSAO_PARAMETROS_CALIBRACAO;

    parametros.crc32 = calcularCRC32Calibracao(
        reinterpret_cast<const uint8_t *>(&parametros),
        offsetof(ParametrosCalibracao, crc32)
    );

    if (sd.exists(ARQUIVO_PARAMETROS_CALIBRACAO_TEMP))
        sd.remove(ARQUIVO_PARAMETROS_CALIBRACAO_TEMP);

    FsFile arquivo;

    if (!arquivo.open(
            ARQUIVO_PARAMETROS_CALIBRACAO_TEMP,
            O_RDWR | O_CREAT | O_TRUNC))
    {
        return false;
    }

    const size_t escritos = arquivo.write(
        &parametros,
        sizeof(parametros)
    );

    bool sucesso = (escritos == sizeof(parametros));

    if (sucesso)
        sucesso = arquivo.sync();

    arquivo.close();

    if (!sucesso)
    {
        sd.remove(ARQUIVO_PARAMETROS_CALIBRACAO_TEMP);
        return false;
    }

    // Leitura de verificacao antes de substituir o arquivo valido.
    ParametrosCalibracao verificacao;
    FsFile arquivoVerificacao;

    if (!arquivoVerificacao.open(ARQUIVO_PARAMETROS_CALIBRACAO_TEMP, O_RDONLY))
    {
        sd.remove(ARQUIVO_PARAMETROS_CALIBRACAO_TEMP);
        return false;
    }

    const size_t quantidadeLida = arquivoVerificacao.read(
        &verificacao,
        sizeof(verificacao)
    );

    arquivoVerificacao.close();

    if (quantidadeLida != sizeof(verificacao))
    {
        sd.remove(ARQUIVO_PARAMETROS_CALIBRACAO_TEMP);
        return false;
    }

    const uint32_t crc = calcularCRC32Calibracao(
        reinterpret_cast<const uint8_t *>(&verificacao),
        offsetof(ParametrosCalibracao, crc32)
    );

    if (verificacao.assinatura != ASSINATURA_PARAMETROS_CALIBRACAO ||
        verificacao.versao != VERSAO_PARAMETROS_CALIBRACAO ||
        verificacao.crc32 != crc)
    {
        sd.remove(ARQUIVO_PARAMETROS_CALIBRACAO_TEMP);
        return false;
    }

    const bool arquivoBinarioSalvo = substituirArquivoComSeguranca(
        ARQUIVO_PARAMETROS_CALIBRACAO_TEMP,
        ARQUIVO_PARAMETROS_CALIBRACAO_FINAL,
        ARQUIVO_PARAMETROS_CALIBRACAO_RESERVA
    );

    if (!arquivoBinarioSalvo)
        return false;

    // Arquivo de leitura humana. O binario acima e a fonte autoritativa
    // usada pelo firmware; este TXT registra tambem as metricas de ajuste.
    FsFile arquivoTexto;

    if (arquivoTexto.open(
            ARQUIVO_RESULTADOS_CALIBRACAO,
            O_RDWR | O_CREAT | O_TRUNC))
    {
        arquivoTexto.println("HELMHOLTZ CAGE - CALIBRATION PARAMETERS");
        arquivoTexto.print("FORMAT_VERSION=");
        arquivoTexto.println(parametros.versao);
        arquivoTexto.print("VALID_AXIS_MASK=");
        arquivoTexto.println(parametros.mascaraEixosValidos);

        for (uint8_t i = 0; i < 3; i++)
        {
            const uint8_t bit = bitEixoCalibracao(i);
            if (!(parametros.mascaraEixosValidos & bit) || !parametros.eixo[i].valido)
                continue;

            const ParametrosCalibracaoEixo &parametrosEixo = parametros.eixo[i];

            arquivoTexto.println();
            arquivoTexto.print("AXIS="); arquivoTexto.println(letraEixoCalibracao(i));
            arquivoTexto.print("SOFT_LIMIT_A="); arquivoTexto.println(parametrosEixo.limiteOperacional_A, 9);
            arquivoTexto.print("HARD_LIMIT_A="); arquivoTexto.println(parametrosEixo.limiteCritico_A, 9);
            arquivoTexto.print("PWM_SOFT_POS="); arquivoTexto.println(parametrosEixo.pwmOperacionalPositivo);
            arquivoTexto.print("PWM_SOFT_NEG="); arquivoTexto.println(parametrosEixo.pwmOperacionalNegativo);
            arquivoTexto.print("PWM_CAL_POS="); arquivoTexto.println(parametrosEixo.pwmCalibracaoPositivo);
            arquivoTexto.print("PWM_CAL_NEG="); arquivoTexto.println(parametrosEixo.pwmCalibracaoNegativo);

            arquivoTexto.println("MODEL=B0+k_sinal*PWM;CALIBRACAO_DIRETA");
            arquivoTexto.print("B0_UT="); arquivoTexto.println(parametrosEixo.campoEmZero_uT, 10);
            arquivoTexto.print("BP_POS_A_UT_PER_PWM="); arquivoTexto.println(parametrosEixo.coefAngularCampoPwmPositivo, 10);
            arquivoTexto.print("BP_NEG_A_UT_PER_PWM="); arquivoTexto.println(parametrosEixo.coefAngularCampoPwmNegativo, 10);
            arquivoTexto.print("BP_POS_R2="); arquivoTexto.println(parametrosEixo.qualidadeCampoPwmPositivo.r2, 10);
            arquivoTexto.print("BP_POS_RMSE_UT="); arquivoTexto.println(parametrosEixo.qualidadeCampoPwmPositivo.rmse, 10);
            arquivoTexto.print("BP_POS_MAX_UT="); arquivoTexto.println(parametrosEixo.qualidadeCampoPwmPositivo.erroAbsolutoMaximo, 10);
            arquivoTexto.print("BP_NEG_R2="); arquivoTexto.println(parametrosEixo.qualidadeCampoPwmNegativo.r2, 10);
            arquivoTexto.print("BP_NEG_RMSE_UT="); arquivoTexto.println(parametrosEixo.qualidadeCampoPwmNegativo.rmse, 10);
            arquivoTexto.print("BP_NEG_MAX_UT="); arquivoTexto.println(parametrosEixo.qualidadeCampoPwmNegativo.erroAbsolutoMaximo, 10);
        }

        arquivoTexto.sync();
        arquivoTexto.close();
    }

    return true;
}

// LOAD CALIBRATION PARAMETERS

bool RegistradorDados::carregarParametrosCalibracao(
    ParametrosCalibracao &parametros
)
{
    if (!inicializado || !sd.exists(ARQUIVO_PARAMETROS_CALIBRACAO_FINAL))
        return false;

    FsFile arquivo;

    if (!arquivo.open(ARQUIVO_PARAMETROS_CALIBRACAO_FINAL, O_RDONLY))
        return false;

    const size_t quantidadeLida = arquivo.read(
        &parametros,
        sizeof(parametros)
    );

    arquivo.close();

    if (quantidadeLida != sizeof(parametros))
        return false;

    if (parametros.assinatura != ASSINATURA_PARAMETROS_CALIBRACAO ||
        parametros.versao != VERSAO_PARAMETROS_CALIBRACAO)
    {
        return false;
    }

    const uint32_t crc = calcularCRC32Calibracao(
        reinterpret_cast<const uint8_t *>(&parametros),
        offsetof(ParametrosCalibracao, crc32)
    );

    return crc == parametros.crc32;
}

// ESTADO DA CALIBRACAO

bool RegistradorDados::calibracaoEstaAtiva() const
{
    return calibracaoAtiva;
}
