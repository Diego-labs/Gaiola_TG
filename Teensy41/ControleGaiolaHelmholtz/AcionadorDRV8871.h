// Interface de acionamento bidirecional das bobinas pelo DRV8871.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#ifndef ACIONADOR_DRV8871_H
#define ACIONADOR_DRV8871_H

#include <Arduino.h>

class AcionadorDRV8871
{
private:

    // Pinos de controle do AcionadorDRV8871

    uint8_t pinoIN1;
    uint8_t pinoIN2;

    // Configuração do PWM

    uint32_t frequenciaPWM;
    uint8_t resolucaoPWM;

    // Valor máximo do PWM.
    // Para 12 bits:
    // 0 ... 4095

    int pwmMaximo;
    int ultimoPWM; // Evita reconfigurar as entradas a cada passo de controle

public:

    // CONSTRUTOR

    // Cria um AcionadorDRV8871 informando os pinos IN1 e IN2.

    AcionadorDRV8871(uint8_t pinoIn1, uint8_t pinoIn2);

    // INICIALIZAÇÃO

    void iniciar();

    // CONTROLE DO AcionadorDRV8871

    // value:
    // +4095 ... +1  -> sentido positivo
    //       0       -> stop / coast
    // -1 ... -4095  -> sentido negativo
    // Utiliza PWM com drive + brake (slow decay).

    void definirPWM(int valor);

    // PARADA

    // IN1 = LOW
    // IN2 = LOW
    // Estado: Coast / High-Z

    void parar();

    // BRAKE

    // IN1 = HIGH
    // IN2 = HIGH
    // Estado: Brake / low-side decay

    void frear();
};

#endif
