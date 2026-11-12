// Arquivo principal: inicializa o hardware, processa comandos e coordena experimento, calibracao, controle e simulacao.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "ConfiguracaoExperimento.h"
#include "Comunicacao.h"
#include "AcionadorDRV8871.h"
#include "SensorINA226.h"
#include "ConversorAD7606.h"
#include "RegistradorDados.h"
#include "ConfiguracoesCalibracao.h"
#include "TiposCalibracao.h"
#include "GerenciadorCalibracao.h"
#include "ConfiguracoesControle.h"
#include "TiposControle.h"
#include "ControladorCampo.h"
#include "ConfiguracoesSimulacao.h"
#include "TiposSimulacao.h"
#include "GerenciadorTrajetoria.h"

// MODO DE TESTE
// true  -> apenas imprime os comandos de PWM
// false -> aciona fisicamente os AcionadorDRV8871

#define MODO_TESTE_PWM false

// DEBUG DAS AMOSTRAS PELA SERIAL
// false -> amostras são gravadas somente no SD
// true  -> também imprime as amostras pela USB Serial
// Para funcionamento normal: false

#define DEPURACAO_AMOSTRAS_SERIAL false

// OBJETOS

// COMUNICAÇÃO

Comunicacao comunicacao;

// AcionadorDRV8871

AcionadorDRV8871 acionadorX(2, 3);
AcionadorDRV8871 acionadorY(4, 5);
AcionadorDRV8871 acionadorZ(6, 7);

// INA226

SensorINA226 sensorCorrenteX(0x40, &Wire2);
SensorINA226 sensorCorrenteY(0x41, &Wire2);
SensorINA226 sensorCorrenteZ(0x44, &Wire2);

bool inaXPronto = false;
bool inaYPronto = false;
bool inaZPronto = false;

// AD7606
// CS      = 37
// BUSY    = 14
// RESET   = 15
// CONVST  = 16
// OS0     = 17
// OS1     = 20
// OS2     = 21
// SPI:
// SCLK    = 13
// DOUT    = 12

ConversorAD7606 conversorADC(
    37,     // CS
    14,     // BUSY
    15,     // RESET
    16,     // CONVST
    17,     // OS0
    20,     // OS1
    21      // OS2
);

bool ad7606Pronto = false;

// REGISTRADOR DE DADOS

RegistradorDados registradorDados;

bool registradorDadosPronto = false;

GerenciadorTrajetoria gerenciadorTrajetoria(registradorDados);
uint32_t ultimaTelemetriaSimulacao_ms = 0;
uint32_t ultimaTelemetriaControle_ms = 0;

// CALIBRACAO AUTOMATICA

GerenciadorCalibracao gerenciadorCalibracao(
    acionadorX, acionadorY, acionadorZ,
    sensorCorrenteX, sensorCorrenteY, sensorCorrenteZ,
    conversorADC,
    registradorDados
);

ParametrosCalibracao parametrosCalibracaoAtivos = {};
bool parametrosCalibracaoCarregados = false;

// CONTROLE DIRETO DE CAMPO

ControladorCampo controladorCampo;

uint32_t inicioControle_us = 0;
uint32_t inicioControle_ms = 0;
uint32_t ultimaMalhaCampo_us = 0;
uint32_t ultimaAquisicaoCampo_us = 0;

// ESTADOS

enum EstadoExperimento
{
    OCIOSO,
    PRONTO,
    EXECUTANDO,
    CONTROLE,
    SIMULACAO,
    SIMULACAO_PAUSADA,
    CONCLUIDO,
    FALHA
};

EstadoExperimento estadoExperimento = OCIOSO;

// CONTROLE DO EXPERIMENTO

int16_t pwmAtual = 0;

unsigned long inicioPassoPWM = 0;

bool experimentoInicializado = false;

// PROTEÇÕES

bool limiteOperacionalAtingido = false;
bool limiteCriticoAtingido = false;

float correnteNoLimite = 0.0f;
char eixoNoLimite = '-';

// ESTADO PARA TEXTO

const char *estadoParaTexto(EstadoExperimento estado)
{
    switch (estado)
    {
        case OCIOSO:
            return "IDLE";

        case PRONTO:
            return "READY";

        case EXECUTANDO:
            return "RUNNING";

        case CONTROLE:
            return "CONTROL";

        case SIMULACAO:
            return "SIMULATION";

        case SIMULACAO_PAUSADA:
            return "SIMULATION_PAUSED";

        case CONCLUIDO:
            return "COMPLETE";

        case FALHA:
            return "FAULT";

        default:
            return "UNKNOWN";
    }
}

// ESTADO GLOBAL E EXCLUSAO MUTUA

bool sessaoSimulacaoEstaAtiva()
{
    return estadoExperimento == SIMULACAO ||
           estadoExperimento == SIMULACAO_PAUSADA;
}

bool controleMalhaFechadaEstaAtivo()
{
    return estadoExperimento == CONTROLE ||
           estadoExperimento == SIMULACAO;
}

bool sistemaEstaOcupado()
{
    return estadoExperimento == EXECUTANDO ||
           estadoExperimento == CONTROLE ||
           sessaoSimulacaoEstaAtiva() ||
           gerenciadorTrajetoria.estaRecebendo() ||
           gerenciadorCalibracao.estaExecutando();
}

// INA226 - MEDIA DE AMOSTRAS

uint8_t converterMediaParaINA226(uint16_t media)
{
    switch (media)
    {
        case 1:
            return INA226_1_SAMPLE;

        case 4:
            return INA226_4_SAMPLES;

        case 16:
            return INA226_16_SAMPLES;

        case 64:
            return INA226_64_SAMPLES;

        case 128:
            return INA226_128_SAMPLES;

        case 256:
            return INA226_256_SAMPLES;

        case 512:
            return INA226_512_SAMPLES;

        case 1024:
            return INA226_1024_SAMPLES;

        default:
            return INA226_16_SAMPLES;
    }
}

// INA226 - TEMPO DE CONVERSAO

uint8_t converterTempoConversaoParaINA226(uint16_t tempo_us)
{
    switch (tempo_us)
    {
        case 140:
            return INA226_140_us;

        case 204:
            return INA226_204_us;

        case 332:
            return INA226_332_us;

        case 588:
            return INA226_588_us;

        case 1100:
            return INA226_1100_us;

        case 2116:
            return INA226_2100_us;

        case 4156:
            return INA226_4200_us;

        case 8244:
            return INA226_8300_us;

        default:
            return INA226_332_us;
    }
}

// AD7606 - SOBREAMOSTRAGEM
// ConfiguracaoExperimento:
// 1  -> 1x
// 2  -> 2x
// 4  -> 4x
// 8  -> 8x
// 16 -> 16x
// 32 -> 32x
// 64 -> 64x
// Classe AD7606:
// 0 -> 1x
// 1 -> 2x
// 2 -> 4x
// 3 -> 8x
// 4 -> 16x
// 5 -> 32x
// 6 -> 64x

uint8_t converterSobreamostragemParaAD7606(
    uint8_t oversampling)
{
    switch (oversampling)
    {
        case 1:
            return 0;

        case 2:
            return 1;

        case 4:
            return 2;

        case 8:
            return 3;

        case 16:
            return 4;

        case 32:
            return 5;

        case 64:
            return 6;

        default:
            return 0;
    }
}

// CONFIGURAÇÃO DAS INA226

ConfiguracaoINA226 criarConfiguracaoINA226(
    float resistenciaShunt_Ohm)
{
    ConfiguracaoINA226 configuracaoINA;

    // correnteMaxima define a faixa usada na calibração interna do INA226.
    // Ela NÃO substitui os limites de segurança do experimento.
    configuracaoINA.correnteMaxima =
        ConfiguracoesCalibracao::CORRENTE_MAXIMA_INA_A;

    // O valor do shunt precisa corresponder ao resistor real do eixo.
    // Como I = Vshunt / Rshunt, um valor incorreto aqui produz uma
    // leitura de corrente proporcionalmente incorreta.
    configuracaoINA.resistenciaShunt =
        resistenciaShunt_Ohm;

    configuracaoINA.media =
        converterMediaParaINA226(
            ConfiguracoesCalibracao::MEDIA_INA);

    configuracaoINA.tempoConversaoShunt =
        converterTempoConversaoParaINA226(
            ConfiguracoesCalibracao::TEMPO_CONVERSAO_SHUNT_INA_US);

    configuracaoINA.tempoConversaoBarramento =
        converterTempoConversaoParaINA226(
            ConfiguracoesCalibracao::TEMPO_CONVERSAO_BARRAMENTO_INA_US);

    return configuracaoINA;
}

bool configurarINA226DoExperimento()
{
    ConfiguracaoExperimento configuracao =
        comunicacao.obterConfiguracao();

    // Cada eixo recebe sua própria resistência de shunt. Os demais
    // parâmetros do INA226 continuam iguais entre os três sensores.

    // X

    if (configuracao.mascaraEixos & 0b001)
    {
        if (!inaXPronto)
        {
            Serial.println(
                "ERROR;INA_X_NOT_READY");

            return false;
        }

        ConfiguracaoINA226 configuracaoINA =
            criarConfiguracaoINA226(
                ConfiguracoesCalibracao::
                    RESISTENCIA_SHUNT_INA_X_OHM);

        if (!sensorCorrenteX.configurar(
                configuracaoINA))
        {
            Serial.println(
                "ERROR;INA_X_CONFIG");

            return false;
        }
    }

    // Y

    if (configuracao.mascaraEixos & 0b010)
    {
        if (!inaYPronto)
        {
            Serial.println(
                "ERROR;INA_Y_NOT_READY");

            return false;
        }

        ConfiguracaoINA226 configuracaoINA =
            criarConfiguracaoINA226(
                ConfiguracoesCalibracao::
                    RESISTENCIA_SHUNT_INA_Y_OHM);

        if (!sensorCorrenteY.configurar(
                configuracaoINA))
        {
            Serial.println(
                "ERROR;INA_Y_CONFIG");

            return false;
        }
    }

    // Z

    if (configuracao.mascaraEixos & 0b100)
    {
        if (!inaZPronto)
        {
            Serial.println(
                "ERROR;INA_Z_NOT_READY");

            return false;
        }

        ConfiguracaoINA226 configuracaoINA =
            criarConfiguracaoINA226(
                ConfiguracoesCalibracao::
                    RESISTENCIA_SHUNT_INA_Z_OHM);

        if (!sensorCorrenteZ.configurar(
                configuracaoINA))
        {
            Serial.println(
                "ERROR;INA_Z_CONFIG");

            return false;
        }
    }

    return true;
}

// CONFIGURAÇÃO DO AD7606

bool configurarAD7606DoExperimento()
{
    const uint8_t sobreamostragem =
        converterSobreamostragemParaAD7606(
            ConfiguracoesCalibracao::SOBREAMOSTRAGEM_AD7606
        );

    return conversorADC.definirSobreamostragem(sobreamostragem);
}

// APLICACAO DO PWM

void aplicarPWM(int16_t pwm)
{
    ConfiguracaoExperimento configuracao =
        comunicacao.obterConfiguracao();

    // X

    if (configuracao.mascaraEixos & 0b001)
    {
        if (MODO_TESTE_PWM)
        {
            Serial.print(
                "PWM_COMMAND;X=");

            Serial.print(
                pwm);

            Serial.println(";");
        }
        else
        {
            acionadorX.definirPWM(pwm);
        }
    }
    else
    {
        if (!MODO_TESTE_PWM)
            acionadorX.parar();
    }

    // Y

    if (configuracao.mascaraEixos & 0b010)
    {
        if (MODO_TESTE_PWM)
        {
            Serial.print(
                "PWM_COMMAND;Y=");

            Serial.print(
                pwm);

            Serial.println(";");
        }
        else
        {
            acionadorY.definirPWM(pwm);
        }
    }
    else
    {
        if (!MODO_TESTE_PWM)
            acionadorY.parar();
    }

    // Z

    if (configuracao.mascaraEixos & 0b100)
    {
        if (MODO_TESTE_PWM)
        {
            Serial.print(
                "PWM_COMMAND;Z=");

            Serial.print(
                pwm);

            Serial.println(";");
        }
        else
        {
            acionadorZ.definirPWM(pwm);
        }
    }
    else
    {
        if (!MODO_TESTE_PWM)
            acionadorZ.parar();
    }
}

// PARADA DE TODOS OS EIXOS

void pararTodosEixos()
{
    if (MODO_TESTE_PWM)
    {
        Serial.println(
            "PWM_COMMAND;ALL=0");
    }
    else
    {
        acionadorX.parar();
        acionadorY.parar();
        acionadorZ.parar();
    }
}

// CALCULO DO PROXIMO PWM

int16_t calcularProximoPWM(
    int16_t pwm,
    const ConfiguracaoExperimento &configuracao)
{
    int32_t proximoPWM =
        (int32_t)pwm +
        configuracao.passoPWM;

    // Sweep crescente

    if (configuracao.passoPWM > 0)
    {
        if (proximoPWM > configuracao.pwmFinal)
        {
            proximoPWM =
                configuracao.pwmFinal;
        }
    }

    // Sweep decrescente

    else
    {
        if (proximoPWM < configuracao.pwmFinal)
        {
            proximoPWM =
                configuracao.pwmFinal;
        }
    }

    // Limites absolutos

    if (proximoPWM > 4095)
        proximoPWM = 4095;

    if (proximoPWM < -4095)
        proximoPWM = -4095;

    return (int16_t)proximoPWM;
}

// PROTECAO DE CORRENTE

void verificarProtecaoCorrente(
    char eixo,
    float corrente)
{
    ConfiguracaoExperimento configuracao =
        comunicacao.obterConfiguracao();

    float correnteAbsoluta =
        fabsf(corrente);

    // LIMITE CRÍTICO

    if (
        correnteAbsoluta >=
        configuracao.limiteCorrenteCritico
    )
    {
        if (!limiteCriticoAtingido)
        {
            limiteCriticoAtingido = true;

            correnteNoLimite =
                corrente;

            eixoNoLimite =
                eixo;

            // Guarda PWM antes de zerá-lo

            int16_t pwmNoLimite =
                pwmAtual;

            // Cria evento

            char evento[120];

            snprintf(
                evento,
                sizeof(evento),
                "CURRENT_HARD_LIMIT;axis=%c;PWM=%d;I=%.6f;LIMIT=%.6f",
                eixo,
                pwmNoLimite,
                corrente,
                configuracao.limiteCorrenteCritico
            );

            // Registra no SD

            registradorDados.registrarEvento(
                evento
            );

            // Registra na Serial

            Serial.println(
                evento
            );

            // Para TODOS os eixos

            pararTodosEixos();

            // Finaliza experimento como FALHA

            registradorDados.abortarExperimento();

            pwmAtual =
                0;

            experimentoInicializado =
                false;

            estadoExperimento =
                FALHA;

            Serial.println(
                "FAULT"
            );
        }

        return;
    }

    // LIMITE OPERACIONAL
    // Interrompe a progressao normal da varredura sem remover a protecao critica; serve para nao operar continuamente perto do limite de corrente.

    if (
        correnteAbsoluta >=
        configuracao.limiteCorrenteOperacional
    )
    {
        if (!limiteOperacionalAtingido)
        {
            limiteOperacionalAtingido = true;

            correnteNoLimite =
                corrente;

            eixoNoLimite =
                eixo;

            // Cria evento

            char evento[120];

            snprintf(
                evento,
                sizeof(evento),
                "CURRENT_SOFT_LIMIT;axis=%c;PWM=%d;I=%.6f;LIMIT=%.6f",
                eixo,
                pwmAtual,
                corrente,
                configuracao.limiteCorrenteOperacional
            );

            // Registra no SD

            registradorDados.registrarEvento(
                evento
            );

            // Registra na Serial

            Serial.println(
                evento
            );
        }
    }
}

// MESCLA DOS PARAMETROS DE CALIBRACAO PROCESSADOS PELO MATLAB

void mesclarParametrosCalibracao(
    ParametrosCalibracao &destino,
    const ParametrosCalibracao &origem
)
{
    if (destino.assinatura != ASSINATURA_PARAMETROS_CALIBRACAO ||
        destino.versao != VERSAO_PARAMETROS_CALIBRACAO)
    {
        memset(&destino, 0, sizeof(destino));
        destino.assinatura = ASSINATURA_PARAMETROS_CALIBRACAO;
        destino.versao = VERSAO_PARAMETROS_CALIBRACAO;
    }

    for (uint8_t i = 0; i < 3; i++)
    {
        const uint8_t bit = bitEixoCalibracao(i);

        if (origem.mascaraEixosValidos & bit)
        {
            destino.eixo[i] = origem.eixo[i];
            destino.eixo[i].valido = 1;
            destino.mascaraEixosValidos |= bit;
        }
    }
}

// FUNCOES AUXILIARES DO CONTROLE DIRETO DE CAMPO

bool hardwareEixoProntoParaControle(
    uint8_t eixo
)
{
    if (!ConfiguracoesControle::MONITORAR_INA_NO_CONTROLE)
        return true;

    if (eixo == 0)
        return inaXPronto;

    if (eixo == 1)
        return inaYPronto;

    if (eixo == 2)
        return inaZPronto;

    return false;
}

bool referenciaCampoDentroDosLimites(
    const ReferenciaCampo &referencia,
    bool imprimirErro
)
{
    for (uint8_t eixo = 0; eixo < 3; eixo++)
    {
        if (!controladorCampo.eixoEstaHabilitado(eixo))
            continue;

        float minimo_uT = 0.0f;
        float maximo_uT = 0.0f;

        if (!controladorCampo.obterLimitesOperacionaisCampo(
                eixo,
                minimo_uT,
                maximo_uT))
        {
            if (imprimirErro)
            {
                Serial.print("ERROR;FIELD_LIMIT_UNAVAILABLE;AXIS=");
                Serial.println(letraEixoCalibracao(eixo));
            }
            return false;
        }

        const float valor = referencia.B_uT[eixo];

        if (!isfinite(valor) ||
            valor < minimo_uT ||
            valor > maximo_uT)
        {
            if (imprimirErro)
            {
                Serial.print("ERROR;FIELD_REF_RANGE;AXIS=");
                Serial.print(letraEixoCalibracao(eixo));
                Serial.print(";VALUE=");
                Serial.print(valor, 6);
                Serial.print(";MIN=");
                Serial.print(minimo_uT, 6);
                Serial.print(";MAX=");
                Serial.println(maximo_uT, 6);
            }
            return false;
        }
    }

    return true;
}

void imprimirLimitesControle()
{
    Serial.print("CONTROL_LIMITS");

    for (uint8_t eixo = 0; eixo < 3; eixo++)
    {
        if (!controladorCampo.eixoEstaHabilitado(eixo))
            continue;

        float minimo_uT = 0.0f;
        float maximo_uT = 0.0f;

        if (!controladorCampo.obterLimitesOperacionaisCampo(
                eixo,
                minimo_uT,
                maximo_uT))
        {
            Serial.print(";");
            Serial.print(letraEixoCalibracao(eixo));
            Serial.print("_VALID=0");
            continue;
        }

        const char a = letraEixoCalibracao(eixo);
        Serial.print(";"); Serial.print(a); Serial.print("_MIN=");
        Serial.print(minimo_uT, 6);
        Serial.print(";"); Serial.print(a); Serial.print("_MAX=");
        Serial.print(maximo_uT, 6);
    }

    Serial.println();
}

void registrarEventoReferenciaManual(
    const ReferenciaCampo &referencia
)
{
    if (!registradorDados.controleManualEstaAtivo())
        return;

    char evento[128];
    snprintf(
        evento,
        sizeof(evento),
        "FIELD_REF;BX=%.6f;BY=%.6f;BZ=%.6f",
        referencia.B_uT[0],
        referencia.B_uT[1],
        referencia.B_uT[2]
    );

    registradorDados.registrarEventoControleManual(evento);
}

void aplicarPWMControle()
{
    const int16_t pwmX =
        controladorCampo.eixoEstaHabilitado(0)
        ? controladorCampo.obterPWM(0)
        : 0;

    const int16_t pwmY =
        controladorCampo.eixoEstaHabilitado(1)
        ? controladorCampo.obterPWM(1)
        : 0;

    const int16_t pwmZ =
        controladorCampo.eixoEstaHabilitado(2)
        ? controladorCampo.obterPWM(2)
        : 0;

    if (MODO_TESTE_PWM)
    {
        return;
    }

    acionadorX.definirPWM(pwmX);
    acionadorY.definirPWM(pwmY);
    acionadorZ.definirPWM(pwmZ);
}

void pararControlePorFalha(
    const char *motivo
)
{
    const bool falhaSimulacao =
        sessaoSimulacaoEstaAtiva() ||
        gerenciadorTrajetoria.estaExecutando() ||
        gerenciadorTrajetoria.estaPausado() ||
        registradorDados.simulacaoEstaAtiva();

    const bool falhaControleManual =
        estadoExperimento == CONTROLE ||
        registradorDados.controleManualEstaAtivo();

    pararTodosEixos();
    controladorCampo.reiniciarDinamica();

    if (falhaSimulacao)
    {
        gerenciadorTrajetoria.abortarReproducao();

        if (registradorDados.simulacaoEstaAtiva())
            registradorDados.abortarSimulacao(motivo ? motivo : "UNKNOWN");

        Serial.print("SIM_FAULT;REASON=");
        Serial.println(motivo ? motivo : "UNKNOWN");
        Serial.println("SIM_DOWNLOAD_READY");
    }
    else if (falhaControleManual)
    {
        if (registradorDados.controleManualEstaAtivo())
            registradorDados.abortarControleManual(motivo ? motivo : "UNKNOWN");

        Serial.print("CONTROL_FAULT;REASON=");
        Serial.println(motivo ? motivo : "UNKNOWN");
        Serial.println("CONTROL_DOWNLOAD_READY");
    }
    else
    {
        Serial.print("CONTROL_FAULT;REASON=");
        Serial.println(motivo ? motivo : "UNKNOWN");
    }

    pwmAtual = 0;
    estadoExperimento = FALHA;
}

bool iniciarControleCampo()
{
    if (estadoExperimento == FALHA)
    {
        Serial.println(
            "ERROR;FAULT_CLEAR_REQUIRED"
        );

        return false;
    }

    if (sistemaEstaOcupado())
    {
        Serial.println(
            "ERROR;SYSTEM_BUSY"
        );

        return false;
    }

    if (!registradorDadosPronto)
    {
        Serial.println("ERROR;SD_NOT_READY");
        return false;
    }

    if (!parametrosCalibracaoCarregados)
    {
        Serial.println(
            "ERROR;CALIBRATION_PARAMS_NOT_LOADED"
        );

        return false;
    }

    if (!comunicacao.possuiReferenciaCampo())
    {
        Serial.println(
            "ERROR;FIELD_REF_NOT_SET"
        );

        return false;
    }

    controladorCampo.definirMascaraEixos(
        ConfiguracoesControle::MASCARA_EIXOS
    );

    controladorCampo.carregarCalibracao(
        parametrosCalibracaoAtivos
    );

    if (!controladorCampo.estaPronto())
    {
        Serial.println(
            "ERROR;CONTROL_CALIBRATION_INVALID"
        );

        return false;
    }

    for (uint8_t eixo = 0; eixo < 3; eixo++)
    {
        if (!controladorCampo.eixoEstaHabilitado(eixo))
            continue;

        if (!hardwareEixoProntoParaControle(eixo))
        {
            Serial.print(
                "ERROR;CONTROL_INA_NOT_READY;AXIS="
            );

            Serial.println(
                letraEixoCalibracao(eixo)
            );

            return false;
        }
    }

    if (!ad7606Pronto)
    {
        Serial.println(
            "ERROR;CONTROL_AD7606_NOT_READY"
        );

        return false;
    }

    const ReferenciaCampo referencia =
        comunicacao.obterReferenciaCampo();

    if (!referenciaCampoDentroDosLimites(referencia, true))
        return false;

    pararTodosEixos();

    controladorCampo.reiniciarDinamica();

    controladorCampo.definirReferenciaCampo(referencia);

    if (!registradorDados.iniciarControleManual(referencia))
    {
        controladorCampo.reiniciarDinamica();
        Serial.println("ERROR;CONTROL_SD_START");
        return false;
    }

    const uint32_t agora_us =
        micros();

    inicioControle_us =
        agora_us;

    inicioControle_ms = millis();

    // Faz com que a primeira aquisicao/iteracao ocorra
    // assim que o loop de controle comecar.

    ultimaMalhaCampo_us =
        agora_us -
        ConfiguracoesControle::PERIODO_MALHA_CAMPO_US;

    ultimaAquisicaoCampo_us =
        agora_us -
        ConfiguracoesControle::PERIODO_AQUISICAO_CAMPO_US;

    pwmAtual = 0;

    ultimaTelemetriaControle_ms =
        millis() - ConfiguracoesControle::PERIODO_TELEMETRIA_MANUAL_MS;

    estadoExperimento =
        CONTROLE;

    Serial.println(
        "CONTROL_STARTED"
    );

    imprimirLimitesControle();

    return true;
}

void pararControleCampo()
{
    pararTodosEixos();
    controladorCampo.reiniciarDinamica();
    pwmAtual = 0;

    if (registradorDados.controleManualEstaAtivo())
    {
        if (!registradorDados.finalizarControleManual())
        {
            estadoExperimento = FALHA;
            Serial.println("CONTROL_FAULT;REASON=SD_FINISH");
            return;
        }
    }

    estadoExperimento = CONCLUIDO;

    Serial.println("CONTROL_COMPLETE");
    Serial.println("CONTROL_DOWNLOAD_READY");
}

void atualizarMedicoesCorrenteControle()
{
    if (!ConfiguracoesControle::MONITORAR_INA_NO_CONTROLE) return;
    SensorINA226 *sensores[3] =
    {
        &sensorCorrenteX,
        &sensorCorrenteY,
        &sensorCorrenteZ
    };

    for (uint8_t eixo = 0; eixo < 3; eixo++)
    {
        if (!controladorCampo.eixoEstaHabilitado(eixo))
            continue;

        SensorINA226 *sensor =
            sensores[eixo];

        if (!sensor->conversaoEstaPronta())
            continue;

        const uint32_t instante_us =
            micros();

        const float corrente_A =
            sensor->lerCorrente();

        if (!isfinite(corrente_A))
        {
            pararControlePorFalha(
                "INA226_READ"
            );

            return;
        }

        const float limiteCritico_A =
            controladorCampo.obterLimiteCritico(
                eixo
            );

        if (
            limiteCritico_A > 0.0f &&
            fabsf(corrente_A) >= limiteCritico_A
        )
        {
            Serial.print(
                "CONTROL_HARD_LIMIT;AXIS="
            );

            Serial.print(
                letraEixoCalibracao(eixo)
            );

            Serial.print(
                ";I="
            );

            Serial.print(
                corrente_A,
                6
            );

            Serial.print(
                ";LIMIT="
            );

            Serial.println(
                limiteCritico_A,
                6
            );

            pararControlePorFalha(
                "HARD_CURRENT_LIMIT"
            );

            return;
        }

        controladorCampo.definirMedicaoCorrente(
            eixo,
            corrente_A,
            instante_us
        );
    }
}

bool atualizarMedicaoCampoControle(
    uint32_t agora_us
)
{
    if (
        (uint32_t)(
            agora_us -
            ultimaAquisicaoCampo_us
        )
        <
        ConfiguracoesControle::PERIODO_AQUISICAO_CAMPO_US
    )
    {
        return false;
    }

    ultimaAquisicaoCampo_us = agora_us;

    AmostraAD7606 amostra;

    const bool conversaoOK = conversorADC.lerAmostra(amostra);

    if (!conversaoOK)
    {
        pararControlePorFalha(
            "AD7606_READ"
        );

        return false;
    }

    float B_uT[3];

    for (uint8_t eixo = 0; eixo < 3; eixo++)
    {
        B_uT[eixo] =
            amostra.tensao[eixo] /
            ConfiguracoesCalibracao::
                SENSIBILIDADE_FLUXGATE_V_POR_UT;
    }

    controladorCampo.definirMedicaoCampo(
        B_uT,
        amostra.instante
    );

    return true;
}

// MALHA DE CAMPO (PI DIRETO)

void atualizarMalhaCampoControle(
    uint32_t agora_us
)
{
    if (
        (uint32_t)(
            agora_us -
            ultimaMalhaCampo_us
        )
        <
        ConfiguracoesControle::PERIODO_MALHA_CAMPO_US
    )
    {
        return;
    }

    ultimaMalhaCampo_us =
        agora_us;

    const bool controleOK = controladorCampo.atualizarMalhaCampo(agora_us);

    if (!controleOK)
    {
        if (
            (uint32_t)(
                agora_us -
                inicioControle_us
            )
            >
            ConfiguracoesControle::TEMPO_TOLERANCIA_INICIAL_US
        )
        {
            pararControlePorFalha(
                "FIELD_STALE"
            );
        }
    }
    else
    {
        // Aplica PWM com o mesmo comando e a mesma cadencia anteriores.
        aplicarPWMControle();
    }
}

bool registrarAmostraControleManual()
{
    if (estadoExperimento != CONTROLE ||
        !registradorDados.controleManualEstaAtivo())
    {
        return true;
    }

    RegistroControleManual registro = {};

    registro.tempoDecorrido_ms =
        (uint32_t)(millis() - inicioControle_ms);

    for (uint8_t eixo = 0; eixo < 3; eixo++)
    {
        const EstadoControleEixo &estado =
            controladorCampo.obterEstado(eixo);

        registro.referenciaCampo_uT[eixo] =
            estado.referenciaCampo_uT;
        registro.campoMedido_uT[eixo] =
            estado.campoMedido_uT;
        registro.referenciaCorrente_A[eixo] =
            estado.referenciaCorrente_A;
        registro.correnteMedida_A[eixo] =
            estado.correnteMedida_A;
        registro.pwm[eixo] =
            estado.comandoPWM;

        if (estado.correnteValida)
            registro.indicadores |= (uint16_t)(1u << eixo);

        if (estado.campoValido)
            registro.indicadores |= (uint16_t)(1u << (eixo + 3));
    }

    return registradorDados.registrarControleManual(registro);
}

bool registrarAmostraSimulacao()
{
    if (estadoExperimento != SIMULACAO ||
        !registradorDados.simulacaoEstaAtiva())
    {
        return true;
    }

    RegistroSimulacao registro = {};
    const uint32_t agora_ms = millis();

    registro.tempoRealDecorrido_ms =
        gerenciadorTrajetoria.obterTempoRealDecorrido_ms(agora_ms);

    registro.indiceTrajetoria =
        gerenciadorTrajetoria.obterIndiceAtual();

    registro.tempoSimulado_s =
        gerenciadorTrajetoria.obterTempoSimulado_s(agora_ms);

    for (uint8_t eixo = 0; eixo < 3; eixo++)
    {
        const EstadoControleEixo &estado =
            controladorCampo.obterEstado(eixo);

        registro.referenciaCampo_uT[eixo] =
            estado.referenciaCampo_uT;

        registro.campoMedido_uT[eixo] =
            estado.campoMedido_uT;

        registro.referenciaCorrente_A[eixo] =
            estado.referenciaCorrente_A;

        registro.correnteMedida_A[eixo] =
            estado.correnteMedida_A;

        registro.pwm[eixo] =
            estado.comandoPWM;

        if (estado.correnteValida)
            registro.indicadores |= (uint16_t)(1u << eixo);

        if (estado.campoValido)
            registro.indicadores |= (uint16_t)(1u << (eixo + 3));
    }

    return registradorDados.registrarSimulacao(registro);
}

void executarControleCampo()
{
    if (!controleMalhaFechadaEstaAtivo())
        return;

    atualizarMedicoesCorrenteControle();

    if (!controleMalhaFechadaEstaAtivo())
        return;

    const uint32_t agora_us = micros();

    const bool novaAmostraCampo =
        atualizarMedicaoCampoControle(agora_us);

    if (!controleMalhaFechadaEstaAtivo())
        return;

    if (novaAmostraCampo && estadoExperimento == CONTROLE)
    {
        const bool registroOK = registrarAmostraControleManual();
        if (!registroOK)
        {
            pararControlePorFalha("SD_CONTROL_WRITE");
            return;
        }
    }

    if (novaAmostraCampo && estadoExperimento == SIMULACAO)
    {
        if (!registrarAmostraSimulacao())
        {
            pararControlePorFalha("SD_SIM_WRITE");
            return;
        }
    }

    // Usar o instante ATUAL, posterior a leitura do AD7606 e ao
    // registro da amostra. O valor agora_us foi obtido antes da
    // conversao: como a amostra.instante e gerada no final da leitura,
    // passá-lo ao PI pode causar underflow em (agora_us - instanteCampo)
    // e provocar FIELD_STALE indevidamente.
    atualizarMalhaCampoControle(micros());

    if (!controleMalhaFechadaEstaAtivo())
        return;

}

bool prepararControladorSimulacao(
    const ReferenciaCampo &referencia
)
{
    if (!parametrosCalibracaoCarregados)
    {
        Serial.println("ERROR;CALIBRATION_PARAMS_NOT_LOADED");
        return false;
    }

    controladorCampo.definirMascaraEixos(ConfiguracoesControle::MASCARA_EIXOS);
    controladorCampo.carregarCalibracao(parametrosCalibracaoAtivos);

    if (!controladorCampo.estaPronto())
    {
        Serial.println("ERROR;CONTROL_CALIBRATION_INVALID");
        return false;
    }

    for (uint8_t eixo = 0; eixo < 3; eixo++)
    {
        if (!controladorCampo.eixoEstaHabilitado(eixo))
            continue;

        if (!hardwareEixoProntoParaControle(eixo))
        {
            Serial.print("ERROR;CONTROL_INA_NOT_READY;AXIS=");
            Serial.println(letraEixoCalibracao(eixo));
            return false;
        }
    }

    if (!ad7606Pronto)
    {
        Serial.println("ERROR;CONTROL_AD7606_NOT_READY");
        return false;
    }

    if (!referenciaCampoDentroDosLimites(referencia, true))
        return false;

    pararTodosEixos();
    controladorCampo.reiniciarDinamica();
    controladorCampo.definirReferenciaCampo(referencia);

    const uint32_t agora_us = micros();

    inicioControle_us = agora_us;
    ultimaMalhaCampo_us =
        agora_us - ConfiguracoesControle::PERIODO_MALHA_CAMPO_US;
    ultimaAquisicaoCampo_us =
        agora_us - ConfiguracoesControle::PERIODO_AQUISICAO_CAMPO_US;

    pwmAtual = 0;
    return true;
}

void enviarTelemetriaControle()
{
    if (estadoExperimento != CONTROLE)
        return;

    const uint32_t agora_ms = millis();

    if ((uint32_t)(agora_ms - ultimaTelemetriaControle_ms) <
        ConfiguracoesControle::PERIODO_TELEMETRIA_MANUAL_MS)
    {
        return;
    }

    ultimaTelemetriaControle_ms = agora_ms;

    const float tempoDecorrido_s =
        (float)((uint32_t)(agora_ms - inicioControle_ms)) * 1.0e-3f;

    Serial.print("CONTROL_TELEMETRY;STATE=CONTROL;T=");
    Serial.print(tempoDecorrido_s, 6);

    for (uint8_t eixo = 0; eixo < 3; eixo++)
    {
        if (!controladorCampo.eixoEstaHabilitado(eixo))
            continue;

        const EstadoControleEixo &estadoEixo =
            controladorCampo.obterEstado(eixo);
        const char a = letraEixoCalibracao(eixo);

        Serial.print(";"); Serial.print(a); Serial.print("_BREF=");
        Serial.print(estadoEixo.referenciaCampo_uT, 6);
        Serial.print(";"); Serial.print(a); Serial.print("_B=");
        Serial.print(estadoEixo.campoMedido_uT, 6);
        Serial.print(";"); Serial.print(a); Serial.print("_IREF=");
        Serial.print(estadoEixo.referenciaCorrente_A, 6);
        Serial.print(";"); Serial.print(a); Serial.print("_I=");
        Serial.print(estadoEixo.correnteMedida_A, 6);
        Serial.print(";"); Serial.print(a); Serial.print("_PWM=");
        Serial.print(estadoEixo.comandoPWM);
    }

    Serial.println();
}

void imprimirEstadoTrajetoria()
{
    const InformacoesTrajetoria &informacoes = gerenciadorTrajetoria.obterInformacoes();

    Serial.print("TRAJ_STATUS;STATE=");
    Serial.print(gerenciadorTrajetoria.obterNomeEstado());
    Serial.print(";RECEIVED=");
    Serial.print(gerenciadorTrajetoria.obterQuantidadePontosRecebidos());
    Serial.print(";EXPECTED=");
    Serial.print(gerenciadorTrajetoria.obterQuantidadePontosEsperados());

    if (gerenciadorTrajetoria.estaPronto() ||
        gerenciadorTrajetoria.estaExecutando() ||
        gerenciadorTrajetoria.estaPausado())
    {
        Serial.print(";ID=");
        Serial.print(informacoes.id);
        Serial.print(";N=");
        Serial.print(informacoes.quantidadePontos);
        Serial.print(";MIN_DT=");
        Serial.print(informacoes.menorIntervalo_s, 6);
        Serial.print(";MAX_ACCEL=");
        Serial.print(gerenciadorTrajetoria.obterAceleracaoMaximaPermitida(), 6);
    }

    Serial.println();
}

void imprimirEstadoSimulacao()
{
    const uint32_t agora_ms = millis();

    Serial.print("SIM_STATUS;STATE=");
    Serial.print(estadoParaTexto(estadoExperimento));
    Serial.print(";TRAJ_STATE=");
    Serial.print(gerenciadorTrajetoria.obterNomeEstado());
    Serial.print(";K=");
    Serial.print(gerenciadorTrajetoria.obterIndiceAtual());
    Serial.print(";T=");
    Serial.print(gerenciadorTrajetoria.obterTempoSimulado_s(agora_ms), 6);
    Serial.print(";PROGRESS=");
    Serial.print(gerenciadorTrajetoria.obterProgressoPercentual(agora_ms), 3);
    Serial.print(";ACCEL=");
    Serial.print(gerenciadorTrajetoria.obterAceleracao(), 6);
    Serial.print(";REF_RATE_MAX=");
    Serial.print(gerenciadorTrajetoria.obterTaxaMaximaReferenciaHz(), 3);
    Serial.print(";HEARTBEAT=");
    Serial.println(
        comunicacao.heartbeatEstaAtivo(
            ConfiguracoesSimulacao::TEMPO_LIMITE_HEARTBEAT_MS
        ) ? "OK" : "LOST"
    );
}

void enviarTelemetriaSimulacao()
{
    if (!sessaoSimulacaoEstaAtiva())
        return;

    const uint32_t agora_ms = millis();

    if ((uint32_t)(agora_ms - ultimaTelemetriaSimulacao_ms) <
        ConfiguracoesSimulacao::PERIODO_TELEMETRIA_MS)
    {
        return;
    }

    ultimaTelemetriaSimulacao_ms = agora_ms;

    Serial.print("SIM_TELEMETRY;STATE=");
    Serial.print(estadoParaTexto(estadoExperimento));
    Serial.print(";T=");
    Serial.print(gerenciadorTrajetoria.obterTempoSimulado_s(agora_ms), 6);
    Serial.print(";K=");
    Serial.print(gerenciadorTrajetoria.obterIndiceAtual());
    Serial.print(";P=");
    Serial.print(gerenciadorTrajetoria.obterProgressoPercentual(agora_ms), 3);

    for (uint8_t eixo = 0; eixo < 3; eixo++)
    {
        const EstadoControleEixo &estadoEixo = controladorCampo.obterEstado(eixo);
        const char a = letraEixoCalibracao(eixo);

        Serial.print(";"); Serial.print(a); Serial.print("_BREF=");
        Serial.print(estadoEixo.referenciaCampo_uT, 6);

        Serial.print(";"); Serial.print(a); Serial.print("_B=");
        Serial.print(estadoEixo.campoMedido_uT, 6);

        Serial.print(";"); Serial.print(a); Serial.print("_IREF=");
        Serial.print(estadoEixo.referenciaCorrente_A, 6);

        Serial.print(";"); Serial.print(a); Serial.print("_I=");
        Serial.print(estadoEixo.correnteMedida_A, 6);

        Serial.print(";"); Serial.print(a); Serial.print("_PWM=");
        Serial.print(
            estadoExperimento == SIMULACAO_PAUSADA
            ? 0
            : estadoEixo.comandoPWM
        );
    }

    Serial.println();
}

bool iniciarSimulacaoTrajetoria(float aceleracao)
{
    if (estadoExperimento == FALHA)
    {
        Serial.println("ERROR;FAULT_CLEAR_REQUIRED");
        return false;
    }

    if (sistemaEstaOcupado())
    {
        Serial.println("ERROR;SYSTEM_BUSY");
        return false;
    }

    if (!gerenciadorTrajetoria.estaPronto())
    {
        Serial.println("ERROR;TRAJECTORY_NOT_READY");
        return false;
    }

    if (!comunicacao.heartbeatEstaAtivo(
            ConfiguracoesSimulacao::TEMPO_LIMITE_HEARTBEAT_MS))
    {
        Serial.println("ERROR;HEARTBEAT_REQUIRED");
        return false;
    }

    if (!registradorDadosPronto)
    {
        Serial.println("ERROR;SD_NOT_READY");
        return false;
    }

    const float aceleracaoMaxima =
        gerenciadorTrajetoria.obterAceleracaoMaximaPermitida();

    if (!isfinite(aceleracao) || aceleracao <= 0.0f ||
        aceleracao > aceleracaoMaxima + 1.0e-5f)
    {
        Serial.print("ERROR;SIM_ACCEL_TOO_HIGH;MAX=");
        Serial.println(aceleracaoMaxima, 6);
        return false;
    }

    const uint32_t agora_ms = millis();

    if (!gerenciadorTrajetoria.iniciar(aceleracao, agora_ms))
    {
        Serial.println("ERROR;TRAJECTORY_START");
        return false;
    }

    if (!registradorDados.iniciarSimulacao(
            gerenciadorTrajetoria.obterInformacoes(),
            aceleracao))
    {
        gerenciadorTrajetoria.abortarReproducao();
        Serial.println("ERROR;SIM_SD_START");
        return false;
    }

    if (!prepararControladorSimulacao(
            gerenciadorTrajetoria.obterReferenciaAtual()))
    {
        gerenciadorTrajetoria.abortarReproducao();
        registradorDados.abortarSimulacao("CONTROL_PREPARE");
        return false;
    }

    estadoExperimento = SIMULACAO;
    ultimaTelemetriaSimulacao_ms =
        agora_ms - ConfiguracoesSimulacao::PERIODO_TELEMETRIA_MS;

    Serial.print("SIM_STARTED;ACCEL=");
    Serial.print(aceleracao, 6);
    Serial.print(";MAX_REF_RATE=");
    Serial.println(
        gerenciadorTrajetoria.obterTaxaMaximaReferenciaHz(),
        3
    );

    return true;
}

void pausarSimulacaoTrajetoria()
{
    if (estadoExperimento != SIMULACAO)
    {
        Serial.println("ERROR;SIM_NOT_RUNNING");
        return;
    }

    if (!gerenciadorTrajetoria.pausar(millis()))
    {
        pararControlePorFalha("SIM_PAUSE");
        return;
    }

    pararTodosEixos();
    estadoExperimento = SIMULACAO_PAUSADA;
    registradorDados.registrarEventoSimulacao("SIMULATION_PAUSED");

    Serial.print("SIM_PAUSED;K=");
    Serial.print(gerenciadorTrajetoria.obterIndiceAtual());
    Serial.print(";T=");
    Serial.println(
        gerenciadorTrajetoria.obterTempoSimulado_s(millis()),
        6
    );
}

void retomarSimulacaoTrajetoria()
{
    if (estadoExperimento != SIMULACAO_PAUSADA)
    {
        Serial.println("ERROR;SIM_NOT_PAUSED");
        return;
    }

    // O heartbeat confirma que o computador supervisor continua presente. Perde-lo durante a simulacao desliga as bobinas por seguranca.
    if (!comunicacao.heartbeatEstaAtivo(
            ConfiguracoesSimulacao::TEMPO_LIMITE_HEARTBEAT_MS))
    {
        pararControlePorFalha("USB_TIMEOUT");
        return;
    }

    if (!gerenciadorTrajetoria.retomar(millis()))
    {
        pararControlePorFalha("SIM_RESUME");
        return;
    }

    if (!prepararControladorSimulacao(
            gerenciadorTrajetoria.obterReferenciaAtual()))
    {
        pararControlePorFalha("CONTROL_PREPARE");
        return;
    }

    estadoExperimento = SIMULACAO;
    registradorDados.registrarEventoSimulacao("SIMULATION_RESUMED");
    Serial.println("SIM_RESUMED");
}

void abortarSimulacaoTrajetoria()
{
    if (!sessaoSimulacaoEstaAtiva())
    {
        Serial.println("ERROR;SIM_NOT_ACTIVE");
        return;
    }

    pararTodosEixos();
    controladorCampo.reiniciarDinamica();
    gerenciadorTrajetoria.abortarReproducao();

    if (registradorDados.simulacaoEstaAtiva())
        registradorDados.abortarSimulacao("USER_ABORT");

    estadoExperimento = comunicacao.possuiConfiguracao()
        ? PRONTO
        : OCIOSO;

    Serial.println("SIM_ABORTED");
    Serial.println("SIM_DOWNLOAD_READY");
}

void finalizarSimulacaoTrajetoria()
{
    pararTodosEixos();
    controladorCampo.reiniciarDinamica();

    if (!registradorDados.finalizarSimulacao())
    {
        estadoExperimento = FALHA;
        Serial.println("SIM_FAULT;REASON=SD_FINISH");
        return;
    }

    estadoExperimento = CONCLUIDO;
    Serial.println("SIMULATION_COMPLETE");
    Serial.println("SIM_DOWNLOAD_READY");
}

void executarSimulacaoTrajetoria()
{
    if (!sessaoSimulacaoEstaAtiva())
        return;

    if (!comunicacao.heartbeatEstaAtivo(
            ConfiguracoesSimulacao::TEMPO_LIMITE_HEARTBEAT_MS))
    {
        pararControlePorFalha("USB_TIMEOUT");
        return;
    }

    if (estadoExperimento == SIMULACAO_PAUSADA)
    {
        registradorDados.atenderGravacao();
        enviarTelemetriaSimulacao();
        return;
    }

    const bool referenciaMudou =
        gerenciadorTrajetoria.atualizar(millis());

    if (gerenciadorTrajetoria.estaEmFalha())
    {
        pararControlePorFalha(
            gerenciadorTrajetoria.houveAtrasoTemporizacao()
            ? "TRAJECTORY_TIMING_OVERRUN"
            : "TRAJECTORY_READ"
        );
        return;
    }

    if (referenciaMudou)
    {
        const ReferenciaCampo referencia =
            gerenciadorTrajetoria.obterReferenciaAtual();

        if (!referenciaCampoDentroDosLimites(referencia, false))
        {
            pararControlePorFalha("FIELD_REF_OUT_OF_RANGE");
            return;
        }

        controladorCampo.definirReferenciaCampo(referencia);
    }

    if (gerenciadorTrajetoria.estaConcluido())
    {
        finalizarSimulacaoTrajetoria();
        return;
    }

    executarControleCampo();

    if (estadoExperimento != SIMULACAO)
        return;

    registradorDados.atenderGravacao();
    enviarTelemetriaSimulacao();
}

void tratarComandosTrajetoriaESimulacao()
{
    if (comunicacao.inicioTrajetoriaFoiSolicitado())
    {
        const CabecalhoTrajetoria cabecalho =
            comunicacao.obterCabecalhoTrajetoria();
        comunicacao.limparSolicitacaoInicioTrajetoria();

        if (sistemaEstaOcupado())
        {
            Serial.println("ERROR;SYSTEM_BUSY");
        }
        else if (!registradorDadosPronto)
        {
            Serial.println("ERROR;SD_NOT_READY");
        }
        else if (!gerenciadorTrajetoria.iniciarEnvio(cabecalho))
        {
            Serial.println("ERROR;TRAJ_BEGIN");
        }
        else
        {
            Serial.print("TRAJ_BEGIN_OK;ID=");
            Serial.print(cabecalho.id);
            Serial.print(";N=");
            Serial.println(cabecalho.quantidadePontos);
        }
    }

    if (comunicacao.pontoTrajetoriaEstaPronto())
    {
        const PontoTrajetoria ponto =
            comunicacao.obterPontoTrajetoria();
        comunicacao.limparPontoTrajetoriaPronto();

        if (!gerenciadorTrajetoria.estaRecebendo())
        {
            Serial.println("ERROR;TRAJ_NOT_RECEIVING");
        }
        else if (!gerenciadorTrajetoria.adicionarPonto(ponto))
        {
            gerenciadorTrajetoria.abortarEnvio();
            Serial.print("ERROR;TRAJ_POINT;I=");
            Serial.println(ponto.indice);
        }
        else
        {
            const uint32_t recebidos =
                gerenciadorTrajetoria.obterQuantidadePontosRecebidos();

            if ((recebidos %
                 ConfiguracoesSimulacao::INTERVALO_CONFIRMACAO_TRAJETORIA) == 0 ||
                recebidos == gerenciadorTrajetoria.obterQuantidadePontosEsperados())
            {
                Serial.print("TRAJ_PROGRESS;RECEIVED=");
                Serial.print(recebidos);
                Serial.print(";EXPECTED=");
                Serial.println(
                    gerenciadorTrajetoria.obterQuantidadePontosEsperados()
                );
            }
        }
    }

    if (comunicacao.fimTrajetoriaFoiSolicitado())
    {
        comunicacao.limparSolicitacaoFimTrajetoria();

        if (!gerenciadorTrajetoria.finalizarEnvio())
        {
            gerenciadorTrajetoria.abortarEnvio();
            Serial.println("ERROR;TRAJ_INCOMPLETE");
        }
        else
        {
            const InformacoesTrajetoria &informacoes = gerenciadorTrajetoria.obterInformacoes();

            Serial.print("TRAJECTORY_READY;ID=");
            Serial.print(informacoes.id);
            Serial.print(";N=");
            Serial.print(informacoes.quantidadePontos);
            Serial.print(";MIN_DT=");
            Serial.print(informacoes.menorIntervalo_s, 6);
            Serial.print(";MAX_ACCEL=");
            Serial.println(
                gerenciadorTrajetoria.obterAceleracaoMaximaPermitida(),
                6
            );
        }
    }

    if (comunicacao.abortoTrajetoriaFoiSolicitado())
    {
        comunicacao.limparSolicitacaoAbortoTrajetoria();

        if (!gerenciadorTrajetoria.estaRecebendo())
        {
            Serial.println("ERROR;TRAJ_NOT_RECEIVING");
        }
        else
        {
            gerenciadorTrajetoria.abortarEnvio();
            Serial.println("TRAJ_ABORTED");
        }
    }

    if (comunicacao.estadoTrajetoriaFoiSolicitado())
    {
        comunicacao.limparSolicitacaoEstadoTrajetoria();
        imprimirEstadoTrajetoria();
    }

    if (comunicacao.inicioSimulacaoFoiSolicitado())
    {
        const SolicitacaoInicioSimulacao solicitacao =
            comunicacao.obterSolicitacaoInicioSimulacao();
        comunicacao.limparSolicitacaoInicioSimulacao();
        iniciarSimulacaoTrajetoria(solicitacao.aceleracao);
    }

    if (comunicacao.pausaSimulacaoFoiSolicitada())
    {
        comunicacao.limparSolicitacaoPausaSimulacao();
        pausarSimulacaoTrajetoria();
    }

    if (comunicacao.retomadaSimulacaoFoiSolicitada())
    {
        comunicacao.limparSolicitacaoRetomadaSimulacao();
        retomarSimulacaoTrajetoria();
    }

    if (comunicacao.abortoSimulacaoFoiSolicitado())
    {
        comunicacao.limparSolicitacaoAbortoSimulacao();
        abortarSimulacaoTrajetoria();
    }

    if (comunicacao.estadoSimulacaoFoiSolicitado())
    {
        comunicacao.limparSolicitacaoEstadoSimulacao();
        imprimirEstadoSimulacao();
    }

    if (comunicacao.baixarSimulacaoFoiSolicitado())
    {
        comunicacao.limparSolicitacaoBaixarDadosSimulacao();

        if (sistemaEstaOcupado())
        {
            Serial.println("ERROR;SYSTEM_BUSY");
        }
        else if (!registradorDadosPronto)
        {
            Serial.println("ERROR;SD_NOT_READY");
        }
        else if (!registradorDados.baixarArquivosSimulacao(Serial))
        {
            Serial.println("ERROR;SIM_DOWNLOAD");
        }
    }
}

void imprimirEstadoControle()
{
    Serial.print(
        "CONTROL_STATUS;STATE="
    );

    Serial.print(
        estadoParaTexto(
            estadoExperimento
        )
    );

    for (uint8_t eixo = 0; eixo < 3; eixo++)
    {
        if (!controladorCampo.eixoEstaHabilitado(eixo))
            continue;

        const EstadoControleEixo &estadoEixo =
            controladorCampo.obterEstado(eixo);

        Serial.print(
            ";"
        );

        Serial.print(
            letraEixoCalibracao(eixo)
        );

        Serial.print(
            "_BREF="
        );

        Serial.print(
            estadoEixo.referenciaCampo_uT,
            6
        );

        Serial.print(
            ";"
        );

        Serial.print(
            letraEixoCalibracao(eixo)
        );

        Serial.print(
            "_B="
        );

        Serial.print(
            estadoEixo.campoMedido_uT,
            6
        );

        Serial.print(
            ";"
        );

        Serial.print(
            letraEixoCalibracao(eixo)
        );

        Serial.print(
            "_IREF="
        );

        Serial.print(
            estadoEixo.referenciaCorrente_A,
            6
        );

        Serial.print(
            ";"
        );

        Serial.print(
            letraEixoCalibracao(eixo)
        );

        Serial.print(
            "_I="
        );

        Serial.print(
            estadoEixo.correnteMedida_A,
            6
        );

        Serial.print(
            ";"
        );

        Serial.print(
            letraEixoCalibracao(eixo)
        );

        Serial.print(
            "_PWM="
        );

        Serial.print(
            estadoEixo.comandoPWM
        );

        float minimo_uT = 0.0f;
        float maximo_uT = 0.0f;

        if (controladorCampo.obterLimitesOperacionaisCampo(
                eixo,
                minimo_uT,
                maximo_uT))
        {
            Serial.print(";");
            Serial.print(letraEixoCalibracao(eixo));
            Serial.print("_BMIN=");
            Serial.print(minimo_uT, 6);
            Serial.print(";");
            Serial.print(letraEixoCalibracao(eixo));
            Serial.print("_BMAX=");
            Serial.print(maximo_uT, 6);
        }
    }

    Serial.println();
}

// STATUS

void imprimirEstadoSistema()
{
    Serial.print(
        "STATUS;STATE="
    );

    Serial.print(
        estadoParaTexto(
            estadoExperimento
        )
    );

    Serial.print(
        ";PWM="
    );

    Serial.print(
        pwmAtual
    );

    Serial.print(
        ";CONFIG="
    );

    if (comunicacao.possuiConfiguracao())
        Serial.println("OK");
    else
        Serial.println("NONE");
}

// SETUP

void setup()
{
    // Comunicação

    comunicacao.iniciar();

    // PWM

    analogWriteResolution(12);

    // AcionadorDRV8871

    acionadorX.iniciar();
    acionadorY.iniciar();
    acionadorZ.iniciar();

    pararTodosEixos();

    // I2C - INA226

    Wire2.begin();

    Wire2.setClock(400000);

    // INA226

    inaXPronto =
        sensorCorrenteX.iniciar();

    inaYPronto =
        sensorCorrenteY.iniciar();

    inaZPronto =
        sensorCorrenteZ.iniciar();

    Serial.print(
        "INA X: ");

    if (inaXPronto)
        Serial.println("OK");
    else
        Serial.println("ERROR");

    Serial.print(
        "INA Y: ");

    if (inaYPronto)
        Serial.println("OK");
    else
        Serial.println("ERROR");

    Serial.print(
        "INA Z: ");

    if (inaZPronto)
        Serial.println("OK");
    else
        Serial.println("ERROR");

    // AD7606

    ad7606Pronto =
        conversorADC.iniciar();

    Serial.print(
        "AD7606: ");

    if (ad7606Pronto)
        Serial.println("OK");
    else
        Serial.println("ERROR");

    // CONFIGURACAO FIXA DOS SENSORES
    // Cada INA226 é configurado com o valor de shunt do seu próprio eixo.

    if (inaXPronto)
    {
        ConfiguracaoINA226 configuracaoINAX =
            criarConfiguracaoINA226(
                ConfiguracoesCalibracao::
                    RESISTENCIA_SHUNT_INA_X_OHM);

        if (!sensorCorrenteX.configurar(configuracaoINAX))
            inaXPronto = false;
    }

    if (inaYPronto)
    {
        ConfiguracaoINA226 configuracaoINAY =
            criarConfiguracaoINA226(
                ConfiguracoesCalibracao::
                    RESISTENCIA_SHUNT_INA_Y_OHM);

        if (!sensorCorrenteY.configurar(configuracaoINAY))
            inaYPronto = false;
    }

    if (inaZPronto)
    {
        ConfiguracaoINA226 configuracaoINAZ =
            criarConfiguracaoINA226(
                ConfiguracoesCalibracao::
                    RESISTENCIA_SHUNT_INA_Z_OHM);

        if (!sensorCorrenteZ.configurar(configuracaoINAZ))
            inaZPronto = false;
    }

    if (ad7606Pronto)
    {
        ad7606Pronto = conversorADC.definirSobreamostragem(
            converterSobreamostragemParaAD7606(ConfiguracoesCalibracao::SOBREAMOSTRAGEM_AD7606)
        );
    }

    // REGISTRADOR DE DADOS

    registradorDadosPronto =
        registradorDados.iniciar();

    Serial.print(
        "SD: ");

    if (registradorDadosPronto)
        Serial.println("OK");
    else
        Serial.println("ERROR");

    // PARAMETROS DE CALIBRACAO JA PROCESSADOS

    if (registradorDadosPronto)
    {
        parametrosCalibracaoCarregados =
            registradorDados.carregarParametrosCalibracao(
                parametrosCalibracaoAtivos
            );
    }

    Serial.print("Calibration params: ");
    Serial.println(
        parametrosCalibracaoCarregados ? "LOADED" : "NONE"
    );

    controladorCampo.definirMascaraEixos(
        ConfiguracoesControle::MASCARA_EIXOS
    );

    if (parametrosCalibracaoCarregados)
    {
        controladorCampo.carregarCalibracao(
            parametrosCalibracaoAtivos
        );
    }

    gerenciadorCalibracao.definirHardwarePronto(
        inaXPronto,
        inaYPronto,
        inaZPronto,
        ad7606Pronto,
        registradorDadosPronto
    );

    // Estado inicial

    estadoExperimento =
        OCIOSO;

    pwmAtual =
        0;

    experimentoInicializado =
        false;

    limiteOperacionalAtingido =
        false;

    limiteCriticoAtingido =
        false;

    correnteNoLimite =
        0.0f;

    eixoNoLimite =
        '-';

    Serial.println(
        "SYSTEM_READY"
    );
}

// LOOP

void loop()
{
    // COMUNICAÇÃO

    comunicacao.atualizar();

    // TRAJETORIA / SIMULACAO ORBITAL

    tratarComandosTrajetoriaESimulacao();

    // CONTROLE MANUAL - LIMITES / COMANDO DOWNLOAD (BAIXAR DADOS)

    if (comunicacao.limitesControleForamSolicitados())
    {
        comunicacao.limparSolicitacaoLimitesControle();

        if (!parametrosCalibracaoCarregados)
            Serial.println("ERROR;CALIBRATION_PARAMS_NOT_LOADED");
        else
            imprimirLimitesControle();
    }

    if (comunicacao.baixarControleFoiSolicitado())
    {
        comunicacao.limparSolicitacaoBaixarDadosControle();

        if (sistemaEstaOcupado())
        {
            Serial.println("ERROR;SYSTEM_BUSY");
        }
        else if (!registradorDadosPronto)
        {
            Serial.println("ERROR;SD_NOT_READY");
        }
        else if (!registradorDados.baixarArquivosControleManual(Serial))
        {
            Serial.println("ERROR;CONTROL_DOWNLOAD");
        }
    }

    // CALIBRACAO AUTOMATICA

    if (comunicacao.haNovaConfiguracaoCalibracao())
    {
        comunicacao.limparNovaConfiguracaoCalibracao();

        if (sistemaEstaOcupado())
        {
            Serial.println("ERROR;SYSTEM_BUSY");
        }
        else
        {
            gerenciadorCalibracao.definirHardwarePronto(
                inaXPronto, inaYPronto, inaZPronto,
                ad7606Pronto, registradorDadosPronto
            );

            if (!gerenciadorCalibracao.configurar(
                    comunicacao.obterConfiguracaoCalibracao()))
            {
                Serial.println("ERROR;CAL_CONFIG_REJECTED");
            }
        }
    }

    if (comunicacao.inicioCalibracaoFoiSolicitado())
    {
        comunicacao.limparSolicitacaoInicioCalibracao();

        if (sistemaEstaOcupado())
        {
            Serial.println("ERROR;SYSTEM_BUSY");
        }
        else if (!gerenciadorCalibracao.iniciar())
        {
            Serial.println("ERROR;CAL_START_FAILED");
        }
    }

    if (comunicacao.estadoCalibracaoFoiSolicitado())
    {
        comunicacao.limparSolicitacaoEstadoCalibracao();

        Serial.print("CAL_STATUS;STATE=");
        Serial.print(gerenciadorCalibracao.obterNomeEstado());
        Serial.print(";AXIS=");
        Serial.print(gerenciadorCalibracao.obterEixoAtivo());
        Serial.print(";PWM=");
        Serial.println(gerenciadorCalibracao.obterPWMAtual());
    }

    if (comunicacao.abortoCalibracaoFoiSolicitado())
    {
        comunicacao.limparSolicitacaoAbortoCalibracao();
        gerenciadorCalibracao.abortar("USER_ABORT");
    }

    if (comunicacao.parametrosCalibracaoEstaoProntos())
    {
        ParametrosCalibracao parametrosRecebidos =
            comunicacao.obterParametrosCalibracao();

        comunicacao.limparParametrosCalibracaoProntos();

        if (sistemaEstaOcupado())
        {
            Serial.println("ERROR;SYSTEM_BUSY");
        }
        else
        {
            mesclarParametrosCalibracao(
                parametrosCalibracaoAtivos,
                parametrosRecebidos
            );

            if (registradorDados.salvarParametrosCalibracao(
                    parametrosCalibracaoAtivos))
            {
                parametrosCalibracaoCarregados = true;

                controladorCampo.carregarCalibracao(
                    parametrosCalibracaoAtivos
                );

                Serial.println("CAL_PARAMS_SAVED");
            }
            else
            {
                Serial.println("ERROR;CAL_PARAMS_SAVE");
            }
        }
    }

    if (gerenciadorCalibracao.estaExecutando())
    {
        if (comunicacao.haNovaReferenciaCampo())
        {
            comunicacao.limparNovaReferenciaCampo();
        }

        if (comunicacao.inicioControleFoiSolicitado())
        {
            comunicacao.limparSolicitacaoInicioControle();

            Serial.println(
                "ERROR;SYSTEM_BUSY"
            );
        }

        if (comunicacao.paradaControleFoiSolicitada())
        {
            comunicacao.limparSolicitacaoParadaControle();

            Serial.println(
                "ERROR;CONTROL_NOT_RUNNING"
            );
        }

        if (comunicacao.estadoControleFoiSolicitado())
        {
            comunicacao.limparSolicitacaoEstadoControle();

            imprimirEstadoControle();
        }

        gerenciadorCalibracao.atualizar();
        return;
    }

    // SIMULACAO ORBITAL EM EXECUCAO / PAUSADA

    if (sessaoSimulacaoEstaAtiva())
    {
        // A trajetória é a única fonte de referência durante a simulação.
        if (comunicacao.haNovaReferenciaCampo())
        {
            comunicacao.limparNovaReferenciaCampo();
            Serial.println("ERROR;SIMULATION_ACTIVE");
        }

        if (comunicacao.inicioControleFoiSolicitado())
        {
            comunicacao.limparSolicitacaoInicioControle();
            Serial.println("ERROR;SIMULATION_ACTIVE");
        }

        if (comunicacao.paradaControleFoiSolicitada())
        {
            comunicacao.limparSolicitacaoParadaControle();
            Serial.println("ERROR;SIMULATION_ACTIVE");
        }

        if (comunicacao.estadoControleFoiSolicitado())
        {
            comunicacao.limparSolicitacaoEstadoControle();
            imprimirEstadoControle();
        }

        if (comunicacao.limpezaFoiSolicitada())
        {
            comunicacao.limparSolicitacaoLimpeza();
            Serial.println("ERROR;SYSTEM_BUSY");
        }

        if (comunicacao.baixarDadosFoiSolicitado())
        {
            comunicacao.limparSolicitacaoBaixarDados();
            Serial.println("ERROR;SYSTEM_BUSY");
        }

        if (comunicacao.inicioFoiSolicitado())
        {
            comunicacao.limparSolicitacaoInicio();
            Serial.println("ERROR;SYSTEM_BUSY");
        }

        if (comunicacao.haNovaConfiguracao())
        {
            comunicacao.limparNovaConfiguracao();
            Serial.println("ERROR;SYSTEM_BUSY");
        }

        if (comunicacao.estadoFoiSolicitado())
        {
            comunicacao.limparSolicitacaoEstado();
            imprimirEstadoSistema();
        }

        executarSimulacaoTrajetoria();
        return;
    }

    // NOVA REFERENCIA DE CAMPO

    if (comunicacao.haNovaReferenciaCampo())
    {
        ReferenciaCampo referencia =
            comunicacao.obterReferenciaCampo();

        comunicacao.limparNovaReferenciaCampo();

        if (!parametrosCalibracaoCarregados)
        {
            Serial.println("ERROR;CALIBRATION_PARAMS_NOT_LOADED");
        }
        else if (!referenciaCampoDentroDosLimites(referencia, true))
        {
            Serial.println("FIELD_REF_REJECTED");
        }
        else
        {
            controladorCampo.definirReferenciaCampo(referencia);

            if (estadoExperimento == CONTROLE)
                registrarEventoReferenciaManual(referencia);

            Serial.println("FIELD_REF_OK");
        }
    }

    // COMANDO CONTROL_PARADA (PARAR CONTROLE)

    if (comunicacao.paradaControleFoiSolicitada())
    {
        comunicacao.limparSolicitacaoParadaControle();

        if (estadoExperimento == CONTROLE)
        {
            pararControleCampo();
        }
        else
        {
            Serial.println(
                "ERROR;CONTROL_NOT_RUNNING"
            );
        }
    }

    // COMANDO CONTROL_COMANDO START (INICIAR) (INICIAR CONTROLE)

    if (comunicacao.inicioControleFoiSolicitado())
    {
        comunicacao.limparSolicitacaoInicioControle();

        iniciarControleCampo();
    }

    // COMANDO CONTROL_STATUS (ESTADO DO CONTROLE)

    if (comunicacao.estadoControleFoiSolicitado())
    {
        comunicacao.limparSolicitacaoEstadoControle();

        imprimirEstadoControle();
    }

    // CONTROLE EM EXECUCAO

    if (estadoExperimento == CONTROLE)
    {
        // Comandos de experimento nao sao executados com controle ativo.

        if (comunicacao.limpezaFoiSolicitada())
        {
            comunicacao.limparSolicitacaoLimpeza();

            Serial.println(
                "ERROR;SYSTEM_BUSY"
            );
        }

        if (comunicacao.baixarDadosFoiSolicitado())
        {
            comunicacao.limparSolicitacaoBaixarDados();

            Serial.println(
                "ERROR;SYSTEM_BUSY"
            );
        }

        if (comunicacao.inicioFoiSolicitado())
        {
            comunicacao.limparSolicitacaoInicio();

            Serial.println(
                "ERROR;SYSTEM_BUSY"
            );
        }

        if (comunicacao.haNovaConfiguracao())
        {
            comunicacao.limparNovaConfiguracao();

            Serial.println(
                "ERROR;SYSTEM_BUSY"
            );
        }

        if (comunicacao.estadoFoiSolicitado())
        {
            comunicacao.limparSolicitacaoEstado();

            imprimirEstadoSistema();
        }

        executarControleCampo();

        if (estadoExperimento == CONTROLE)
        {
            registradorDados.atenderGravacao();
            enviarTelemetriaControle();
        }

        return;
    }

    // CLEAR

    if (comunicacao.limpezaFoiSolicitada())
    {
        // Não permite CLEAR durante experimento

        if (sistemaEstaOcupado())
        {
            Serial.println(
                "ERROR;SYSTEM_BUSY"
            );
        }
        else
        {
            pararTodosEixos();

            // Remove dados do experimento anterior

            if (!registradorDados.limparDados())
            {
                Serial.println(
                    "ERROR;SD_CLEAR"
                );
            }
            else
            {
                pwmAtual =
                    0;

                experimentoInicializado =
                    false;

                limiteOperacionalAtingido =
                    false;

                limiteCriticoAtingido =
                    false;

                correnteNoLimite =
                    0.0f;

                eixoNoLimite =
                    '-';

                estadoExperimento =
                    OCIOSO;

                Serial.println(
                    "CLEARED"
                );
            }
        }

        comunicacao.limparSolicitacaoLimpeza();
    }

    // NOVA CONFIGURAÇÃO

    if (comunicacao.haNovaConfiguracao())
    {
        comunicacao.limparNovaConfiguracao();

        if (!sistemaEstaOcupado())
        {
            // INA226

            bool inaOK =
                configurarINA226DoExperimento();

            // AD7606

            bool adcOk =
                false;

            if (ad7606Pronto)
            {
                adcOk =
                    configurarAD7606DoExperimento();
            }

            // SD

            bool sdFuncionando =
                registradorDadosPronto;

            // Resultado

            if (
                inaOK &&
                adcOk &&
                sdFuncionando
            )
            {
                estadoExperimento =
                    PRONTO;
            }
            else
            {
                estadoExperimento =
                    OCIOSO;

                if (!inaOK)
                {
                    Serial.println(
                        "ERROR;INA226_CONFIG"
                    );
                }

                if (!adcOk)
                {
                    Serial.println(
                        "ERROR;AD7606_CONFIG"
                    );
                }

                if (!sdFuncionando)
                {
                    Serial.println(
                        "ERROR;SD_NOT_READY"
                    );
                }
            }
        }
        else
        {
            Serial.println(
                "ERROR;EXPERIMENT_RUNNING"
            );
        }
    }

    // STATUS

    if (comunicacao.estadoFoiSolicitado())
    {
        imprimirEstadoSistema();

        comunicacao.limparSolicitacaoEstado();
    }

    // COMANDO DOWNLOAD (BAIXAR DADOS)

    if (comunicacao.baixarDadosFoiSolicitado())
    {
        comunicacao.limparSolicitacaoBaixarDados();

        // Não permite COMANDO DOWNLOAD (BAIXAR DADOS) durante experimento

        if (sistemaEstaOcupado())
        {
            Serial.println(
                "ERROR;SYSTEM_BUSY"
            );
        }

        // Verifica SD

        else if (!registradorDadosPronto)
        {
            Serial.println(
                "ERROR;SD_NOT_READY"
            );
        }

        // Faz COMANDO DOWNLOAD (BAIXAR DADOS)

        else
        {
            registradorDados.baixarArquivos(
                Serial
            );
        }
    }

    // COMANDO START (INICIAR)

    if (comunicacao.inicioFoiSolicitado())
    {
        comunicacao.limparSolicitacaoInicio();

        // Só pode iniciar em PRONTO

        if (sistemaEstaOcupado())
        {
            Serial.println("ERROR;SYSTEM_BUSY");
        }
        else if (estadoExperimento != PRONTO)
        {
            Serial.println(
                "ERROR;NOT_READY"
            );
        }
        else
        {
            ConfiguracaoExperimento configuracao =
                comunicacao.obterConfiguracao();

            // Verifica SD

            if (!registradorDadosPronto)
            {
                Serial.println(
                    "ERROR;SD_NOT_READY"
                );

                return;
            }

            // Inicia RegistradorDados

            if (!registradorDados.iniciarExperimento(
                    configuracao
                ))
            {
                Serial.println(
                    "ERROR;SD_START"
                );

                return;
            }

            // Reset das proteções

            limiteOperacionalAtingido =
                false;

            limiteCriticoAtingido =
                false;

            correnteNoLimite =
                0.0f;

            eixoNoLimite =
                '-';

            // Inicializa experimento

            pwmAtual =
                configuracao.pwmInicial;

            estadoExperimento =
                EXECUTANDO;

            experimentoInicializado =
                true;

            inicioPassoPWM =
                millis();

            // Protocolo

            Serial.println(
                "STARTED"
            );

            // Primeiro PWM

            aplicarPWM(
                pwmAtual
            );
        }
    }

    // EXECUÇÃO DO EXPERIMENTO

    if (
        estadoExperimento == EXECUTANDO &&
        experimentoInicializado
    )
    {
        ConfiguracaoExperimento configuracao =
            comunicacao.obterConfiguracao();

        // INA226 - X

        if (
            inaXPronto &&
            sensorCorrenteX.conversaoEstaPronta()
        )
        {
            unsigned long instante =
                micros();

            MedicaoINA226 medicao =
                sensorCorrenteX.lerMedicao();

            // Registro para SD

            RegistroINA226 registro;

            registro.instante_us =
                instante;

            registro.pwm =
                pwmAtual;

            registro.eixo =
                'X';

            registro.reservado =
                0;

            registro.corrente_A =
                medicao.corrente;

            registro.tensaoBarramento_V =
                medicao.tensaoBarramento;

            registro.tensaoShunt_V =
                medicao.tensaoShunt;

            registro.potencia_W =
                medicao.potencia;

            // Grava no SD

            if (!registradorDados.registrarINA226(
                    registro
                ))
            {
                Serial.println(
                    "ERROR;SD_INA226_WRITE"
                );
            }

            // Proteção

            verificarProtecaoCorrente(
                'X',
                medicao.corrente
            );

#if DEPURACAO_AMOSTRAS_SERIAL

            Serial.print(
                "INA226;axis=X;PWM="
            );

            Serial.print(
                pwmAtual
            );

            Serial.print(
                ";t="
            );

            Serial.print(
                instante
            );

            Serial.print(
                ";I="
            );

            Serial.print(
                medicao.corrente,
                6
            );

            Serial.print(
                ";V="
            );

            Serial.print(
                medicao.tensaoBarramento,
                6
            );

            Serial.print(
                ";Vshunt="
            );

            Serial.print(
                medicao.tensaoShunt,
                6
            );

            Serial.print(
                ";P="
            );

            Serial.println(
                medicao.potencia,
                6
            );

#endif
        }

        // INA226 - Y

        if (
            estadoExperimento == EXECUTANDO &&
            inaYPronto &&
            sensorCorrenteY.conversaoEstaPronta()
        )
        {
            unsigned long instante =
                micros();

            MedicaoINA226 medicao =
                sensorCorrenteY.lerMedicao();

            RegistroINA226 registro;

            registro.instante_us =
                instante;

            registro.pwm =
                pwmAtual;

            registro.eixo =
                'Y';

            registro.reservado =
                0;

            registro.corrente_A =
                medicao.corrente;

            registro.tensaoBarramento_V =
                medicao.tensaoBarramento;

            registro.tensaoShunt_V =
                medicao.tensaoShunt;

            registro.potencia_W =
                medicao.potencia;

            if (!registradorDados.registrarINA226(
                    registro
                ))
            {
                Serial.println(
                    "ERROR;SD_INA226_WRITE"
                );
            }

            verificarProtecaoCorrente(
                'Y',
                medicao.corrente
            );

#if DEPURACAO_AMOSTRAS_SERIAL

            Serial.print(
                "INA226;axis=Y;PWM="
            );

            Serial.print(
                pwmAtual
            );

            Serial.print(
                ";t="
            );

            Serial.print(
                instante
            );

            Serial.print(
                ";I="
            );

            Serial.print(
                medicao.corrente,
                6
            );

            Serial.print(
                ";V="
            );

            Serial.print(
                medicao.tensaoBarramento,
                6
            );

            Serial.print(
                ";Vshunt="
            );

            Serial.print(
                medicao.tensaoShunt,
                6
            );

            Serial.print(
                ";P="
            );

            Serial.println(
                medicao.potencia,
                6
            );

#endif
        }

        // INA226 - Z

        if (
            estadoExperimento == EXECUTANDO &&
            inaZPronto &&
            sensorCorrenteZ.conversaoEstaPronta()
        )
        {
            unsigned long instante =
                micros();

            MedicaoINA226 medicao =
                sensorCorrenteZ.lerMedicao();

            RegistroINA226 registro;

            registro.instante_us =
                instante;

            registro.pwm =
                pwmAtual;

            registro.eixo =
                'Z';

            registro.reservado =
                0;

            registro.corrente_A =
                medicao.corrente;

            registro.tensaoBarramento_V =
                medicao.tensaoBarramento;

            registro.tensaoShunt_V =
                medicao.tensaoShunt;

            registro.potencia_W =
                medicao.potencia;

            if (!registradorDados.registrarINA226(
                    registro
                ))
            {
                Serial.println(
                    "ERROR;SD_INA226_WRITE"
                );
            }

            verificarProtecaoCorrente(
                'Z',
                medicao.corrente
            );

#if DEPURACAO_AMOSTRAS_SERIAL

            Serial.print(
                "INA226;axis=Z;PWM="
            );

            Serial.print(
                pwmAtual
            );

            Serial.print(
                ";t="
            );

            Serial.print(
                instante
            );

            Serial.print(
                ";I="
            );

            Serial.print(
                medicao.corrente,
                6
            );

            Serial.print(
                ";V="
            );

            Serial.print(
                medicao.tensaoBarramento,
                6
            );

            Serial.print(
                ";Vshunt="
            );

            Serial.print(
                medicao.tensaoShunt,
                6
            );

            Serial.print(
                ";P="
            );

            Serial.println(
                medicao.potencia,
                6
            );

#endif
        }

        // LIMITE CRITICO
    // E uma trava de seguranca: ao atingir este valor, todos os eixos sao desligados e o sistema entra em FALHA.

        if (
            estadoExperimento == FALHA
        )
        {
            return;
        }

        // AD7606
        // CH1 -> X
        // CH2 -> Y
        // CH3 -> Z

        if (
            ad7606Pronto &&
            estadoExperimento == EXECUTANDO
        )
        {
            AmostraAD7606 amostra;

            if (conversorADC.lerAmostra(amostra))
            {
                // Registro para SD

                RegistroAD7606 registro;

                registro.instante_us =
                    amostra.instante;

                registro.pwm =
                    pwmAtual;

                for (int i = 0; i < 3; i++)
                {
                    registro.valorBruto[i] =
                        amostra.valorBruto[i];

                    registro.tensao[i] =
                        amostra.tensao[i];
                }

                // Grava no SD

                if (!registradorDados.registrarAD7606(
                        registro
                    ))
                {
                    Serial.println(
                        "ERROR;SD_AD7606_WRITE"
                    );
                }

#if DEPURACAO_AMOSTRAS_SERIAL

                Serial.print(
                    "AD7606;t="
                );

                Serial.print(
                    amostra.instante
                );

                Serial.print(
                    ";CH1_RAW="
                );

                Serial.print(
                    amostra.valorBruto[0]
                );

                Serial.print(
                    ";CH1_V="
                );

                Serial.print(
                    amostra.tensao[0],
                    6
                );

                Serial.print(
                    ";CH2_RAW="
                );

                Serial.print(
                    amostra.valorBruto[1]
                );

                Serial.print(
                    ";CH2_V="
                );

                Serial.print(
                    amostra.tensao[1],
                    6
                );

                Serial.print(
                    ";CH3_RAW="
                );

                Serial.print(
                    amostra.valorBruto[2]
                );

                Serial.print(
                    ";CH3_V="
                );

                Serial.println(
                    amostra.tensao[2],
                    6
                );

#endif
            }
            else
            {
                // Falha crítica de leitura

                Serial.println(
                    "ERROR;AD7606_READ"
                );

                pararTodosEixos();

                registradorDados.registrarEvento(
                    "AD7606_READ_FAULT"
                );

                registradorDados.abortarExperimento();

                pwmAtual =
                    0;

                experimentoInicializado =
                    false;

                estadoExperimento =
                    FALHA;

                Serial.println(
                    "FAULT"
                );

                return;
            }
        }

        // GRAVACAO PENDENTE NO SD

        registradorDados.atenderGravacao();

        // CONTROLE DO DWELL / PRÓXIMO PWM

        if (
            millis() - inicioPassoPWM >=
            configuracao.tempoPermanencia_ms
        )
        {
            // LIMITE CRITICO

            if (limiteCriticoAtingido)
            {
                pwmAtual =
                    0;

                experimentoInicializado =
                    false;

                estadoExperimento =
                    FALHA;

                return;
            }

            // LIMITE OPERACIONAL
            // O PWM permanece no valor atual até terminar
            // o dwell.

            if (limiteOperacionalAtingido)
            {
                Serial.println(
                    "CURRENT_SOFT_LIMIT_COMPLETE"
                );

                pararTodosEixos();

                pwmAtual =
                    0;

                experimentoInicializado =
                    false;

                // Finaliza gravação

                if (!registradorDados.finalizarExperimento())
                {
                    Serial.println(
                        "ERROR;SD_FINISH"
                    );

                    estadoExperimento =
                        FALHA;

                    return;
                }

                estadoExperimento =
                    CONCLUIDO;

                Serial.println(
                    "COMPLETE"
                );

                return;
            }

            // Verifica fim normal do sweep

            bool chegouAoFinal =
                false;

            if (configuracao.passoPWM > 0)
            {
                if (
                    pwmAtual >=
                    configuracao.pwmFinal
                )
                {
                    chegouAoFinal =
                        true;
                }
            }
            else
            {
                if (
                    pwmAtual <=
                    configuracao.pwmFinal
                )
                {
                    chegouAoFinal =
                        true;
                }
            }

            // Fim normal

            if (chegouAoFinal)
            {
                pararTodosEixos();

                pwmAtual =
                    0;

                experimentoInicializado =
                    false;

                // Finaliza RegistradorDados

                if (!registradorDados.finalizarExperimento())
                {
                    Serial.println(
                        "ERROR;SD_FINISH"
                    );

                    estadoExperimento =
                        FALHA;

                    return;
                }

                estadoExperimento =
                    CONCLUIDO;

                Serial.println(
                    "COMPLETE"
                );
            }

            // Próximo nível de PWM

            else
            {
                pwmAtual =
                    calcularProximoPWM(
                        pwmAtual,
                        configuracao
                    );

                aplicarPWM(
                    pwmAtual
                );

                inicioPassoPWM =
                    millis();
            }
        }
    }
}
