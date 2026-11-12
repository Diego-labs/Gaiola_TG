// Interpretacao dos comandos seriais; as palavras do protocolo sao preservadas para manter compatibilidade.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#include "Comunicacao.h"
#include <string.h>
#include <math.h>

// CONSTRUTOR

Comunicacao::Comunicacao()
{
    indiceBuffer = 0;

    configuracaoRecebida = false;
    novaConfiguracaoDisponivel = false;
    limpezaSolicitada = false;
    inicioSolicitado = false;
    estadoSolicitado = false;
    baixarDadosSolicitado = false;

    configuracaoCalibracao.mascaraEixos = 0;
    configuracaoCalibracao.limiteCorrenteOperacional = 0.0f;
    configuracaoCalibracao.limiteCorrenteCritico = 0.0f;

    configuracaoCalibracaoRecebida = false;
    novaConfiguracaoCalibracao = false;
    inicioCalibracaoSolicitado = false;
    estadoCalibracaoSolicitado = false;
    abortoCalibracaoSolicitado = false;

    memset(&parametrosCalibracao, 0, sizeof(parametrosCalibracao));
    transferenciaParametrosCalibracaoAtiva = false;
    parametrosCalibracaoProntos = false;
    mascaraParametrosEsperados = 0;
    mascaraLimitesRecebidos = 0;
    mascaraCampoPwmRecebida = 0;

    inicioControleSolicitado = false;
    paradaControleSolicitada = false;
    estadoControleSolicitado = false;
    limitesControleSolicitados = false;
    baixarControleSolicitado = false;

    referenciaCampo.B_uT[0] = 0.0f;
    referenciaCampo.B_uT[1] = 0.0f;
    referenciaCampo.B_uT[2] = 0.0f;

    referenciaCampoRecebida = false;
    novaReferenciaCampo = false;

    memset(&cabecalhoTrajetoria, 0, sizeof(cabecalhoTrajetoria));
    memset(&pontoTrajetoria, 0, sizeof(pontoTrajetoria));
    solicitacaoInicioSimulacao.aceleracao = 1.0f;

    inicioTrajetoriaSolicitado = false;
    pontoTrajetoriaPronto = false;
    fimTrajetoriaSolicitado = false;
    abortoTrajetoriaSolicitado = false;
    estadoTrajetoriaSolicitado = false;

    inicioSimulacaoSolicitado = false;
    pausaSimulacaoSolicitada = false;
    retomadaSimulacaoSolicitada = false;
    abortoSimulacaoSolicitado = false;
    estadoSimulacaoSolicitado = false;
    baixarSimulacaoSolicitado = false;

    heartbeatRecebido = false;
    ultimoHeartbeatMs = 0;
}

// BEGIN

void Comunicacao::iniciar()
{
    Serial.begin(115200);

    indiceBuffer = 0;
    bufferTemporario[0] = '\0';

    configuracaoRecebida = false;
    novaConfiguracaoDisponivel = false;
    limpezaSolicitada = false;
    inicioSolicitado = false;
    estadoSolicitado = false;
    baixarDadosSolicitado = false;

    configuracaoCalibracaoRecebida = false;
    novaConfiguracaoCalibracao = false;
    inicioCalibracaoSolicitado = false;
    estadoCalibracaoSolicitado = false;
    abortoCalibracaoSolicitado = false;

    memset(&parametrosCalibracao, 0, sizeof(parametrosCalibracao));
    transferenciaParametrosCalibracaoAtiva = false;
    parametrosCalibracaoProntos = false;
    mascaraParametrosEsperados = 0;
    mascaraLimitesRecebidos = 0;
    mascaraCampoPwmRecebida = 0;

    inicioControleSolicitado = false;
    paradaControleSolicitada = false;
    estadoControleSolicitado = false;
    limitesControleSolicitados = false;
    baixarControleSolicitado = false;

    referenciaCampo.B_uT[0] = 0.0f;
    referenciaCampo.B_uT[1] = 0.0f;
    referenciaCampo.B_uT[2] = 0.0f;

    referenciaCampoRecebida = false;
    novaReferenciaCampo = false;

    memset(&cabecalhoTrajetoria, 0, sizeof(cabecalhoTrajetoria));
    memset(&pontoTrajetoria, 0, sizeof(pontoTrajetoria));
    solicitacaoInicioSimulacao.aceleracao = 1.0f;

    inicioTrajetoriaSolicitado = false;
    pontoTrajetoriaPronto = false;
    fimTrajetoriaSolicitado = false;
    abortoTrajetoriaSolicitado = false;
    estadoTrajetoriaSolicitado = false;

    inicioSimulacaoSolicitado = false;
    pausaSimulacaoSolicitada = false;
    retomadaSimulacaoSolicitada = false;
    abortoSimulacaoSolicitado = false;
    estadoSimulacaoSolicitado = false;
    baixarSimulacaoSolicitado = false;

    heartbeatRecebido = false;
    ultimoHeartbeatMs = 0;
}

// UPDATE

void Comunicacao::atualizar()
{
    while (Serial.available() > 0)
    {
        char c = Serial.read();

        // Fim do comando

        if (c == '\n' || c == '\r')
        {
            if (indiceBuffer > 0)
            {
                bufferTemporario[indiceBuffer] = '\0';

                processarComando(bufferTemporario);

                indiceBuffer = 0;
                bufferTemporario[0] = '\0';

                // Processa no máximo um comando completo por chamada.
                // Isso garante que comandos TRAJ_POINT não sobrescrevam
                // o ponto pendente antes que o loop principal o grave no SD.
                return;
            }
        }

        // Adiciona caractere ao buffer

        else
        {
            if (indiceBuffer < sizeof(bufferTemporario) - 1)
            {
                bufferTemporario[indiceBuffer++] = c;
            }
            else
            {
                indiceBuffer = 0;
                bufferTemporario[0] = '\0';

                Serial.println("ERROR;BUFFER_OVERFLOW");
            }
        }
    }
}

// PROCESS COMMAND

void Comunicacao::processarComando(char* comando)
{
    if (comando == nullptr)
        return;

    // Remove espaços iniciais

    while (*comando == ' ')
        comando++;

    // PING

    if (strcmp(comando, "PING") == 0)
    {
        Serial.println("PONG");
        return;
    }

    // HEARTBEAT MATLAB <-> TEENSY

    if (strcmp(comando, "HEARTBEAT") == 0)
    {
        heartbeatRecebido = true;
        ultimoHeartbeatMs = millis();
        Serial.println("HEARTBEAT_OK");
        return;
    }

    // CLEAR

    if (strcmp(comando, "CLEAR") == 0)
    {
        limpezaSolicitada = true;

        configuracaoRecebida = false;
        novaConfiguracaoDisponivel = false;
        inicioSolicitado = false;
        estadoSolicitado = false;
        baixarDadosSolicitado = false;
        baixarControleSolicitado = false;
        baixarSimulacaoSolicitado = false;

        Serial.println("CLEARED");

        return;
    }

    // COMANDO START (INICIAR)

    if (strcmp(comando, "START") == 0)
    {
        if (!configuracaoRecebida)
        {
            Serial.println("ERROR;NOT_READY");
            return;
        }

        inicioSolicitado = true;

        return;
    }

    // STATUS

    if (strcmp(comando, "STATUS") == 0)
    {
        estadoSolicitado = true;

        return;
    }

    // COMANDO DOWNLOAD (BAIXAR DADOS)

    if (strcmp(comando, "DOWNLOAD") == 0)
    {
        baixarDadosSolicitado = true;

        return;
    }

    // CONFIG

    if (strncmp(comando, "CONFIG;", 7) == 0)
    {
        interpretarConfiguracao(comando + 7);
        return;
    }

    // CALIBRACAO AUTOMATICA

    if (strncmp(comando, "CAL_CONFIG;", 11) == 0)
    {
        interpretarConfiguracaoCalibracao(comando + 11);
        return;
    }

    if (strcmp(comando, "CAL_START") == 0)
    {
        if (!configuracaoCalibracaoRecebida)
        {
            Serial.println("ERROR;CAL_NOT_CONFIGURED");
            return;
        }

        inicioCalibracaoSolicitado = true;
        return;
    }

    if (strcmp(comando, "CAL_STATUS") == 0)
    {
        estadoCalibracaoSolicitado = true;
        return;
    }

    if (strcmp(comando, "CAL_ABORT") == 0)
    {
        abortoCalibracaoSolicitado = true;
        return;
    }

    if (strncmp(comando, "CAL_PARAMS_BEGIN;", 17) == 0)
    {
        interpretarInicioParametrosCalibracao(comando + 17);
        return;
    }

    if (strncmp(comando, "CAL_PARAMS_LIMITS;", 18) == 0)
    {
        interpretarLimitesParametrosCalibracao(comando + 18);
        return;
    }

    if (strncmp(comando, "CAL_PARAMS_BP;", 14) == 0)
    {
        interpretarParametrosCampoPWM(comando + 14);
        return;
    }

    if (strcmp(comando, "CAL_PARAMS_COMMIT") == 0)
    {
        confirmarParametrosCalibracao();
        return;
    }

    // CONTROLE DIRETO DE CAMPO

    if (strcmp(comando, "CONTROL_START") == 0)
    {
        inicioControleSolicitado = true;
        return;
    }

    if (strcmp(comando, "CONTROL_STOP") == 0)
    {
        paradaControleSolicitada = true;
        return;
    }

    if (strcmp(comando, "CONTROL_STATUS") == 0)
    {
        estadoControleSolicitado = true;
        return;
    }

    if (strcmp(comando, "CONTROL_LIMITS") == 0)
    {
        limitesControleSolicitados = true;
        return;
    }

    if (strcmp(comando, "CONTROL_DOWNLOAD") == 0)
    {
        baixarControleSolicitado = true;
        return;
    }

    if (strncmp(comando, "FIELD_REF;", 10) == 0)
    {
        interpretarReferenciaCampo(comando + 10);
        return;
    }

    // TRAJETORIA ORBITAL

    if (strncmp(comando, "TRAJ_BEGIN;", 11) == 0)
    {
        interpretarInicioTrajetoria(comando + 11);
        return;
    }

    if (strncmp(comando, "TRAJ_POINT;", 11) == 0)
    {
        interpretarPontoTrajetoria(comando + 11);
        return;
    }

    if (strcmp(comando, "TRAJ_END") == 0)
    {
        fimTrajetoriaSolicitado = true;
        return;
    }

    if (strcmp(comando, "TRAJ_ABORT") == 0)
    {
        abortoTrajetoriaSolicitado = true;
        return;
    }

    if (strcmp(comando, "TRAJ_STATUS") == 0)
    {
        estadoTrajetoriaSolicitado = true;
        return;
    }

    // SIMULACAO ORBITAL

    if (strncmp(comando, "SIM_START;", 10) == 0)
    {
        interpretarInicioSimulacao(comando + 10);
        return;
    }

    if (strcmp(comando, "SIM_PAUSE") == 0)
    {
        pausaSimulacaoSolicitada = true;
        return;
    }

    if (strcmp(comando, "SIM_RESUME") == 0)
    {
        retomadaSimulacaoSolicitada = true;
        return;
    }

    if (strcmp(comando, "SIM_ABORT") == 0)
    {
        abortoSimulacaoSolicitado = true;
        return;
    }

    if (strcmp(comando, "SIM_STATUS") == 0)
    {
        estadoSimulacaoSolicitado = true;
        return;
    }

    if (strcmp(comando, "SIM_DOWNLOAD") == 0)
    {
        baixarSimulacaoSolicitado = true;
        return;
    }

    // COMANDO DESCONHECIDO

    Serial.println("ERROR;UNKNOWN_COMMAND");
}

// PARSE CONFIG

void Comunicacao::interpretarConfiguracao(char* parametros)
{
    if (parametros == nullptr)
    {
        Serial.println("ERROR;CONFIG");
        return;
    }

    // Configuração temporária

    ConfiguracaoExperimento novaConfiguracao;

    novaConfiguracao.mascaraEixos = 0;

    novaConfiguracao.pwmInicial = 0;
    novaConfiguracao.pwmFinal = 0;
    novaConfiguracao.passoPWM = 0;

    novaConfiguracao.tempoPermanencia_ms = 0;

    novaConfiguracao.limiteCorrenteOperacional = -1.0f;
    novaConfiguracao.limiteCorrenteCritico = -1.0f;

    novaConfiguracao.sobreamostragemAD7606 = 1;

    novaConfiguracao.mediaINA226 = 16;

    novaConfiguracao.tempoConversaoShuntINA226_us = 332;
    novaConfiguracao.tempoConversaoBarramentoINA226_us = 332;

    // Indicadores de presença dos parâmetros

    bool recebeuMascaraEixos = false;
    bool recebeuPWMInicial = false;
    bool recebeuPWMFinal = false;
    bool recebeuPassoPWM = false;
    bool recebeuTempoPermanencia = false;

    bool recebeuLimiteOperacional = false;
    bool recebeuLimiteCritico = false;

    bool recebeuSobreamostragemAD7606 = false;

    bool recebeuMediaINA = false;
    bool recebeuTempoShuntINA = false;
    bool recebeuTempoBarramentoINA = false;

    // Converte para String apenas internamente.
    // A função continua com a assinatura original char*.

    String dados = String(parametros);

    // PROCESSA CADA CAMPO

    while (dados.length() > 0)
    {
        int separador = dados.indexOf(';');

        String campo;

        if (separador >= 0)
        {
            campo = dados.substring(0, separador);
            dados = dados.substring(separador + 1);
        }
        else
        {
            campo = dados;
            dados = "";
        }

        campo.trim();

        if (campo.length() == 0)
            continue;

        // Separa KEY=VALUE

        int igual = campo.indexOf('=');

        if (igual < 0)
        {
            Serial.println("ERROR;CONFIG");
            return;
        }

        String chave = campo.substring(0, igual);
        String valor = campo.substring(igual + 1);

        chave.trim();
        valor.trim();

        // AXIS

        if (chave == "AXIS")
        {
            novaConfiguracao.mascaraEixos =
                (uint8_t)valor.toInt();

            recebeuMascaraEixos = true;
        }

        // PWM COMANDO START (INICIAR)

        else if (chave == "PWMSTART")
        {
            novaConfiguracao.pwmInicial =
                (int16_t)valor.toInt();

            recebeuPWMInicial = true;
        }

        // PWM PARADA

        else if (chave == "PWMSTOP")
        {
            novaConfiguracao.pwmFinal =
                (int16_t)valor.toInt();

            recebeuPWMFinal = true;
        }

        // PWM STEP

        else if (chave == "PWMSTEP")
        {
            novaConfiguracao.passoPWM =
                (int16_t)valor.toInt();

            recebeuPassoPWM = true;
        }

        // DWELL

        else if (chave == "DWELL")
        {
            novaConfiguracao.tempoPermanencia_ms =
                (uint32_t)valor.toInt();

            recebeuTempoPermanencia = true;
        }

        // LIMITE SUAVE

        else if (chave == "SOFT")
        {
            novaConfiguracao.limiteCorrenteOperacional =
                valor.toFloat();

            recebeuLimiteOperacional = true;
        }

        // LIMITE CRÍTICO

        else if (chave == "HARD")
        {
            novaConfiguracao.limiteCorrenteCritico =
                valor.toFloat();

            recebeuLimiteCritico = true;
        }

        // SOBREAMOSTRAGEM AD7606

        else if (chave == "AD_OS")
        {
            novaConfiguracao.sobreamostragemAD7606 =
                (uint8_t)valor.toInt();

            recebeuSobreamostragemAD7606 = true;
        }

        // INA226 MEDIA DE AMOSTRAS

        else if (chave == "INA_AVG")
        {
            novaConfiguracao.mediaINA226 =
                (uint16_t)valor.toInt();

            recebeuMediaINA = true;
        }

        // INA226 CONVERSAO DO SHUNT

        else if (chave == "INA_SHUNT")
        {
            novaConfiguracao.tempoConversaoShuntINA226_us =
                (uint16_t)valor.toInt();

            recebeuTempoShuntINA = true;
        }

        // INA226 CONVERSAO DO BARRAMENTO

        else if (chave == "INA_BUS")
        {
            novaConfiguracao.tempoConversaoBarramentoINA226_us =
                (uint16_t)valor.toInt();

            recebeuTempoBarramentoINA = true;
        }

        // CAMPO DESCONHECIDO

        else
        {
            Serial.print("ERROR;UNKNOWN_CONFIG=");
            Serial.println(chave);

            return;
        }
    }

    // VERIFICA SE TODOS OS CAMPOS EXISTEM

    if (!recebeuMascaraEixos ||
        !recebeuPWMInicial ||
        !recebeuPWMFinal ||
        !recebeuPassoPWM ||
        !recebeuTempoPermanencia ||
        !recebeuLimiteOperacional ||
        !recebeuLimiteCritico ||
        !recebeuSobreamostragemAD7606 ||
        !recebeuMediaINA ||
        !recebeuTempoShuntINA ||
        !recebeuTempoBarramentoINA)
    {
        Serial.println("ERROR;CONFIG_MISSING");
        return;
    }

    // VALIDA AXIS MASK

    if (novaConfiguracao.mascaraEixos < 1 ||
        novaConfiguracao.mascaraEixos > 7)
    {
        Serial.println("ERROR;AXIS");
        return;
    }

    // VALIDA PWM

    if (novaConfiguracao.pwmInicial < -4095 ||
        novaConfiguracao.pwmInicial > 4095)
    {
        Serial.println("ERROR;PWMSTART");
        return;
    }

    if (novaConfiguracao.pwmFinal < -4095 ||
        novaConfiguracao.pwmFinal > 4095)
    {
        Serial.println("ERROR;PWMSTOP");
        return;
    }

    if (novaConfiguracao.passoPWM == 0)
    {
        Serial.println("ERROR;PWMSTEP");
        return;
    }

    // VALIDA DIREÇÃO DO SWEEP

    if (novaConfiguracao.passoPWM > 0 &&
        novaConfiguracao.pwmFinal < novaConfiguracao.pwmInicial)
    {
        Serial.println("ERROR;PWM_DIRECTION");
        return;
    }

    if (novaConfiguracao.passoPWM < 0 &&
        novaConfiguracao.pwmFinal > novaConfiguracao.pwmInicial)
    {
        Serial.println("ERROR;PWM_DIRECTION");
        return;
    }

    // DWELL

    if (novaConfiguracao.tempoPermanencia_ms == 0)
    {
        Serial.println("ERROR;DWELL");
        return;
    }

    // LIMITES DE CORRENTE

    if (novaConfiguracao.limiteCorrenteOperacional <= 0.0f)
    {
        Serial.println("ERROR;SOFT_LIMIT");
        return;
    }

    if (novaConfiguracao.limiteCorrenteCritico <= 0.0f)
    {
        Serial.println("ERROR;HARD_LIMIT");
        return;
    }

    if (novaConfiguracao.limiteCorrenteCritico <=
        novaConfiguracao.limiteCorrenteOperacional)
    {
        Serial.println("ERROR;LIMIT_ORDER");
        return;
    }

    // AD7606 SOBREAMOSTRAGEM

    switch (novaConfiguracao.sobreamostragemAD7606)
    {
        case 1:
        case 2:
        case 4:
        case 8:
        case 16:
        case 32:
        case 64:
            break;

        default:
            Serial.println("ERROR;AD_OS");
            return;
    }

    // INA226 MEDIA DE AMOSTRAS

    switch (novaConfiguracao.mediaINA226)
    {
        case 1:
        case 4:
        case 16:
        case 64:
        case 128:
        case 256:
        case 512:
        case 1024:
            break;

        default:
            Serial.println("ERROR;INA_AVG");
            return;
    }

    // INA226 CONVERSAO DO SHUNT

    switch (novaConfiguracao.tempoConversaoShuntINA226_us)
    {
        case 140:
        case 204:
        case 332:
        case 588:
        case 1100:
        case 2116:
        case 4156:
        case 8244:
            break;

        default:
            Serial.println("ERROR;INA_SHUNT");
            return;
    }

    // INA226 CONVERSAO DO BARRAMENTO

    switch (novaConfiguracao.tempoConversaoBarramentoINA226_us)
    {
        case 140:
        case 204:
        case 332:
        case 588:
        case 1100:
        case 2116:
        case 4156:
        case 8244:
            break;

        default:
            Serial.println("ERROR;INA_BUS");
            return;
    }

    // CONFIGURAÇÃO VALIDADA

    configuracao = novaConfiguracao;

    configuracaoRecebida = true;
    novaConfiguracaoDisponivel = true;

    Serial.println("CONFIG_OK");
}

// CONFIGURACAO DISPONIVEL

bool Comunicacao::possuiConfiguracao()
{
    return configuracaoRecebida;
}

// OBTER CONFIGURACAO

ConfiguracaoExperimento Comunicacao::obterConfiguracao()
{
    return configuracao;
}

// NOVA CONFIGURACAO

bool Comunicacao::haNovaConfiguracao()
{
    return novaConfiguracaoDisponivel;
}

void Comunicacao::limparNovaConfiguracao()
{
    novaConfiguracaoDisponivel = false;
}

// LIMPAR SOLICITACAO

bool Comunicacao::limpezaFoiSolicitada()
{
    return limpezaSolicitada;
}

void Comunicacao::limparSolicitacaoLimpeza()
{
    limpezaSolicitada = false;
}

// SOLICITACAO DE INICIO

bool Comunicacao::inicioFoiSolicitado()
{
    return inicioSolicitado;
}

void Comunicacao::limparSolicitacaoInicio()
{
    inicioSolicitado = false;
}

// SOLICITACAO DE ESTADO

bool Comunicacao::estadoFoiSolicitado()
{
    return estadoSolicitado;
}

void Comunicacao::limparSolicitacaoEstado()
{
    estadoSolicitado = false;
}

// SOLICITACAO DE COMANDO DOWNLOAD (BAIXAR DADOS)

bool Comunicacao::baixarDadosFoiSolicitado()
{
    return baixarDadosSolicitado;
}

void Comunicacao::limparSolicitacaoBaixarDados()
{
    baixarDadosSolicitado = false;
}

// CALIBRATION PARSING HELPERS

static bool obterCampoDoComando(
    const String &dados,
    const char *chave,
    String &valor
)
{
    const String prefixo = String(chave) + "=";

    int inicio = dados.indexOf(prefixo);
    if (inicio < 0)
        return false;

    inicio += prefixo.length();

    int fim = dados.indexOf(';', inicio);
    if (fim < 0)
        fim = dados.length();

    valor = dados.substring(inicio, fim);
    valor.trim();

    return valor.length() > 0;
}

static bool obterInteiroDoComando(
    const String &dados,
    const char *chave,
    int &valor
)
{
    String texto;
    if (!obterCampoDoComando(dados, chave, texto))
        return false;

    valor = texto.toInt();
    return true;
}

static bool obterRealDoComando(
    const String &dados,
    const char *chave,
    float &valor
)
{
    String texto;
    if (!obterCampoDoComando(dados, chave, texto))
        return false;

    valor = texto.toFloat();
    return isfinite(valor);
}

static bool obterEixoDoComando(
    const String &dados,
    char &eixo
)
{
    String texto;
    if (!obterCampoDoComando(dados, "AXIS", texto) || texto.length() != 1)
        return false;

    eixo = texto.charAt(0);
    return indiceEixoCalibracao(eixo) >= 0;
}

// FIELD_REF
// Formato:
// FIELD_REF;BX=...;BY=...;BZ=...
// Os valores sao campos totais desejados em microtesla.

void Comunicacao::interpretarReferenciaCampo(
    char *parametros
)
{
    if (parametros == nullptr)
    {
        Serial.println("ERROR;FIELD_REF");
        return;
    }

    const String dados(parametros);

    float campoX_uT = 0.0f;
    float campoY_uT = 0.0f;
    float campoZ_uT = 0.0f;

    if (!obterRealDoComando(dados, "BX", campoX_uT) ||
        !obterRealDoComando(dados, "BY", campoY_uT) ||
        !obterRealDoComando(dados, "BZ", campoZ_uT))
    {
        Serial.println("ERROR;FIELD_REF_FORMAT");
        return;
    }

    referenciaCampo.B_uT[0] = campoX_uT;
    referenciaCampo.B_uT[1] = campoY_uT;
    referenciaCampo.B_uT[2] = campoZ_uT;

    referenciaCampoRecebida = true;
    novaReferenciaCampo = true;
}

// TRAJ_BEGIN
// TRAJ_BEGIN;ID=<texto>;N=<numero de pontos>

void Comunicacao::interpretarInicioTrajetoria(char *parametros)
{
    if (parametros == nullptr)
    {
        Serial.println("ERROR;TRAJ_BEGIN_FORMAT");
        return;
    }

    const String dados(parametros);
    String id;
    int quantidadePontos = 0;

    if (!obterCampoDoComando(dados, "ID", id) ||
        !obterInteiroDoComando(dados, "N", quantidadePontos))
    {
        Serial.println("ERROR;TRAJ_BEGIN_FORMAT");
        return;
    }

    if (id.length() == 0 || id.length() >= TAMANHO_ID_TRAJETORIA ||
        quantidadePontos < 2)
    {
        Serial.println("ERROR;TRAJ_BEGIN_RANGE");
        return;
    }

    memset(&cabecalhoTrajetoria, 0, sizeof(cabecalhoTrajetoria));
    id.toCharArray(cabecalhoTrajetoria.id, sizeof(cabecalhoTrajetoria.id));
    cabecalhoTrajetoria.quantidadePontos = (uint32_t)quantidadePontos;

    inicioTrajetoriaSolicitado = true;
}

// TRAJ_POINT
// TRAJ_POINT;I=0;T=0.0;BX=...;BY=...;BZ=...

void Comunicacao::interpretarPontoTrajetoria(char *parametros)
{
    if (parametros == nullptr)
    {
        Serial.println("ERROR;TRAJ_POINT_FORMAT");
        return;
    }

    const String dados(parametros);

    int indice = -1;
    float tempo_s = 0.0f;
    float campoX_uT = 0.0f;
    float campoY_uT = 0.0f;
    float campoZ_uT = 0.0f;

    if (!obterInteiroDoComando(dados, "I", indice) || indice < 0 ||
        !obterRealDoComando(dados, "T", tempo_s) ||
        !obterRealDoComando(dados, "BX", campoX_uT) ||
        !obterRealDoComando(dados, "BY", campoY_uT) ||
        !obterRealDoComando(dados, "BZ", campoZ_uT))
    {
        Serial.println("ERROR;TRAJ_POINT_FORMAT");
        return;
    }

    pontoTrajetoria.indice = (uint32_t)indice;
    pontoTrajetoria.tempo_s = tempo_s;
    pontoTrajetoria.B_uT[0] = campoX_uT;
    pontoTrajetoria.B_uT[1] = campoY_uT;
    pontoTrajetoria.B_uT[2] = campoZ_uT;

    pontoTrajetoriaPronto = true;
}

// SIM_COMANDO START (INICIAR)
// SIM_COMANDO START (INICIAR);ACCEL=100

void Comunicacao::interpretarInicioSimulacao(char *parametros)
{
    if (parametros == nullptr)
    {
        Serial.println("ERROR;SIM_START_FORMAT");
        return;
    }

    const String dados(parametros);
    float aceleracao = 0.0f;

    if (!obterRealDoComando(dados, "ACCEL", aceleracao) ||
        aceleracao <= 0.0f)
    {
        Serial.println("ERROR;SIM_ACCEL");
        return;
    }

    solicitacaoInicioSimulacao.aceleracao = aceleracao;
    inicioSimulacaoSolicitado = true;
}

// CAL_CONFIG

void Comunicacao::interpretarConfiguracaoCalibracao(char *parametros)
{
    if (parametros == nullptr)
    {
        Serial.println("ERROR;CAL_CONFIG");
        return;
    }

    const String dados(parametros);

    int mascaraEixos = 0;
    float operacional = 0.0f;
    float critico = 0.0f;

    if (!obterInteiroDoComando(dados, "AXIS", mascaraEixos) ||
        !obterRealDoComando(dados, "SOFT", operacional) ||
        !obterRealDoComando(dados, "HARD", critico))
    {
        Serial.println("ERROR;CAL_CONFIG_MISSING");
        return;
    }

    if (mascaraEixos < 1 || mascaraEixos > 7)
    {
        Serial.println("ERROR;CAL_AXIS");
        return;
    }

    if (operacional <= 0.0f)
    {
        Serial.println("ERROR;CAL_SOFT_LIMIT");
        return;
    }

    if (critico <= operacional)
    {
        Serial.println("ERROR;CAL_LIMIT_ORDER");
        return;
    }

    configuracaoCalibracao.mascaraEixos = (uint8_t)mascaraEixos;
    configuracaoCalibracao.limiteCorrenteOperacional = operacional;
    configuracaoCalibracao.limiteCorrenteCritico = critico;

    configuracaoCalibracaoRecebida = true;
    novaConfiguracaoCalibracao = true;
}

// CAL_PARAMS_BEGIN

void Comunicacao::interpretarInicioParametrosCalibracao(char *parametros)
{
    if (parametros == nullptr)
    {
        Serial.println("ERROR;CAL_PARAMS_BEGIN");
        return;
    }

    const String dados(parametros);
    int mascaraEixos = 0;

    int versaoRecebida = 0;
    if (!obterInteiroDoComando(dados, "AXIS_MASK", mascaraEixos) ||
        !obterInteiroDoComando(dados, "VERSION", versaoRecebida) ||
        versaoRecebida != VERSAO_PARAMETROS_CALIBRACAO ||
        mascaraEixos < 1 || mascaraEixos > 7)
    {
        Serial.println("ERROR;CAL_PARAMS_VERSION_OR_AXIS_MASK");
        return;
    }

    memset(&parametrosCalibracao, 0, sizeof(parametrosCalibracao));

    parametrosCalibracao.assinatura = ASSINATURA_PARAMETROS_CALIBRACAO;
    parametrosCalibracao.versao = VERSAO_PARAMETROS_CALIBRACAO;
    parametrosCalibracao.mascaraEixosValidos = (uint8_t)mascaraEixos;

    mascaraParametrosEsperados = (uint8_t)mascaraEixos;
    mascaraLimitesRecebidos = 0;
    mascaraCampoPwmRecebida = 0;

    transferenciaParametrosCalibracaoAtiva = true;
    parametrosCalibracaoProntos = false;

    Serial.println("CAL_PARAMS_BEGIN_OK");
}

// CAL_PARAMS_LIMITS

void Comunicacao::interpretarLimitesParametrosCalibracao(char *parametros)
{
    if (!transferenciaParametrosCalibracaoAtiva || parametros == nullptr)
    {
        Serial.println("ERROR;CAL_PARAMS_NOT_ACTIVE");
        return;
    }

    const String dados(parametros);

    char eixo = '-';
    float operacional = 0.0f;
    float critico = 0.0f;
    int pwmOperacionalPositivoRecebido = 0;
    int pwmOperacionalNegativoRecebido = 0;
    int pwmCalibracaoPositivoRecebido = 0;
    int pwmCalibracaoNegativoRecebido = 0;

    if (!obterEixoDoComando(dados, eixo) ||
        !obterRealDoComando(dados, "SOFT", operacional) ||
        !obterRealDoComando(dados, "HARD", critico) ||
        !obterInteiroDoComando(dados, "PWM_SOFT_POS", pwmOperacionalPositivoRecebido) ||
        !obterInteiroDoComando(dados, "PWM_SOFT_NEG", pwmOperacionalNegativoRecebido) ||
        !obterInteiroDoComando(dados, "PWM_CAL_POS", pwmCalibracaoPositivoRecebido) ||
        !obterInteiroDoComando(dados, "PWM_CAL_NEG", pwmCalibracaoNegativoRecebido))
    {
        Serial.println("ERROR;CAL_PARAMS_LIMITS_FORMAT");
        return;
    }

    const int indice = indiceEixoCalibracao(eixo);
    const uint8_t bit = bitEixoCalibracao(indice);

    if (!(mascaraParametrosEsperados & bit))
    {
        Serial.println("ERROR;CAL_PARAMS_AXIS_NOT_EXPECTED");
        return;
    }

    if (operacional <= 0.0f || critico <= operacional ||
        pwmOperacionalPositivoRecebido < 0 || pwmOperacionalPositivoRecebido > 4095 ||
        pwmOperacionalNegativoRecebido > 0 || pwmOperacionalNegativoRecebido < -4095 ||
        pwmCalibracaoPositivoRecebido < 0 || pwmCalibracaoPositivoRecebido > pwmOperacionalPositivoRecebido ||
        pwmCalibracaoNegativoRecebido > 0 || pwmCalibracaoNegativoRecebido < pwmOperacionalNegativoRecebido)
    {
        Serial.println("ERROR;CAL_PARAMS_LIMITS_RANGE");
        return;
    }

    ParametrosCalibracaoEixo &parametrosEixo = parametrosCalibracao.eixo[indice];

    parametrosEixo.limiteOperacional_A = operacional;
    parametrosEixo.limiteCritico_A = critico;

    parametrosEixo.pwmOperacionalPositivo = (int16_t)pwmOperacionalPositivoRecebido;
    parametrosEixo.pwmOperacionalNegativo = (int16_t)pwmOperacionalNegativoRecebido;
    parametrosEixo.pwmCalibracaoPositivo = (int16_t)pwmCalibracaoPositivoRecebido;
    parametrosEixo.pwmCalibracaoNegativo = (int16_t)pwmCalibracaoNegativoRecebido;

    mascaraLimitesRecebidos |= bit;

    Serial.print("CAL_PARAMS_LIMITS_OK;AXIS=");
    Serial.println(eixo);
}

// CAL_PARAMS_BP: regressao DIRETA B(PWM), ancorada em B0.
void Comunicacao::interpretarParametrosCampoPWM(char *parametros)
{
    if (!transferenciaParametrosCalibracaoAtiva || parametros == nullptr)
    {
        Serial.println("ERROR;CAL_PARAMS_NOT_ACTIVE");
        return;
    }
    const String dados(parametros);
    char eixo = '-';
    float b0, kp, kn, r2p, rmsep, maxp, r2n, rmsen, maxn;
    if (!obterEixoDoComando(dados, eixo) ||
        !obterRealDoComando(dados, "B0", b0) ||
        !obterRealDoComando(dados, "POS_A", kp) ||
        !obterRealDoComando(dados, "NEG_A", kn) ||
        !obterRealDoComando(dados, "POS_R2", r2p) ||
        !obterRealDoComando(dados, "POS_RMSE", rmsep) ||
        !obterRealDoComando(dados, "POS_MAX", maxp) ||
        !obterRealDoComando(dados, "NEG_R2", r2n) ||
        !obterRealDoComando(dados, "NEG_RMSE", rmsen) ||
        !obterRealDoComando(dados, "NEG_MAX", maxn))
    {
        Serial.println("ERROR;CAL_PARAMS_BP_FORMAT");
        return;
    }
    const int indice = indiceEixoCalibracao(eixo);
    if (indice < 0)
    {
        Serial.println("ERROR;CAL_PARAMS_BP_AXIS");
        return;
    }
    const uint8_t bit = bitEixoCalibracao((uint8_t)indice);
    if (!(mascaraParametrosEsperados & bit) || !(mascaraLimitesRecebidos & bit))
    {
        Serial.println("ERROR;CAL_PARAMS_BP_LIMITS_REQUIRED");
        return;
    }
    if (!isfinite(b0) || !isfinite(kp) || !isfinite(kn) ||
        kp <= 1e-8f || kn <= 1e-8f ||
        !isfinite(r2p) || !isfinite(rmsep) || !isfinite(maxp) ||
        !isfinite(r2n) || !isfinite(rmsen) || !isfinite(maxn) ||
        rmsep < 0 || rmsen < 0 || maxp < 0 || maxn < 0)
    {
        Serial.println("ERROR;CAL_PARAMS_BP_VALUES");
        return;
    }
    ParametrosCalibracaoEixo &p = parametrosCalibracao.eixo[indice];
    p.campoEmZero_uT = b0;
    p.coefAngularCampoPwmPositivo = kp;
    p.coefAngularCampoPwmNegativo = kn;
    p.qualidadeCampoPwmPositivo = {r2p, rmsep, maxp};
    p.qualidadeCampoPwmNegativo = {r2n, rmsen, maxn};
    mascaraCampoPwmRecebida |= bit;
    Serial.print("CAL_PARAMS_BP_OK;AXIS=");
    Serial.println(eixo);
}

// CAL_PARAMS_COMMIT

void Comunicacao::confirmarParametrosCalibracao()
{
    if (!transferenciaParametrosCalibracaoAtiva)
    {
        Serial.println("ERROR;CAL_PARAMS_NOT_ACTIVE");
        return;
    }

    if (mascaraLimitesRecebidos != mascaraParametrosEsperados ||
        mascaraCampoPwmRecebida != mascaraParametrosEsperados)
    {
        Serial.println("ERROR;CAL_PARAMS_INCOMPLETE");
        return;
    }

    for (uint8_t i = 0; i < 3; i++)
    {
        const uint8_t bit = bitEixoCalibracao(i);

        if (mascaraParametrosEsperados & bit)
            parametrosCalibracao.eixo[i].valido = 1;
    }

    parametrosCalibracaoProntos = true;
    transferenciaParametrosCalibracaoAtiva = false;

    Serial.println("CAL_PARAMS_READY");
}

// CALIBRATION CONSULTAS / FLAGS

bool Comunicacao::possuiConfiguracaoCalibracao()
{
    return configuracaoCalibracaoRecebida;
}

SolicitacaoCalibracao Comunicacao::obterConfiguracaoCalibracao()
{
    return configuracaoCalibracao;
}

bool Comunicacao::haNovaConfiguracaoCalibracao()
{
    return novaConfiguracaoCalibracao;
}

void Comunicacao::limparNovaConfiguracaoCalibracao()
{
    novaConfiguracaoCalibracao = false;
}

bool Comunicacao::inicioCalibracaoFoiSolicitado()
{
    return inicioCalibracaoSolicitado;
}

void Comunicacao::limparSolicitacaoInicioCalibracao()
{
    inicioCalibracaoSolicitado = false;
}

bool Comunicacao::estadoCalibracaoFoiSolicitado()
{
    return estadoCalibracaoSolicitado;
}

void Comunicacao::limparSolicitacaoEstadoCalibracao()
{
    estadoCalibracaoSolicitado = false;
}

bool Comunicacao::abortoCalibracaoFoiSolicitado()
{
    return abortoCalibracaoSolicitado;
}

void Comunicacao::limparSolicitacaoAbortoCalibracao()
{
    abortoCalibracaoSolicitado = false;
}

bool Comunicacao::parametrosCalibracaoEstaoProntos()
{
    return parametrosCalibracaoProntos;
}

ParametrosCalibracao Comunicacao::obterParametrosCalibracao()
{
    return parametrosCalibracao;
}

void Comunicacao::limparParametrosCalibracaoProntos()
{
    parametrosCalibracaoProntos = false;
}

// CONSULTAS E SINALIZADORES DO CONTROLE

bool Comunicacao::inicioControleFoiSolicitado()
{
    return inicioControleSolicitado;
}

void Comunicacao::limparSolicitacaoInicioControle()
{
    inicioControleSolicitado = false;
}

bool Comunicacao::paradaControleFoiSolicitada()
{
    return paradaControleSolicitada;
}

void Comunicacao::limparSolicitacaoParadaControle()
{
    paradaControleSolicitada = false;
}

bool Comunicacao::estadoControleFoiSolicitado()
{
    return estadoControleSolicitado;
}

void Comunicacao::limparSolicitacaoEstadoControle()
{
    estadoControleSolicitado = false;
}

bool Comunicacao::limitesControleForamSolicitados()
{
    return limitesControleSolicitados;
}

void Comunicacao::limparSolicitacaoLimitesControle()
{
    limitesControleSolicitados = false;
}


bool Comunicacao::baixarControleFoiSolicitado()
{
    return baixarControleSolicitado;
}

void Comunicacao::limparSolicitacaoBaixarDadosControle()
{
    baixarControleSolicitado = false;
}

bool Comunicacao::possuiReferenciaCampo()
{
    return referenciaCampoRecebida;
}

bool Comunicacao::haNovaReferenciaCampo()
{
    return novaReferenciaCampo;
}

ReferenciaCampo Comunicacao::obterReferenciaCampo()
{
    return referenciaCampo;
}

void Comunicacao::limparNovaReferenciaCampo()
{
    novaReferenciaCampo = false;
}

// CONSULTAS E SINALIZADORES DE TRAJETORIA / SIMULACAO

bool Comunicacao::inicioTrajetoriaFoiSolicitado()
{
    return inicioTrajetoriaSolicitado;
}

void Comunicacao::limparSolicitacaoInicioTrajetoria()
{
    inicioTrajetoriaSolicitado = false;
}

CabecalhoTrajetoria Comunicacao::obterCabecalhoTrajetoria()
{
    return cabecalhoTrajetoria;
}

bool Comunicacao::pontoTrajetoriaEstaPronto()
{
    return pontoTrajetoriaPronto;
}

void Comunicacao::limparPontoTrajetoriaPronto()
{
    pontoTrajetoriaPronto = false;
}

PontoTrajetoria Comunicacao::obterPontoTrajetoria()
{
    return pontoTrajetoria;
}

bool Comunicacao::fimTrajetoriaFoiSolicitado()
{
    return fimTrajetoriaSolicitado;
}

void Comunicacao::limparSolicitacaoFimTrajetoria()
{
    fimTrajetoriaSolicitado = false;
}

bool Comunicacao::abortoTrajetoriaFoiSolicitado()
{
    return abortoTrajetoriaSolicitado;
}

void Comunicacao::limparSolicitacaoAbortoTrajetoria()
{
    abortoTrajetoriaSolicitado = false;
}

bool Comunicacao::estadoTrajetoriaFoiSolicitado()
{
    return estadoTrajetoriaSolicitado;
}

void Comunicacao::limparSolicitacaoEstadoTrajetoria()
{
    estadoTrajetoriaSolicitado = false;
}

bool Comunicacao::inicioSimulacaoFoiSolicitado()
{
    return inicioSimulacaoSolicitado;
}

void Comunicacao::limparSolicitacaoInicioSimulacao()
{
    inicioSimulacaoSolicitado = false;
}

SolicitacaoInicioSimulacao Comunicacao::obterSolicitacaoInicioSimulacao()
{
    return solicitacaoInicioSimulacao;
}

bool Comunicacao::pausaSimulacaoFoiSolicitada()
{
    return pausaSimulacaoSolicitada;
}

void Comunicacao::limparSolicitacaoPausaSimulacao()
{
    pausaSimulacaoSolicitada = false;
}

bool Comunicacao::retomadaSimulacaoFoiSolicitada()
{
    return retomadaSimulacaoSolicitada;
}

void Comunicacao::limparSolicitacaoRetomadaSimulacao()
{
    retomadaSimulacaoSolicitada = false;
}

bool Comunicacao::abortoSimulacaoFoiSolicitado()
{
    return abortoSimulacaoSolicitado;
}

void Comunicacao::limparSolicitacaoAbortoSimulacao()
{
    abortoSimulacaoSolicitado = false;
}

bool Comunicacao::estadoSimulacaoFoiSolicitado()
{
    return estadoSimulacaoSolicitado;
}

void Comunicacao::limparSolicitacaoEstadoSimulacao()
{
    estadoSimulacaoSolicitado = false;
}

bool Comunicacao::baixarSimulacaoFoiSolicitado()
{
    return baixarSimulacaoSolicitado;
}

void Comunicacao::limparSolicitacaoBaixarDadosSimulacao()
{
    baixarSimulacaoSolicitado = false;
}

bool Comunicacao::possuiHeartbeat() const
{
    return heartbeatRecebido;
}

bool Comunicacao::heartbeatEstaAtivo(uint32_t tempoLimite_ms) const
{
    if (!heartbeatRecebido)
        return false;

    return (uint32_t)(millis() - ultimoHeartbeatMs) <= tempoLimite_ms;
}

uint32_t Comunicacao::obterUltimoHeartbeatMs() const
{
    return ultimoHeartbeatMs;
}
