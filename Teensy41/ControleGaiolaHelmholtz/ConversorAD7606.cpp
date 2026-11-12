// Implementacao da conversao simultanea e leitura dos canais do AD7606.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#include "ConversorAD7606.h"

// CONSTRUTOR

ConversorAD7606::ConversorAD7606(
    uint8_t pinoCs,
    uint8_t pinoBusy,
    uint8_t pinoReset,
    uint8_t pinoConvst,
    uint8_t pinoOs0,
    uint8_t pinoOs1,
    uint8_t pinoOs2
)
{
    // Pinos

    pinoCS = pinoCs;
    pinoBUSY = pinoBusy;
    pinoRESET = pinoReset;
    pinoCONVST = pinoConvst;

    pinoOS0 = pinoOs0;
    pinoOS1 = pinoOs1;
    pinoOS2 = pinoOs2;

    // SPI

    frequenciaSPI = 1000000;    // 1 MHz

    // Faixa do ADC
    // Configuração física:
    // ±10 V

    faixaADC = 10.0f;

    // Tempo limite BUSY
    // 10000 us = 10 ms

    tempoLimiteBusy = 10000;
}

// INICIALIZAÇÃO

bool ConversorAD7606::iniciar()
{
    // Configuração dos pinos

    pinMode(pinoCS, OUTPUT);

    pinMode(pinoCONVST, OUTPUT);

    pinMode(pinoRESET, OUTPUT);

    pinMode(pinoBUSY, INPUT);

    pinMode(pinoOS0, OUTPUT);

    pinMode(pinoOS1, OUTPUT);

    pinMode(pinoOS2, OUTPUT);

    // Estados iniciais

    digitalWrite(pinoCS, HIGH);

    digitalWrite(pinoCONVST, LOW);

    // Oversampling inicial
    // 0 = 1x / sem oversampling

    definirSobreamostragem(0);

    // Inicializa periférico SPI

    SPI.begin();

    // Reset

    reiniciar();

    // Pequena espera após reset

    delay(100);

    return true;
}

// RESET

void ConversorAD7606::reiniciar()
{
    digitalWrite(pinoRESET, HIGH);

    delayMicroseconds(10);

    digitalWrite(pinoRESET, LOW);

    delayMicroseconds(10);
}

// INICIA CONVERSÃO
// O CONVST recebe um pulso HIGH.
// A subida inicia a conversão.

void ConversorAD7606::iniciarConversao()
{
    digitalWrite(pinoCONVST, HIGH);

    delayMicroseconds(1);

    digitalWrite(pinoCONVST, LOW);
}

// ESPERA BUSY SUBIR
// Depois do CONVST, esperamos:
// BUSY: LOW → HIGH
// Isso indica que a conversão começou.

bool ConversorAD7606::aguardarBusyAlto()
{
    uint32_t inicio = micros();

    while (digitalRead(pinoBUSY) == LOW)
    {
        if (micros() - inicio > tempoLimiteBusy)
        {
            return false;
        }
    }

    return true;
}

// ESPERA BUSY DESCER
// Depois que a conversão termina:
// BUSY: HIGH → LOW
// Agora os dados podem ser lidos.

bool ConversorAD7606::aguardarBusyBaixo()
{
    uint32_t inicio = micros();

    while (digitalRead(pinoBUSY) == HIGH)
    {
        if (micros() - inicio > tempoLimiteBusy)
        {
            return false;
        }
    }

    return true;
}

// LEITURA DE UM CANAL
// O AD7606 envia uma palavra de 16 bits por canal.
// MSB primeiro.

int16_t ConversorAD7606::lerCanal()
{
    uint16_t valor =
        SPI.transfer16(0x0000);

    return (int16_t)valor;
}

// CONFIGURAÇÃO DO SOBREAMOSTRAGEM
// os:
// 0 → 1×
// 1 → 2×
// 2 → 4×
// 3 → 8×
// 4 → 16×
// 5 → 32×
// 6 → 64×

bool ConversorAD7606::definirSobreamostragem(uint8_t sobreamostragem)
{
    // Valores válidos:
    // 0 até 6

    if (sobreamostragem > 6)
    {
        return false;
    }

    // OS0 = bit 0
    // OS1 = bit 1
    // OS2 = bit 2

    digitalWrite(
        pinoOS0,
        sobreamostragem & 0x01
    );

    digitalWrite(
        pinoOS1,
        (sobreamostragem >> 1) & 0x01
    );

    digitalWrite(
        pinoOS2,
        (sobreamostragem >> 2) & 0x01
    );

    return true;
}

// AQUISIÇÃO

bool ConversorAD7606::lerAmostra(
    AmostraAD7606 &amostra
)
{
    // 1. Inicia conversão

    iniciarConversao();

    // 2. Espera BUSY subir
    // Confirma que a conversão começou.

    if (!aguardarBusyAlto())
    {
        return false;
    }

    // 3. Espera BUSY descer
    // Conversão terminou.

    if (!aguardarBusyBaixo())
    {
        return false;
    }

    // 4. Inicia transação SPI

    SPI.beginTransaction(
        SPISettings(
            frequenciaSPI,
            MSBFIRST,
            SPI_MODE3
        )
    );

    // 5. Seleciona o AD7606

    digitalWrite(
        pinoCS,
        LOW
    );

    delayMicroseconds(1);

    // 6. Lê os 3 canais

    amostra.valorBruto[0] =
        lerCanal();

    amostra.valorBruto[1] =
        lerCanal();

    amostra.valorBruto[2] =
        lerCanal();

    // 7. Desseleciona o AD7606

    digitalWrite(
        pinoCS,
        HIGH
    );

    // 8. Finaliza SPI

    SPI.endTransaction();

    // 9. Converte os valores

    amostra.tensao[0] =
        converterBrutoParaTensao(amostra.valorBruto[0]);

    amostra.tensao[1] =
        converterBrutoParaTensao(amostra.valorBruto[1]);

    amostra.tensao[2] =
        converterBrutoParaTensao(amostra.valorBruto[2]);

    // 10. Timestamp

    amostra.instante = micros();

    return true;
}

// ADC → VOLTS
// ±10 V
// ADC = -32768 → aproximadamente -10 V
// ADC =      0 → 0 V
// ADC = +32767 → aproximadamente +10 V

float ConversorAD7606::converterBrutoParaTensao(
    int16_t valorBruto
) const
{
    return (
        (float)valorBruto *
        faixaADC
    ) / 32768.0f;
}
