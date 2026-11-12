// Implementacao da configuracao e leitura dos INA226.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#include "SensorINA226.h"
#include <math.h>

// CONSTRUTOR

SensorINA226::SensorINA226(
    uint8_t endereco,
    TwoWire *barramento
)
    : sensor(endereco, barramento)
{
    enderecoI2C = endereco;
    barramentoI2C = barramento;

    configuracao.correnteMaxima = 1.0f;
    configuracao.resistenciaShunt = 0.01f;

    configuracao.media = INA226_16_SAMPLES;

    configuracao.tempoConversaoShunt = INA226_332_us;
    configuracao.tempoConversaoBarramento = INA226_332_us;
}

// INICIALIZAÇÃO

bool SensorINA226::iniciar()
{
    return sensor.begin();
}

// CONFIGURAÇÃO

bool SensorINA226::configurar(
    const ConfiguracaoINA226 &novaConfiguracao
)
{
    // Guarda a nova configuração

    configuracao = novaConfiguracao;

    // Configuração de calibração

    int erro = sensor.setMaxCurrentShunt(
        configuracao.correnteMaxima,
        configuracao.resistenciaShunt
    );

    if (erro != 0)
    {
        return false;
    }

    // Averaging

    sensor.setAverage(
        configuracao.media
    );

    // Tempo de conversão do shunt

    sensor.setShuntVoltageConversionTime(
        configuracao.tempoConversaoShunt
    );

    // Tempo de conversão do barramento

    sensor.setBusVoltageConversionTime(
        configuracao.tempoConversaoBarramento
    );

    return true;
}

// MEDIA DE AMOSTRAS

void SensorINA226::definirMedia(
    uint8_t media
)
{
    configuracao.media = media;

    sensor.setAverage(media);
}

// CONVERSAO DO SHUNT TIME

void SensorINA226::definirTempoConversaoShunt(
    uint8_t tempo
)
{
    configuracao.tempoConversaoShunt = tempo;

    sensor.setShuntVoltageConversionTime(tempo);
}

// CONVERSAO DO BARRAMENTO TIME

void SensorINA226::definirTempoConversaoBarramento(
    uint8_t tempo
)
{
    configuracao.tempoConversaoBarramento = tempo;

    sensor.setBusVoltageConversionTime(tempo);
}

// LEITURA DE CORRENTE

float SensorINA226::lerCorrente()
{
    return sensor.getCurrent_mA() / 1000.0f;
}

// LEITURA DA TENSÃO DE BARRAMENTO

float SensorINA226::lerTensaoBarramento()
{
    float tensao = sensor.getBusVoltage();

    // A INA226 possui limite de barramento de aproximadamente
    // 36 V. Portanto, valores acima disso são inválidos para
    // a grandeza de tensão de barramento.
    // NÃO fazemos divisão por 1000 automaticamente aqui,
    // pois a biblioteca 0.6.6 já retorna volts.

    if (!isfinite(tensao))
    {
        return NAN;
    }

    return tensao;
}

// LEITURA DA TENSÃO NO SHUNT

float SensorINA226::lerTensaoShunt()
{
    return sensor.getShuntVoltage();
}

// LEITURA DE POTÊNCIA

float SensorINA226::lerPotencia()
{
    return sensor.getPower_mW() / 1000.0f;
}

// LEITURA COMPLETA

MedicaoINA226 SensorINA226::lerMedicao()
{
    MedicaoINA226 medicao;

    medicao.corrente =
        lerCorrente();

    medicao.tensaoBarramento =
        lerTensaoBarramento();

    medicao.tensaoShunt =
        lerTensaoShunt();

    medicao.potencia =
        lerPotencia();

    return medicao;
}

// CONVERSÃO PRONTA

bool SensorINA226::conversaoEstaPronta()
{
    return sensor.isConversionReady();
}

// VERIFICACAO DA CONEXAO I2C

bool SensorINA226::estaConectado()
{
    if (barramentoI2C == nullptr)
        return false;

    barramentoI2C->beginTransmission(enderecoI2C);
    const uint8_t erro = barramentoI2C->endTransmission();

    return erro == 0;
}
