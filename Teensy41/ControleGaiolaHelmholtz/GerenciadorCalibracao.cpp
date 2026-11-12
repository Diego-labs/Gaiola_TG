// Implementacao da busca de limites e da varredura de calibracao.
// Nomes do protocolo serial, bibliotecas externas e funcoes setup()/loop() permanecem inalterados quando exigidos por compatibilidade.

#include "GerenciadorCalibracao.h"

GerenciadorCalibracao::GerenciadorCalibracao(
    AcionadorDRV8871 &acionadorX,
    AcionadorDRV8871 &acionadorY,
    AcionadorDRV8871 &acionadorZ,
    SensorINA226 &inaX,
    SensorINA226 &inaY,
    SensorINA226 &inaZ,
    ConversorAD7606 &conversorADCRecebido,
    RegistradorDados &registradorRef
)
{
    acionadores[0] = &acionadorX;
    acionadores[1] = &acionadorY;
    acionadores[2] = &acionadorZ;

    sensores[0] = &inaX;
    sensores[1] = &inaY;
    sensores[2] = &inaZ;

    conversorADC = &conversorADCRecebido;
    registrador = &registradorRef;

    inaPronto[0] = false;
    inaPronto[1] = false;
    inaPronto[2] = false;
    adcPronto = false;
    sdPronto = false;

    solicitacao.mascaraEixos = 0;
    solicitacao.limiteCorrenteOperacional = 0.0f;
    solicitacao.limiteCorrenteCritico = 0.0f;

    configurado = false;
    estado = CALIBRACAO_OCIOSA;
    indiceEixoAtivo = 0;
    pwmAtual = 0;

    for (uint8_t i = 0; i < 3; i++)
    {
        limites[i].valido = false;
        limites[i].pwmOperacionalPositivo = 0;
        limites[i].pwmOperacionalNegativo = 0;
        limites[i].pwmCalibracaoPositivo = 0;
        limites[i].pwmCalibracaoNegativo = 0;
    }

    direcaoBusca = 1;
    buscaFina = false;
    pwmCandidatoBusca = 0;
    pwmSeguroAnterior = 0;
    pwmOperacionalBusca = 0;

    indicePontoVarredura = 0;
    indiceLadoVarredura = 0;

    proximaEtapaAposZero = ZERO_PARA_BUSCA_POSITIVA;
    inicioEsperaZero_ms = 0;

    finalidadePonto = PONTO_SEM_FINALIDADE;
    pontoAtivo = false;
    inicioAcomodacaoPonto_ms = 0;
    inicioEsperaAmostra_ms = 0;

    zerarAcumuladorPonto();

    avisoLimiteOperacionalImpresso = false;
}

void GerenciadorCalibracao::definirHardwarePronto(
    bool eixoXPronto,
    bool eixoYPronto,
    bool eixoZPronto,
    bool ad7606Pronto,
    bool registradorPronto
)
{
    inaPronto[0] = eixoXPronto;
    inaPronto[1] = eixoYPronto;
    inaPronto[2] = eixoZPronto;
    adcPronto = ad7606Pronto;
    sdPronto = registradorPronto;
}

bool GerenciadorCalibracao::configurar(const SolicitacaoCalibracao &novaSolicitacao)
{
    if (estaExecutando())
        return false;

    if (novaSolicitacao.mascaraEixos < 1 || novaSolicitacao.mascaraEixos > 7)
        return false;

    if (!isfinite(novaSolicitacao.limiteCorrenteOperacional) ||
        !isfinite(novaSolicitacao.limiteCorrenteCritico))
        return false;

    if (novaSolicitacao.limiteCorrenteOperacional <= 0.0f)
        return false;

    if (novaSolicitacao.limiteCorrenteCritico <= novaSolicitacao.limiteCorrenteOperacional)
        return false;

    // Um limite critico acima da faixa digital do INA226 nao seria
    // monitorado com confiabilidade: rejeitar antes de energizar.
    if (novaSolicitacao.limiteCorrenteCritico >
        ConfiguracoesCalibracao::CORRENTE_MAXIMA_INA_A)
    {
        Serial.print("CAL_WARNING;TYPE=INA_RANGE;HARD=");
        Serial.print(novaSolicitacao.limiteCorrenteCritico, 6);
        Serial.print(";INA_MAX=");
        Serial.println(ConfiguracoesCalibracao::CORRENTE_MAXIMA_INA_A, 6);
        return false;
    }

    for (uint8_t i = 0; i < 3; i++)
    {
        if ((novaSolicitacao.mascaraEixos & bitEixoCalibracao(i)) && !inaPronto[i])
            return false;
    }

    if (!adcPronto || !sdPronto)
        return false;

    solicitacao = novaSolicitacao;
    configurado = true;

    Serial.print("CAL_CONFIG_OK;AXIS_MASK=");
    Serial.print(solicitacao.mascaraEixos);
    Serial.print(";SOFT=");
    Serial.print(solicitacao.limiteCorrenteOperacional, 6);
    Serial.print(";HARD=");
    Serial.println(solicitacao.limiteCorrenteCritico, 6);

    return true;
}

bool GerenciadorCalibracao::iniciar()
{
    if (!configurado || estaExecutando())
        return false;

    for (uint8_t i = 0; i < 3; i++)
    {
        if ((solicitacao.mascaraEixos & bitEixoCalibracao(i)) && !inaPronto[i])
            return false;
    }

    if (!adcPronto || !sdPronto)
        return false;

    pararTodosEixos();

    if (!registrador->iniciarCalibracao(solicitacao))
    {
        Serial.println("ERROR;CAL_SD_START");
        return false;
    }

    for (uint8_t i = 0; i < 3; i++)
    {
        limites[i].valido = false;
        limites[i].pwmOperacionalPositivo = 0;
        limites[i].pwmOperacionalNegativo = 0;
        limites[i].pwmCalibracaoPositivo = 0;
        limites[i].pwmCalibracaoNegativo = 0;
    }

    indiceEixoAtivo = 0;
    if (!selecionarProximoEixo(0))
    {
        registrador->abortarCalibracao();
        return false;
    }

    estado = CALIBRACAO_AGUARDANDO_ZERO;
    pwmAtual = 0;
    avisoLimiteOperacionalImpresso = false;

    Serial.print("CAL_STARTED;AXIS_MASK=");
    Serial.println(solicitacao.mascaraEixos);

    Serial.print("CAL_SETTINGS;PWM_MAX=");
    Serial.print(ConfiguracoesCalibracao::PWM_MAXIMO);
    Serial.print(";INA_MAX=");
    Serial.print(ConfiguracoesCalibracao::CORRENTE_MAXIMA_INA_A, 3);
    Serial.print(";SOFT=");
    Serial.print(solicitacao.limiteCorrenteOperacional, 3);
    Serial.print(";HARD=");
    Serial.println(solicitacao.limiteCorrenteCritico, 3);

    iniciarEixoAtual();
    return true;
}

void GerenciadorCalibracao::atualizar()
{
    if (!estaExecutando())
        return;

    if (pontoAtivo)
        atualizarMedicaoPonto();
}

void GerenciadorCalibracao::abortar(const char *motivo)
{
    if (!estaExecutando())
        return;

    falhar(motivo ? motivo : "USER_ABORT");
}

bool GerenciadorCalibracao::estaConfigurado() const
{
    return configurado;
}

bool GerenciadorCalibracao::estaExecutando() const
{
    return estado == CALIBRACAO_AGUARDANDO_ZERO || estado == CALIBRACAO_AGUARDANDO_PONTO;
}

bool GerenciadorCalibracao::estaConcluido() const
{
    return estado == CALIBRACAO_CONCLUIDA;
}

bool GerenciadorCalibracao::estaEmFalha() const
{
    return estado == CALIBRACAO_FALHA;
}

GerenciadorCalibracao::Estado GerenciadorCalibracao::obterEstado() const
{
    return estado;
}

const char *GerenciadorCalibracao::obterNomeEstado() const
{
    switch (estado)
    {
        case CALIBRACAO_OCIOSA: return "IDLE";
        case CALIBRACAO_AGUARDANDO_ZERO: return "WAIT_ZERO";
        case CALIBRACAO_AGUARDANDO_PONTO: return "WAIT_POINT";
        case CALIBRACAO_CONCLUIDA: return "COMPLETE";
        case CALIBRACAO_FALHA: return "FAULT";
        default: return "UNKNOWN";
    }
}

int16_t GerenciadorCalibracao::obterPWMAtual() const
{
    return pwmAtual;
}

char GerenciadorCalibracao::obterEixoAtivo() const
{
    return letraEixoCalibracao(indiceEixoAtivo);
}

SolicitacaoCalibracao GerenciadorCalibracao::obterSolicitacao() const
{
    return solicitacao;
}

LimitesCalibracaoEixo GerenciadorCalibracao::obterLimitesEixo(uint8_t indiceEixo) const
{
    LimitesCalibracaoEixo limitesInvalidos = {};
    limitesInvalidos.valido = false;

    return (indiceEixo < 3) ? limites[indiceEixo] : limitesInvalidos;
}

void GerenciadorCalibracao::pararTodosEixos()
{
    acionadores[0]->parar();
    acionadores[1]->parar();
    acionadores[2]->parar();
    pwmAtual = 0;
}

void GerenciadorCalibracao::definirPWMDoEixoAtivo(int16_t pwm)
{
    pwm = constrain(
        pwm,
        -ConfiguracoesCalibracao::PWM_MAXIMO,
        ConfiguracoesCalibracao::PWM_MAXIMO
    );

    for (uint8_t i = 0; i < 3; i++)
    {
        if (i == indiceEixoAtivo)
            acionadores[i]->definirPWM(pwm);
        else
            acionadores[i]->parar();
    }

    pwmAtual = pwm;
}

bool GerenciadorCalibracao::selecionarProximoEixo(uint8_t indiceInicial)
{
    for (uint8_t i = indiceInicial; i < 3; i++)
    {
        if (solicitacao.mascaraEixos & bitEixoCalibracao(i))
        {
            indiceEixoAtivo = i;
            return true;
        }
    }

    return false;
}

void GerenciadorCalibracao::iniciarEixoAtual()
{
    pararTodosEixos();
    avisoLimiteOperacionalImpresso = false;

    Serial.print("CAL_AXIS_BEGIN;AXIS=");
    Serial.println(obterEixoAtivo());

    informarEtapa("ZERO_BEFORE_SEARCH");
    iniciarEsperaZero(ZERO_PARA_BUSCA_POSITIVA);
}

void GerenciadorCalibracao::finalizarEixoAtual()
{
    limites[indiceEixoAtivo].valido = true;
    informarLimitesEixo(indiceEixoAtivo);

    char informacoes[220];
    snprintf(
        informacoes,
        sizeof(informacoes),
        "AXIS=%c;PWM_SOFT_POS=%d;PWM_SOFT_NEG=%d;PWM_CAL_POS=%d;PWM_CAL_NEG=%d;SOFT=%.6f;HARD=%.6f",
        obterEixoAtivo(),
        limites[indiceEixoAtivo].pwmOperacionalPositivo,
        limites[indiceEixoAtivo].pwmOperacionalNegativo,
        limites[indiceEixoAtivo].pwmCalibracaoPositivo,
        limites[indiceEixoAtivo].pwmCalibracaoNegativo,
        solicitacao.limiteCorrenteOperacional,
        solicitacao.limiteCorrenteCritico
    );
    registrador->registrarInfoCalibracao(informacoes);

    uint8_t proximo = indiceEixoAtivo + 1;
    if (selecionarProximoEixo(proximo))
    {
        iniciarEixoAtual();
    }
    else
    {
        finalizarCalibracao();
    }
}

void GerenciadorCalibracao::finalizarCalibracao()
{
    pararTodosEixos();

    if (!registrador->finalizarCalibracao())
    {
        falhar("CAL_SD_FINISH");
        return;
    }

    estado = CALIBRACAO_CONCLUIDA;
    pontoAtivo = false;
    finalidadePonto = PONTO_SEM_FINALIDADE;

    Serial.println("CAL_COMPLETE");
}

void GerenciadorCalibracao::iniciarEsperaZero(ProximaEtapaAposZero proximo)
{
    pararTodosEixos();

    proximaEtapaAposZero = proximo;
    inicioEsperaZero_ms = millis();
    estado = CALIBRACAO_AGUARDANDO_ZERO;

    iniciarMedicaoPonto(0, PONTO_VERIFICACAO_ZERO);
}

void GerenciadorCalibracao::processarMedicaoZero(
    const MedicaoPonto &medicao
)
{
    const float toleranciaZero =
        solicitacao.limiteCorrenteOperacional *
        ConfiguracoesCalibracao::FRACAO_TOLERANCIA_ZERO;

    if (medicao.correnteAbsolutaMaxima_A <= toleranciaZero)
    {
        switch (proximaEtapaAposZero)
        {
            case ZERO_PARA_BUSCA_POSITIVA:
                informarEtapa("SEARCH_POSITIVE");
                iniciarBusca(+1);
                break;

            case ZERO_PARA_BUSCA_NEGATIVA:
                informarEtapa("SEARCH_NEGATIVE");
                iniciarBusca(-1);
                break;

            case ZERO_PARA_VARREDURA_ZERO:
                informarEtapa("SWEEP_ZERO");
                iniciarVarredura();
                break;

            case ZERO_PARA_VARREDURA_NEGATIVA:
                informarEtapa("SWEEP_NEGATIVE");
                indiceLadoVarredura = 1;
                agendarProximoPontoNegativo();
                break;

            case ZERO_PARA_PROXIMO_EIXO:
                informarEtapa("AXIS_COMPLETE");
                finalizarEixoAtual();
                break;

            case ZERO_PARA_FINALIZAR:
                finalizarCalibracao();
                break;
        }

        return;
    }

    if ((millis() - inicioEsperaZero_ms) >=
        ConfiguracoesCalibracao::TEMPO_LIMITE_RETORNO_ZERO_MS)
    {
        falhar("ZERO_RETURN_TIMEOUT");
        return;
    }

    // Nova janela de confirmação em zero.
    iniciarMedicaoPonto(0, PONTO_VERIFICACAO_ZERO);
}

void GerenciadorCalibracao::iniciarBusca(int8_t direcao)
{
    direcaoBusca = (direcao >= 0) ? 1 : -1;
    buscaFina = false;

    pwmSeguroAnterior = 0;
    pwmOperacionalBusca = 0;

    pwmCandidatoBusca =
        direcaoBusca *
        ConfiguracoesCalibracao::PASSO_BUSCA_GROSSA;

    iniciarMedicaoPonto(
        pwmCandidatoBusca,
        PONTO_BUSCA
    );
}

void GerenciadorCalibracao::agendarProximoCandidatoBusca()
{
    int16_t passo = buscaFina
        ? ConfiguracoesCalibracao::PASSO_BUSCA_FINA
        : ConfiguracoesCalibracao::PASSO_BUSCA_GROSSA;

    int32_t proximo =
        (int32_t)pwmCandidatoBusca +
        (int32_t)direcaoBusca * passo;

    if (proximo > ConfiguracoesCalibracao::PWM_MAXIMO)
        proximo = ConfiguracoesCalibracao::PWM_MAXIMO;

    if (proximo < -ConfiguracoesCalibracao::PWM_MAXIMO)
        proximo = -ConfiguracoesCalibracao::PWM_MAXIMO;

    if (proximo == pwmCandidatoBusca)
    {
        finalizarDirecaoBusca();
        return;
    }

    pwmCandidatoBusca = (int16_t)proximo;
    iniciarMedicaoPonto(pwmCandidatoBusca, PONTO_BUSCA);
}

void GerenciadorCalibracao::processarMedicaoBusca(
    const MedicaoPonto &medicao
)
{
    const float mediaAbsoluta = fabsf(medicao.mediaCorrente_A);
    const float maximoAbsoluto = medicao.correnteAbsolutaMaxima_A;

    const float limiarBuscaFina =
        solicitacao.limiteCorrenteOperacional *
        ConfiguracoesCalibracao::FRACAO_INICIO_BUSCA_FINA;

    const float operatingTarget =
        solicitacao.limiteCorrenteOperacional *
        (1.0f - ConfiguracoesCalibracao::FRACAO_MARGEM_LIMITE_OPERACIONAL);

    const bool excedeLimiteOperacional =
        (mediaAbsoluta >= solicitacao.limiteCorrenteOperacional) ||
        (maximoAbsoluto >= solicitacao.limiteCorrenteOperacional);

    // BUSCA GROSSA
    // Ao chegar à região próxima do soft limit, voltamos ao
    // último ponto grosso seguro e repetimos a região com passo fino.
    // Isso evita que o passo grosso determine a precisão do limite.

    if (!buscaFina)
    {
        if (excedeLimiteOperacional || maximoAbsoluto >= limiarBuscaFina)
        {
            buscaFina = true;

            pwmCandidatoBusca =
                pwmSeguroAnterior +
                direcaoBusca *
                ConfiguracoesCalibracao::PASSO_BUSCA_FINA;

            if (pwmCandidatoBusca > ConfiguracoesCalibracao::PWM_MAXIMO)
                pwmCandidatoBusca = ConfiguracoesCalibracao::PWM_MAXIMO;

            if (pwmCandidatoBusca < -ConfiguracoesCalibracao::PWM_MAXIMO)
                pwmCandidatoBusca = -ConfiguracoesCalibracao::PWM_MAXIMO;

            iniciarMedicaoPonto(
                pwmCandidatoBusca,
                PONTO_BUSCA
            );

            return;
        }

        // Continua em região segura e ainda distante do limite.
        pwmSeguroAnterior = pwmCandidatoBusca;

        if (maximoAbsoluto <= operatingTarget)
            pwmOperacionalBusca = pwmCandidatoBusca;

        // Se o teto de PWM for atingido sem chegar ao soft limit,
        // completar a calibracao na faixa realmente alcançada e avisar.
        if (abs(pwmCandidatoBusca) >= ConfiguracoesCalibracao::PWM_MAXIMO)
        {
            avisarTetoPWMSemAtingirSoftLimit(medicao);
            finalizarDirecaoBusca();
            return;
        }

        agendarProximoCandidatoBusca();
        return;
    }

    // BUSCA FINA

    if (excedeLimiteOperacional)
    {
        // O PWM que violou o soft limit NÃO é armazenado.
        // O limite é o último ponto confirmado abaixo dele.
        finalizarDirecaoBusca();
        return;
    }

    pwmSeguroAnterior = pwmCandidatoBusca;

    if (maximoAbsoluto <= operatingTarget)
        pwmOperacionalBusca = pwmCandidatoBusca;

    if (abs(pwmCandidatoBusca) >= ConfiguracoesCalibracao::PWM_MAXIMO)
    {
        avisarTetoPWMSemAtingirSoftLimit(medicao);
        finalizarDirecaoBusca();
        return;
    }

    agendarProximoCandidatoBusca();
}

void GerenciadorCalibracao::avisarTetoPWMSemAtingirSoftLimit(
    const MedicaoPonto &medicao)
{
    if (medicao.correnteAbsolutaMaxima_A >=
        solicitacao.limiteCorrenteOperacional)
        return;

    Serial.print("CAL_WARNING;TYPE=PWM_CEILING;AXIS=");
    Serial.print(obterEixoAtivo());
    Serial.print(";PWM=");
    Serial.print(pwmCandidatoBusca);
    Serial.print(";I_MAX=");
    Serial.print(medicao.correnteAbsolutaMaxima_A, 6);
    Serial.print(";SOFT=");
    Serial.println(solicitacao.limiteCorrenteOperacional, 6);
}

void GerenciadorCalibracao::finalizarDirecaoBusca()
{
    // Se o alvo com margem não chegou a ser marcado, usa o último
    // ponto seguro disponível. Isso só deve ocorrer em condições muito
    // particulares, por exemplo quando a corrente sobe abruptamente já
    // nos primeiros counts de PWM.
    if (pwmOperacionalBusca == 0)
        pwmOperacionalBusca = pwmSeguroAnterior;

    if (direcaoBusca > 0)
    {
        limites[indiceEixoAtivo].pwmOperacionalPositivo =
            pwmSeguroAnterior;

        limites[indiceEixoAtivo].pwmCalibracaoPositivo =
            pwmOperacionalBusca;

        informarEtapa("ZERO_AFTER_POSITIVE_SEARCH");
        iniciarEsperaZero(ZERO_PARA_BUSCA_NEGATIVA);
    }
    else
    {
        limites[indiceEixoAtivo].pwmOperacionalNegativo =
            pwmSeguroAnterior;

        limites[indiceEixoAtivo].pwmCalibracaoNegativo =
            pwmOperacionalBusca;

        informarEtapa("ZERO_AFTER_NEGATIVE_SEARCH");
        iniciarEsperaZero(ZERO_PARA_VARREDURA_ZERO);
    }
}

void GerenciadorCalibracao::iniciarVarredura()
{
    indicePontoVarredura = 0;
    indiceLadoVarredura = 0;
    agendarZeroVarredura();
}

void GerenciadorCalibracao::agendarZeroVarredura()
{
    iniciarMedicaoPonto(0, PONTO_VARREDURA_ZERO);
}

void GerenciadorCalibracao::agendarProximoPontoPositivo()
{
    if (indiceLadoVarredura > ConfiguracoesCalibracao::PONTOS_POR_LADO)
    {
        informarEtapa("ZERO_BETWEEN_SWEEPS");
        iniciarEsperaZero(ZERO_PARA_VARREDURA_NEGATIVA);
        return;
    }

    const int16_t pontoFinal =
        limites[indiceEixoAtivo].pwmCalibracaoPositivo;

    int32_t pwm = lroundf(
        ((float)indiceLadoVarredura * (float)pontoFinal) /
        (float)ConfiguracoesCalibracao::PONTOS_POR_LADO
    );

    iniciarMedicaoPonto(
        (int16_t)pwm,
        PONTO_VARREDURA_POSITIVA
    );
}

void GerenciadorCalibracao::agendarProximoPontoNegativo()
{
    if (indiceLadoVarredura > ConfiguracoesCalibracao::PONTOS_POR_LADO)
    {
        informarEtapa("ZERO_AFTER_AXIS");
        iniciarEsperaZero(ZERO_PARA_PROXIMO_EIXO);
        return;
    }

    const int16_t pontoFinal =
        limites[indiceEixoAtivo].pwmCalibracaoNegativo;

    int32_t pwm = lroundf(
        ((float)indiceLadoVarredura * (float)pontoFinal) /
        (float)ConfiguracoesCalibracao::PONTOS_POR_LADO
    );

    iniciarMedicaoPonto(
        (int16_t)pwm,
        PONTO_VARREDURA_NEGATIVA
    );
}

void GerenciadorCalibracao::processarMedicaoVarredura(
    const MedicaoPonto &medicao,
    FinalidadePonto finalidade
)
{
    RegistroPontoCalibracao registro;
    registro.eixo = obterEixoAtivo();
    registro.indice = indicePontoVarredura;
    registro.pwm = pwmAtual;

    // Campo e ajustado diretamente contra PWM no MATLAB.
    // Corrente registrada e a corrente da FONTE (INA em VM).
    registro.corrente_A = medicao.mediaCorrente_A;

    for (uint8_t i = 0; i < 3; i++)
        registro.B_uT[i] = medicao.mediaCampo_uT[i];

    if (!registrador->registrarPontoCalibracao(registro))
    {
        falhar("CAL_SD_POINT_WRITE");
        return;
    }

    informarPonto(registro);

    indicePontoVarredura++;

    // Soft limit durante a varredura é apenas aviso.
    if (!avisoLimiteOperacionalImpresso &&
        medicao.correnteAbsolutaMaxima_A >= solicitacao.limiteCorrenteOperacional)
    {
        avisoLimiteOperacionalImpresso = true;

        Serial.print("CAL_WARNING;TYPE=SOFT_LIMIT;AXIS=");
        Serial.print(obterEixoAtivo());
        Serial.print(";PWM=");
        Serial.print(pwmAtual);
        Serial.print(";I_MAX=");
        Serial.println(medicao.correnteAbsolutaMaxima_A, 6);
    }

    switch (finalidade)
    {
        case PONTO_VARREDURA_ZERO:
            indiceLadoVarredura = 1;
            informarEtapa("SWEEP_POSITIVE");
            agendarProximoPontoPositivo();
            break;

        case PONTO_VARREDURA_POSITIVA:
            indiceLadoVarredura++;
            agendarProximoPontoPositivo();
            break;

        case PONTO_VARREDURA_NEGATIVA:
            indiceLadoVarredura++;
            agendarProximoPontoNegativo();
            break;

        default:
            falhar("CAL_INTERNAL_SWEEP_STATE");
            break;
    }
}

void GerenciadorCalibracao::iniciarMedicaoPonto(
    int16_t pwm,
    FinalidadePonto finalidade
)
{
    definirPWMDoEixoAtivo(pwm);

    finalidadePonto = finalidade;
    pontoAtivo = true;
    estado = (finalidade == PONTO_VERIFICACAO_ZERO)
        ? CALIBRACAO_AGUARDANDO_ZERO
        : CALIBRACAO_AGUARDANDO_PONTO;

    inicioAcomodacaoPonto_ms = millis();
    inicioEsperaAmostra_ms = inicioAcomodacaoPonto_ms;

    zerarAcumuladorPonto();
}

void GerenciadorCalibracao::atualizarMedicaoPonto()
{
    const uint32_t agora = millis();

    if ((agora - inicioAcomodacaoPonto_ms) <
        ConfiguracoesCalibracao::TEMPO_ACOMODACAO_MS)
    {
        return;
    }

    // Verifica ACK I2C antes de usar o dado.
    if (!sensores[indiceEixoAtivo]->estaConectado())
    {
        falhar("INA226_COMM");
        return;
    }

    if (!sensores[indiceEixoAtivo]->conversaoEstaPronta())
    {
        if ((agora - inicioEsperaAmostra_ms) >=
            ConfiguracoesCalibracao::TEMPO_LIMITE_AMOSTRA_INA_MS)
        {
            falhar("INA226_TIMEOUT");
        }

        return;
    }

    float corrente_A = 0.0f;

    if (!lerAmostraCorrente(corrente_A))
    {
        falhar("INA226_READ");
        return;
    }

    const float correnteAbsoluta = fabsf(corrente_A);

    // Hard limit: aborta imediatamente.
    if (correnteAbsoluta >= solicitacao.limiteCorrenteCritico)
    {
        Serial.print("CAL_HARD_LIMIT;AXIS=");
        Serial.print(obterEixoAtivo());
        Serial.print(";PWM=");
        Serial.print(pwmAtual);
        Serial.print(";I=");
        Serial.print(corrente_A, 6);
        Serial.print(";LIMIT=");
        Serial.println(solicitacao.limiteCorrenteCritico, 6);

        falhar("HARD_LIMIT");
        return;
    }

    float B_uT[3];

    if (!lerAmostraCampo(B_uT))
    {
        falhar("AD7606_READ");
        return;
    }

    somaCorrente_A += corrente_A;

    if (correnteAbsoluta > correnteAbsolutaMaxima_A)
        correnteAbsolutaMaxima_A = correnteAbsoluta;

    for (uint8_t i = 0; i < 3; i++)
        somaCampo_uT[i] += B_uT[i];

    amostrasColetadas++;
    inicioEsperaAmostra_ms = agora;

    if (amostrasColetadas >= ConfiguracoesCalibracao::AMOSTRAS_CONFIRMACAO)
        finalizarMedicaoPonto();
}

void GerenciadorCalibracao::zerarAcumuladorPonto()
{
    amostrasColetadas = 0;
    somaCorrente_A = 0.0f;
    correnteAbsolutaMaxima_A = 0.0f;

    for (uint8_t i = 0; i < 3; i++)
        somaCampo_uT[i] = 0.0f;
}

void GerenciadorCalibracao::finalizarMedicaoPonto()
{
    MedicaoPonto resultado;

    resultado.mediaCorrente_A =
        somaCorrente_A /
        (float)amostrasColetadas;

    resultado.correnteAbsolutaMaxima_A = correnteAbsolutaMaxima_A;

    for (uint8_t i = 0; i < 3; i++)
    {
        resultado.mediaCampo_uT[i] =
            somaCampo_uT[i] /
            (float)amostrasColetadas;
    }

    FinalidadePonto finishedPurpose = finalidadePonto;

    pontoAtivo = false;
    finalidadePonto = PONTO_SEM_FINALIDADE;

    tratarResultadoPonto(resultado, finishedPurpose);
}

bool GerenciadorCalibracao::lerAmostraCorrente(float &corrente_A)
{
    corrente_A = sensores[indiceEixoAtivo]->lerCorrente();
    return isfinite(corrente_A);
}

bool GerenciadorCalibracao::lerAmostraCampo(float B_uT[3])
{
    AmostraAD7606 amostra;

    if (!conversorADC->lerAmostra(amostra))
        return false;

    for (uint8_t i = 0; i < 3; i++)
    {
        if (!isfinite(amostra.tensao[i]))
            return false;

        B_uT[i] =
            amostra.tensao[i] /
            ConfiguracoesCalibracao::SENSIBILIDADE_FLUXGATE_V_POR_UT;
    }

    return true;
}


void GerenciadorCalibracao::tratarResultadoPonto(
    const MedicaoPonto &medicao,
    FinalidadePonto finalidade
)
{
    switch (finalidade)
    {
        case PONTO_VERIFICACAO_ZERO:
            processarMedicaoZero(medicao);
            break;

        case PONTO_BUSCA:
            processarMedicaoBusca(medicao);
            break;

        case PONTO_VARREDURA_ZERO:
        case PONTO_VARREDURA_POSITIVA:
        case PONTO_VARREDURA_NEGATIVA:
            processarMedicaoVarredura(medicao, finalidade);
            break;

        default:
            falhar("CAL_INTERNAL_POINT_STATE");
            break;
    }
}

void GerenciadorCalibracao::informarEtapa(const char *nomeEtapa)
{
    Serial.print("CAL_STAGE;AXIS=");
    Serial.print(obterEixoAtivo());
    Serial.print(";STAGE=");
    Serial.println(nomeEtapa);
}

void GerenciadorCalibracao::informarPonto(const RegistroPontoCalibracao &registro)
{
    Serial.print("CAL_POINT;AXIS=");
    Serial.print(registro.eixo);
    Serial.print(";INDEX=");
    Serial.print(registro.indice);
    Serial.print(";PWM=");
    Serial.print(registro.pwm);
    Serial.print(";I=");
    Serial.print(registro.corrente_A, 8);
    Serial.print(";BX=");
    Serial.print(registro.B_uT[0], 8);
    Serial.print(";BY=");
    Serial.print(registro.B_uT[1], 8);
    Serial.print(";BZ=");
    Serial.println(registro.B_uT[2], 8);
}

void GerenciadorCalibracao::informarLimitesEixo(uint8_t indiceEixo)
{
    const LimitesCalibracaoEixo &l = limites[indiceEixo];

    Serial.print("CAL_AXIS_DONE;AXIS=");
    Serial.print(letraEixoCalibracao(indiceEixo));
    Serial.print(";PWM_SOFT_POS=");
    Serial.print(l.pwmOperacionalPositivo);
    Serial.print(";PWM_SOFT_NEG=");
    Serial.print(l.pwmOperacionalNegativo);
    Serial.print(";PWM_CAL_POS=");
    Serial.print(l.pwmCalibracaoPositivo);
    Serial.print(";PWM_CAL_NEG=");
    Serial.println(l.pwmCalibracaoNegativo);
}

void GerenciadorCalibracao::falhar(const char *motivo)
{
    pararTodosEixos();

    pontoAtivo = false;
    finalidadePonto = PONTO_SEM_FINALIDADE;

    registrador->abortarCalibracao();

    estado = CALIBRACAO_FALHA;

    Serial.print("CAL_FAULT;REASON=");
    Serial.println(motivo ? motivo : "UNKNOWN");
}
