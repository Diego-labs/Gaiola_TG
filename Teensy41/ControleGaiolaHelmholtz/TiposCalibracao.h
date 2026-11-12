// Estruturas da calibracao e dos ajustes lineares.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#ifndef TIPOS_CALIBRACAO_H
#define TIPOS_CALIBRACAO_H

#include <Arduino.h>

struct SolicitacaoCalibracao
{
    uint8_t mascaraEixos;
    float limiteCorrenteOperacional;
    float limiteCorrenteCritico;
};

struct RegistroPontoCalibracao
{
    char eixo;
    uint16_t indice;
    int16_t pwm;
    float corrente_A;
    float B_uT[3];
};

struct LimitesCalibracaoEixo
{
    bool valido;

    int16_t pwmOperacionalPositivo;
    int16_t pwmOperacionalNegativo;

    int16_t pwmCalibracaoPositivo;
    int16_t pwmCalibracaoNegativo;
};

// Qualidade da regressao linear usada pelo MATLAB: R2 proximo de 1 indica boa aderencia; RMSE e erro maximo ficam nas unidades da grandeza ajustada.
struct QualidadeAjusteLinear
{
    float r2;
    float rmse;
    float erroAbsolutoMaximo;
};

// Versao 2: campo medido diretamente versus PWM. INA226 apenas monitor da fonte.
struct ParametrosCalibracaoEixo
{
    uint8_t valido;
    uint8_t reservado[3];
    float limiteOperacional_A;   // corrente da FONTE, nao corrente da bobina
    float limiteCritico_A;
    int16_t pwmOperacionalPositivo;
    int16_t pwmOperacionalNegativo;
    int16_t pwmCalibracaoPositivo;
    int16_t pwmCalibracaoNegativo;
    float campoEmZero_uT;
    // Modelo continuo: B = B0 + k_sinal * PWM. k vem de medicao B(PWM).
    float coefAngularCampoPwmPositivo;
    float coefAngularCampoPwmNegativo;
    QualidadeAjusteLinear qualidadeCampoPwmPositivo;
    QualidadeAjusteLinear qualidadeCampoPwmNegativo;
};

struct ParametrosCalibracao
{
    uint32_t assinatura;
    uint16_t versao;
    uint8_t mascaraEixosValidos;
    uint8_t reservado;

    ParametrosCalibracaoEixo eixo[3];

    uint32_t crc32;
};

constexpr uint32_t ASSINATURA_PARAMETROS_CALIBRACAO = 0x48434C42UL; // HCLB
constexpr uint16_t VERSAO_PARAMETROS_CALIBRACAO = 2;

inline int indiceEixoCalibracao(char eixo)
{
    switch (eixo)
    {
        case 'X': return 0;
        case 'Y': return 1;
        case 'Z': return 2;
        default:  return -1;
    }
}

inline char letraEixoCalibracao(uint8_t indice)
{
    static const char eixos[3] = {'X', 'Y', 'Z'};
    return (indice < 3) ? eixos[indice] : '-';
}

inline uint8_t bitEixoCalibracao(uint8_t indice)
{
    return (indice < 3) ? (uint8_t)(1u << indice) : 0;
}

#endif
