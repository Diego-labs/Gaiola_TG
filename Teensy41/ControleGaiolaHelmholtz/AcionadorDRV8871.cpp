// Implementacao do PWM bidirecional e modos seguros do DRV8871.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#include "AcionadorDRV8871.h"

// CONSTRUTOR

AcionadorDRV8871::AcionadorDRV8871(
    uint8_t pinoIn1,
    uint8_t pinoIn2
)
{
    // Guarda os pinos do AcionadorDRV8871

    pinoIN1 = pinoIn1;
    pinoIN2 = pinoIn2;

    // Configuração do PWM

    frequenciaPWM = 30000;   // 30 kHz
    resolucaoPWM = 12;     // 12 bits

    // Valor máximo do PWM
    // Para N bits:
    // máximo = 2^N - 1
    // Para 12 bits:
    // 2^12 - 1 = 4095

    pwmMaximo =
        (1 << resolucaoPWM) - 1;

    ultimoPWM = 0;
}

// INICIALIZAÇÃO

void AcionadorDRV8871::iniciar()
{
    // Configura os pinos inicialmente como GPIO

    pinMode(
        pinoIN1,
        OUTPUT
    );

    pinMode(
        pinoIN2,
        OUTPUT
    );

    // Configura frequência do PWM
    // Isso prepara o periférico PWM.
    // O pino só será colocado efetivamente em PWM quando
    // analogWrite() for chamado.

    analogWriteFrequency(
        pinoIN1,
        frequenciaPWM
    );

    analogWriteFrequency(
        pinoIN2,
        frequenciaPWM
    );

    // Estado inicial seguro
    // IN1 = LOW
    // IN2 = LOW
    // -> Coast / High-Z

    parar();
}

// SET PWM
// Controle:
// +PWM:
//     IN1 = HIGH
//     IN2 = PWM
// -PWM:
//     IN1 = PWM
//     IN2 = HIGH
// A modulação é complementar:
// pwmBrake = pwmMax - magnitude
// Portanto:
// magnitude = 4095
//     -> pwmBrake = 0
//     -> drive contínuo
// magnitude = 0
//     -> pwmBrake = 4095
//     -> brake contínuo

void AcionadorDRV8871::definirPWM(
    int valor
)
{
    // Limita o comando

    valor =
        constrain(
            valor,
            -pwmMaximo,
            pwmMaximo
        );

    // Nao introduzir transitorios ao reaplicar o mesmo PWM a 1 kHz.
    if (valor == ultimoPWM)
        return;

    // Se o sentido nao mudou, so atualiza o canal PWM ativo.
    // Nao coloca a ponte em HIGH-Z entre atualizacoes.
    if (valor != 0 &&
        ((valor > 0 && ultimoPWM > 0) ||
         (valor < 0 && ultimoPWM < 0)))
    {
        const int pwmFreio = pwmMaximo - abs(valor);
        analogWrite(valor > 0 ? pinoIN2 : pinoIN1, pwmFreio);
        ultimoPWM = valor;
        return;
    }

    // ZERO

    if (valor == 0)
    {
        parar();

        return;
    }

    // DESABILITA TEMPORARIAMENTE OS DOIS PINOS
    // Esta é a parte importante da correção.
    // analogWrite() coloca o pino na função PWM do
    // microcontrolador.
    // Antes de usar digitalWrite() no pino que será mantido
    // em HIGH, retornamos explicitamente o pino para GPIO
    // usando pinMode().
    // Os dois pinos são primeiro colocados em LOW para evitar
    // uma transição direta entre sentidos.

    pinMode(
        pinoIN1,
        OUTPUT
    );

    pinMode(
        pinoIN2,
        OUTPUT
    );

    digitalWrite(
        pinoIN1,
        LOW
    );

    digitalWrite(
        pinoIN2,
        LOW
    );

    // SENTIDO POSITIVO

    if (valor > 0)
    {
        // IN1 será um nível lógico fixo.
        // Portanto ele precisa permanecer como GPIO.

        pinMode(
            pinoIN1,
            OUTPUT
        );

        digitalWrite(
            pinoIN1,
            HIGH
        );

        // IN2 será o pino PWM.

        int magnitude =
            valor;

        int pwmFreio =
            pwmMaximo - magnitude;

        analogWrite(
            pinoIN2,
            pwmFreio
        );
    }

    // SENTIDO NEGATIVO

    else
    {
        // Converte para magnitude positiva

        int magnitude =
            -valor;

        // IN2 será um nível lógico fixo.
        // IMPORTANTE:
        // Retornamos explicitamente o pino para GPIO antes
        // de usar digitalWrite().

        pinMode(
            pinoIN2,
            OUTPUT
        );

        digitalWrite(
            pinoIN2,
            HIGH
        );

        // IN1 será o pino PWM.

        int pwmFreio =
            pwmMaximo - magnitude;

        analogWrite(
            pinoIN1,
            pwmFreio
        );
    }

    ultimoPWM = valor;
}

// PARADA
// IN1 = LOW
// IN2 = LOW
// Estado do AcionadorDRV8871:
//     Coast / High-Z
// IMPORTANTE:
// Não usamos analogWrite(..., 0) aqui.
// Se fizéssemos isso, o pino continuaria selecionado
// para a função PWM do microcontrolador.
// Em vez disso, retornamos ambos os pinos explicitamente
// para GPIO e colocamos LOW.

void AcionadorDRV8871::parar()
{
    // Retorna ambos os pinos para GPIO

    pinMode(
        pinoIN1,
        OUTPUT
    );

    pinMode(
        pinoIN2,
        OUTPUT
    );

    // Coloca ambos em LOW

    digitalWrite(
        pinoIN1,
        LOW
    );

    digitalWrite(
        pinoIN2,
        LOW
    );

    ultimoPWM = 0;
}

// BRAKE
// IN1 = HIGH
// IN2 = HIGH
// Estado:
//     Brake / low-side decay

void AcionadorDRV8871::frear()
{
    // Os dois pinos precisam estar em GPIO.

    pinMode(
        pinoIN1,
        OUTPUT
    );

    pinMode(
        pinoIN2,
        OUTPUT
    );

    // Ambos HIGH

    digitalWrite(
        pinoIN1,
        HIGH
    );

    digitalWrite(
        pinoIN2,
        HIGH
    );

    // Sentinela diferente de zero para que definirPWM(0)
    // nao confunda BRAKE com COAST.
    ultimoPWM = -4096;
}
