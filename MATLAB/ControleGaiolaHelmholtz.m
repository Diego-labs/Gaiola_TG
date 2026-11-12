classdef ControleGaiolaHelmholtz < handle
    % ============================================================
    % CONTROLE GAIOLA DE HELMHOLTZ
    % ============================================================
    %
    % Interface MATLAB para:
    %
    % - comunicacao serial com o Teensy 4.1;
    % - controle em tempo real da gaiola;
    % - calibracao automatica;
    % - simulacao orbital a partir de TLE;
    % - envio da trajetoria orbital ao Teensy;
    % - supervisao da simulacao acelerada;
    % - download dos arquivos gravados no cartao SD.
    %
    % Para iniciar:
    %
    %   app = ControleGaiolaHelmholtz;
    %
    % ============================================================
    %
    % Arquitetura modular:
    %   InterfaceGaiola.m
    %   ComunicacaoGaiola.m
    %   ControleGaiola.m
    %   CalibracaoGaiola.m
    %   DownloadGaiola.m
    %   OrbitaGaiola.m
    %   UtilGaiola.m
    %   Rel2_Atvi2.m
    %   simular_e_plotar_orbita_2D.m
    %
    % O arquivo principal mantem apenas o estado compartilhado e o
    % ciclo de vida do aplicativo.
    % ============================================================

    properties
        % ========================================================
        % INTERFACE GERAL
        % ========================================================

        JanelaPrincipal
        GrupoAbas
        AbaControle
        AbaOrbita
        GradePrincipal

        % --------------------------------------------------------
        % Status / comunicacao - aba controle
        % --------------------------------------------------------

        ListaPortas
        BotaoAtualizarPortas
        BotaoConectar
        TextoTeensy
        TextoEstado
        TextoCalibracao

        % --------------------------------------------------------
        % Referencias - eixo X
        % --------------------------------------------------------

        ControleCampoX
        ValorCampoX
        TextoFaixaX
        TextoAplicadoX
        TextoMedidoX
        TextoCorrenteX
        TextoPWMX

        % --------------------------------------------------------
        % Referencias - eixo Y
        % --------------------------------------------------------

        ControleCampoY
        ValorCampoY
        TextoFaixaY
        TextoAplicadoY
        TextoMedidoY
        TextoCorrenteY
        TextoPWMY

        % --------------------------------------------------------
        % Referencias - eixo Z
        % --------------------------------------------------------

        ControleCampoZ
        ValorCampoZ
        TextoFaixaZ
        TextoAplicadoZ
        TextoMedidoZ
        TextoCorrenteZ
        TextoPWMZ

        BotaoAplicar
        TextoAlteracao

        % --------------------------------------------------------
        % Graficos - controle em tempo real
        % --------------------------------------------------------

        GraficoControle

        LinhaReferenciaX
        LinhaMedidaX
        LinhaReferenciaY
        LinhaMedidaY
        LinhaReferenciaZ
        LinhaMedidaZ

        % --------------------------------------------------------
        % Comandos - controle em tempo real
        % --------------------------------------------------------

        BotaoIniciar
        BotaoFinalizar
        BotaoZerarCampo
        BotaoCalibracao
        BotaoBaixarDados
        TextoMensagem

        % ========================================================
        % INTERFACE - SIMULACAO ORBITAL
        % ========================================================

        CampoTLE1
        CampoTLE2
        BotaoConfirmarTLE
        TextoEstadoTLE
        TextoConexaoOrbita

        TextoEpocaOrbita
        TextoPeriodoOrbita
        TextoInclinacaoOrbita
        TextoExcentricidadeOrbita
        TextoARNAOrbita
        TextoArgumentoPeriastroOrbita
        TextoAnomaliaVerdadeiraOrbita
        TextoSemiEixoMaiorOrbita
        TextoQuantidadePontosOrbita
        TextoPassoOrbita

        ListaMapeamentoX
        ListaMapeamentoY
        ListaMapeamentoZ
        CampoAceleracaoOrbita
        TextoTaxaReferenciaOrbita
        TextoValidacaoOrbita

        GraficoOrbita
        LinhaOrbitaReferenciaX
        LinhaOrbitaMedidaX
        LinhaOrbitaReferenciaY
        LinhaOrbitaMedidaY
        LinhaOrbitaReferenciaZ
        LinhaOrbitaMedidaZ

        BotaoGerarSimulacao
        BotaoIniciarSimulacao
        BotaoPausarSimulacao
        BotaoFinalizarSimulacao
        BotaoBaixarSimulacao
        TextoEstadoSimulacao
        TextoProgressoSimulacao
        TextoExecucaoSimulacao
        TextoMensagemOrbita

        % ========================================================
        % COMUNICACAO SERIAL
        % ========================================================

        PortaSerial = []
        Conectado = false
        BaudRate = 115200
        DownloadAtivo = false

        TemporizadorHeartbeat = []
        PeriodoHeartbeat_s = 0.25

        % ========================================================
        % ESTADO DO SISTEMA
        % ========================================================

        EstadoSistema = "IDLE"
        EstadoCalibracaoControlador = "DESCONHECIDA"
        LimitesControleTeensy_uT = nan(3, 2)
        % Diagnostico do ultimo erro, independente dos textos de status.
        UltimaFalhaDetalhada = ""
        FalhaGraveNotificada = false
        AvisoCalibracaoPendente = ""
        ReferenciaRejeitadaDetalhada = false

        ReferenciaEditada_uT = [0.0 0.0 0.0]
        ReferenciaAplicada_uT = [NaN NaN NaN]
        ReferenciaAguardando_uT = [NaN NaN NaN]
        ReferenciaFoiAplicada = false
        AlteracaoPendente = false

        DadosControleDisponiveis = false
        DadosControleBaixados = false

        % ========================================================
        % MONITORAMENTO - CONTROLE EM TEMPO REAL
        % ========================================================

        CronometroControle = []
        TempoAcumuladoControle_s = 0.0
        JanelaGrafico_s = 60.0

        % ========================================================
        % CALIBRACAO
        % ========================================================

        EtapaCalibracao = "IDLE"
        ConfiguracaoCalibracao = struct()

        PontosCalibracaoX = zeros(0, 6)
        PontosCalibracaoY = zeros(0, 6)
        PontosCalibracaoZ = zeros(0, 6)

        LimitesCalibracaoX = struct()
        LimitesCalibracaoY = struct()
        LimitesCalibracaoZ = struct()

        ParametrosCalibracaoLocal = struct()
        ParametrosCalibracaoPendentes = struct()
        ParametrosCalibracaoEnvio = struct()
        CaminhoCalibracaoLocal = ""

        % ========================================================
        % SIMULACAO ORBITAL
        % ========================================================

        TLEConfirmado = false
        TLEAtual = ["", ""]
        DadosOrbitaInicial = struct()

        SimulacaoGerada = false
        DadosSimulacaoOrbital = struct()

        MapeamentoOrbita = ["North", "East", "Down"]
        ReferenciasOrbitaXYZ_uT = zeros(0, 3)
        TemposOrbita_s = zeros(0, 1)

        FatorAceleracao = 1.0
        AceleracaoMaximaTeensy = NaN
        TaxaReferenciaAtual_Hz = NaN

        TrajetoriaEnviada = false
        OperacaoOrbitalEmAndamento = false
        InicioSimulacaoPendente = false
        SimulacaoPausada = false

        IndiceSimulacaoAtual = 0
        TempoSimuladoAtual_s = 0.0
        ProgressoSimulacao = 0.0

        IdentificadorEnsaio = ""
        PastaEnsaioAtual = ""

        DadosSimulacaoDisponiveis = false
        DadosSimulacaoBaixados = false
    end

    methods
        % ============================================================
        % CONSTRUTOR
        % ============================================================

        function app = ControleGaiolaHelmholtz()
            app.ParametrosCalibracaoLocal = ...
                CalibracaoGaiola.criarEstruturaCalibracaoVazia(app);

            app.ParametrosCalibracaoPendentes = ...
                CalibracaoGaiola.criarEstruturaCalibracaoVazia(app);

            app.ParametrosCalibracaoEnvio = ...
                CalibracaoGaiola.criarEstruturaCalibracaoVazia(app);

            app.ConfiguracaoCalibracao = struct( ...
                'mascaraEixos', 0, ...
                'limiteSoft_A', NaN, ...
                'limiteHard_A', NaN);

            app.LimitesCalibracaoX = ...
                CalibracaoGaiola.criarLimitesCalibracaoVazios(app);

            app.LimitesCalibracaoY = ...
                CalibracaoGaiola.criarLimitesCalibracaoVazios(app);

            app.LimitesCalibracaoZ = ...
                CalibracaoGaiola.criarLimitesCalibracaoVazios(app);

            CalibracaoGaiola.definirCaminhoCalibracaoLocal(app);
            CalibracaoGaiola.carregarCalibracaoLocal(app);

            InterfaceGaiola.criarInterface(app);
            ComunicacaoGaiola.criarTemporizadorHeartbeat(app);
            ComunicacaoGaiola.atualizarPortasSeriais(app);

            CalibracaoGaiola.aplicarLimitesDaCalibracaoLocal(app);
            InterfaceGaiola.atualizarDisponibilidade(app);
        end

        % ============================================================
        % DESTRUTOR
        % ============================================================

        function delete(app)
            ComunicacaoGaiola.encerrarTemporizadorHeartbeat(app);
            ComunicacaoGaiola.desconectarSerial(app, false);
        end
    end
end
