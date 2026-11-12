// Interface simples para leitura dos monitores de corrente INA226.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#ifndef SENSOR_INA226_H
#define SENSOR_INA226_H

#include <Arduino.h>
#include <Wire.h>
#include <INA226.h>

// CONFIGURAÇÃO DO INA226

struct ConfiguracaoINA226
{
    float correnteMaxima;
    float resistenciaShunt;

    uint8_t media;

    uint8_t tempoConversaoShunt;
    uint8_t tempoConversaoBarramento;
};

// RESULTADO DE UMA MEDIÇÃO

struct MedicaoINA226
{
    float corrente;
    float tensaoBarramento;
    float tensaoShunt;
    float potencia;
};

// CLASSE

class SensorINA226
{
private:

    INA226 sensor;

    uint8_t enderecoI2C;
    TwoWire *barramentoI2C;

    ConfiguracaoINA226 configuracao;

public:

    // CONSTRUTOR

    SensorINA226(
        uint8_t endereco,
        TwoWire *barramento = &Wire
    );

    // INICIALIZAÇÃO

    bool iniciar();

    // CONFIGURAÇÃO

    bool configurar(
        const ConfiguracaoINA226 &novaConfiguracao
    );

    // CONFIGURAÇÕES INDIVIDUAIS

    void definirMedia(uint8_t media);

    void definirTempoConversaoShunt(uint8_t tempo);

    void definirTempoConversaoBarramento(uint8_t tempo);

    // STATUS DA CONVERSÃO

    bool conversaoEstaPronta();

    // Verifica se o dispositivo ainda responde no barramento I2C.
    bool estaConectado();

    // LEITURAS INDIVIDUAIS

    float lerCorrente();

    float lerTensaoBarramento();

    float lerTensaoShunt();

    float lerPotencia();

    // LEITURA COMPLETA

    MedicaoINA226 lerMedicao();
};

#endif
