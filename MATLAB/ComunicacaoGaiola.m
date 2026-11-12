classdef ComunicacaoGaiola
    % ============================================================
    % COMUNICACAOGAIOLA
    % ============================================================
    % Comunicacao serial, estados e heartbeat da simulacao orbital.
    % ============================================================

    methods (Static)
        % ============================================================
        % HEARTBEAT DA SIMULACAO
        % ============================================================

        function criarTemporizadorHeartbeat(app)
            app.TemporizadorHeartbeat = timer( ...
                'ExecutionMode', 'fixedSpacing', ...
                'Period', app.PeriodoHeartbeat_s, ...
                'BusyMode', 'drop', ...
                'TimerFcn', @(~, ~) ComunicacaoGaiola.enviarHeartbeat(app));
        end

        function iniciarTemporizadorHeartbeat(app)
            if isempty(app.TemporizadorHeartbeat)
                return;
            end

            if strcmp(app.TemporizadorHeartbeat.Running, 'off')
                start(app.TemporizadorHeartbeat);
            end
        end

        function pararTemporizadorHeartbeat(app)
            if isempty(app.TemporizadorHeartbeat)
                return;
            end

            if strcmp(app.TemporizadorHeartbeat.Running, 'on')
                stop(app.TemporizadorHeartbeat);
            end
        end

        function encerrarTemporizadorHeartbeat(app)
            if isempty(app.TemporizadorHeartbeat)
                return;
            end

            try
                stop(app.TemporizadorHeartbeat);
            catch
            end

            try
                delete(app.TemporizadorHeartbeat);
            catch
            end

            app.TemporizadorHeartbeat = [];
        end

        function enviarHeartbeat(app)
            if ~app.Conectado || app.DownloadAtivo || isempty(app.PortaSerial)
                return;
            end

            ativo = app.EstadoSistema == "SIMULATION" || ...
                app.EstadoSistema == "SIMULATION_PAUSED" || ...
                app.InicioSimulacaoPendente;

            if ~ativo
                return;
            end

            ComunicacaoGaiola.enviarComando(app, 'HEARTBEAT');
        end

        % ============================================================
        % PORTAS SERIAIS
        % ============================================================

        function atualizarPortasSeriais(app)
            try
                portas = serialportlist("available");
            catch
                portas = strings(0, 1);
            end

            if isempty(portas)
                app.ListaPortas.Items = {'Nenhuma porta'};
                app.ListaPortas.Value = 'Nenhuma porta';
            else
                itens = cellstr(portas);
                app.ListaPortas.Items = itens;

                if ~any(strcmp(app.ListaPortas.Value, itens))
                    app.ListaPortas.Value = itens{1};
                end
            end
        end

        % ============================================================
        % CONECTAR / DESCONECTAR
        % ============================================================

        function conectarOuDesconectar(app)
            if app.Conectado
                if app.EstadoSistema == "CONTROL" || ...
                        app.EstadoSistema == "SIMULATION" || ...
                        app.EstadoSistema == "SIMULATION_PAUSED" || ...
                        app.EstadoSistema == "RUNNING" || ...
                        app.EtapaCalibracao ~= "IDLE" || ...
                        app.DownloadAtivo || ...
                        app.OperacaoOrbitalEmAndamento
                    uialert(app.JanelaPrincipal, ...
                        'Finalize a operação ativa antes de desconectar.', ...
                        'Sistema ativo');
                    return;
                end

                ComunicacaoGaiola.desconectarSerial(app, true);
                return;
            end

            ComunicacaoGaiola.conectarSerial(app);
        end

        function conectarSerial(app)
            porta = string(app.ListaPortas.Value);

            if porta == "Nenhuma porta" || strlength(porta) == 0
                uialert(app.JanelaPrincipal, ...
                    'Nenhuma porta serial disponível.', ...
                    'Conexão');
                return;
            end

            app.TextoMensagem.Text = 'Conectando ao Teensy...';
            app.TextoMensagemOrbita.Text = 'Conectando ao Teensy...';
            drawnow;

            try
                app.PortaSerial = serialport( ...
                    porta, ...
                    app.BaudRate, ...
                    'Timeout', 0.5);

                configureTerminator(app.PortaSerial, "LF");
                flush(app.PortaSerial);

                writeline(app.PortaSerial, "PING");

                recebeuPong = false;
                inicio = tic;

                while toc(inicio) < 3.0
                    if app.PortaSerial.NumBytesAvailable > 0
                        try
                            linha = strtrim(string(readline(app.PortaSerial)));
                        catch
                            linha = "";
                        end

                        if strlength(linha) == 0
                            pause(0.01);
                            continue;
                        end

                        ComunicacaoGaiola.processarLinhaRecebida(app, linha);

                        if linha == "PONG"
                            recebeuPong = true;
                            break;
                        end
                    end

                    pause(0.01);
                    drawnow limitrate;
                end

                if ~recebeuPong
                    error('O Teensy não respondeu ao comando PING.');
                end

                app.Conectado = true;
                app.BotaoConectar.Text = 'Desconectar';
                app.TextoTeensy.Text = 'Teensy: Conectado';
                app.TextoConexaoOrbita.Text = 'Teensy: conectado';

                if CalibracaoGaiola.calibracaoLocalCompleta(app)
                    app.TextoMensagem.Text = ...
                        'Teensy conectado. Verificando calibração do controlador...';
                else
                    app.TextoMensagem.Text = ...
                        'Teensy conectado. Execute a calibração antes do primeiro controle.';
                end

                app.TextoMensagemOrbita.Text = ...
                    'Teensy conectado. A simulação pode ser preparada.';

                configureCallback( ...
                    app.PortaSerial, ...
                    "terminator", ...
                    @(origem, ~) ComunicacaoGaiola.receberLinhaSerial(app, origem));

                ComunicacaoGaiola.enviarComando(app, 'STATUS');
                ComunicacaoGaiola.enviarComando(app, 'CONTROL_LIMITS');
                InterfaceGaiola.atualizarDisponibilidade(app);

            catch erro
                ComunicacaoGaiola.desconectarSerial(app, false);
                uialert(app.JanelaPrincipal, erro.message, 'Falha de conexão');
            end
        end

        function desconectarSerial(app, atualizarMensagem)
            ComunicacaoGaiola.pararTemporizadorHeartbeat(app);

            if ~isempty(app.PortaSerial)
                try
                    configureCallback(app.PortaSerial, "off");
                catch
                end
            end

            app.PortaSerial = [];
            app.Conectado = false;
            app.EstadoSistema = "IDLE";
            app.EstadoCalibracaoControlador = "DESCONHECIDA";
            app.LimitesControleTeensy_uT = nan(3, 2);
            ComunicacaoGaiola.limparFalhaMemorizada(app);

            % Uma referencia confirmada pertence a sessao serial anterior.
            % Depois de reconectar ela precisa ser aplicada novamente para
            % nao permitir que a interface trate como ativo um BREF que o
            % novo estado do Teensy ainda nao recebeu.
            app.ReferenciaAplicada_uT = [NaN NaN NaN];
            app.ReferenciaAguardando_uT = [NaN NaN NaN];
            app.ReferenciaFoiAplicada = false;
            app.AlteracaoPendente = true;

            % Ao desconectar, volta a apresentar os limites calculados a
            % partir da calibracao local (mesma equacao do firmware).
            CalibracaoGaiola.aplicarLimitesDaCalibracaoLocal(app);

            app.CronometroControle = [];
            app.TempoAcumuladoControle_s = 0.0;
            app.InicioSimulacaoPendente = false;
            app.OperacaoOrbitalEmAndamento = false;
            app.SimulacaoPausada = false;

            if ~isempty(app.BotaoConectar) && isvalid(app.BotaoConectar)
                app.BotaoConectar.Text = 'Conectar';
                app.TextoTeensy.Text = 'Teensy: Desconectado';
                app.TextoEstado.Text = 'Estado: IDLE';
                app.TextoConexaoOrbita.Text = 'Teensy: desconectado';
                app.TextoAplicadoX.Text = '-- µT';
                app.TextoAplicadoY.Text = '-- µT';
                app.TextoAplicadoZ.Text = '-- µT';
                app.TextoAlteracao.Text = 'Referências precisam ser aplicadas após reconectar';
                ComunicacaoGaiola.atualizarTextoCalibracao(app);

                if atualizarMensagem
                    app.TextoMensagem.Text = 'Sistema desconectado.';
                    app.TextoMensagemOrbita.Text = 'Teensy desconectado.';
                end

                InterfaceGaiola.atualizarDisponibilidade(app);
            end
        end

        % ============================================================
        % ENVIO DE COMANDOS
        % ============================================================

        function sucesso = enviarComando(app, comando)
            sucesso = false;

            if ~app.Conectado || isempty(app.PortaSerial)
                return;
            end

            try
                writeline(app.PortaSerial, string(comando));
                sucesso = true;
            catch erro
                if ~isempty(app.TextoMensagem) && isvalid(app.TextoMensagem)
                    app.TextoMensagem.Text = ...
                        ['Falha na comunicação serial: ' erro.message];
                end
                ComunicacaoGaiola.desconectarSerial(app, false);
            end
        end

        % ============================================================
        % RECEPCAO SERIAL
        % ============================================================

        function receberLinhaSerial(app, origem)
            if app.DownloadAtivo || app.OperacaoOrbitalEmAndamento
                return;
            end

            try
                linha = strtrim(string(readline(origem)));
            catch
                return;
            end

            if strlength(linha) == 0
                return;
            end

            ComunicacaoGaiola.processarLinhaRecebida(app, linha);
        end

        % ============================================================
        % PROCESSAMENTO DAS MENSAGENS DO TEENSY
        % ============================================================

        function processarLinhaRecebida(app, linha)
            linha = strtrim(string(linha));

            if strlength(linha) == 0
                return;
            end


            if linha == "PONG" || linha == "HEARTBEAT_OK"
                return;
            end

            if linha == "CLEARED"

                % CLEAR reconhecido pelo firmware: permite novo diagnostico.
                ComunicacaoGaiola.limparFalhaMemorizada(app);
                ComunicacaoGaiola.enviarComando(app, 'STATUS');
                return;
            end

            if linha == "SYSTEM_READY"
                app.TextoMensagem.Text = 'Teensy pronto.';
                return;
            end

            if startsWith(linha, "Calibration params:")
                if contains(linha, "LOADED")
                    app.EstadoCalibracaoControlador = "CARREGADA";
                else
                    app.EstadoCalibracaoControlador = "NAO_CARREGADA";
                end
                ComunicacaoGaiola.atualizarTextoCalibracao(app);
                InterfaceGaiola.atualizarDisponibilidade(app);
                return;
            end

            % --------------------------------------------------------
            % STATUS GERAL
            % --------------------------------------------------------

            if startsWith(linha, "STATUS;")
                estado = UtilGaiola.obterCampoTexto(app, linha, 'STATE');
                if strlength(estado) > 0
                    ComunicacaoGaiola.definirEstadoSistema(app, estado);
                    if estado == "FAULT" && ~app.FalhaGraveNotificada
                        ComunicacaoGaiola.mostrarFalha(app, ...
                            [ 'Teensy em FAULT. O STATUS nao informa a causa ' ...
                              'original; consulte o evento anterior.' ], ...
                            'Falha do controlador', true);
                    end
                end
                return;
            end

            % --------------------------------------------------------
            % LIMITES DO CONTROLADOR
            % --------------------------------------------------------

            if startsWith(linha, "CONTROL_LIMITS")
                limites = nan(3, 2);
                eixos = 'XYZ';

                for indice = 1:3
                    eixo = eixos(indice);
                    minimo = UtilGaiola.obterCampoNumero(app, linha, [eixo '_MIN']);
                    maximo = UtilGaiola.obterCampoNumero(app, linha, [eixo '_MAX']);

                    if isfinite(minimo) && isfinite(maximo) && maximo > minimo
                        limites(indice, :) = [minimo maximo];
                    end
                end

                if all(isfinite(limites(:)))
                    app.LimitesControleTeensy_uT = limites;
                    app.EstadoCalibracaoControlador = "CARREGADA";
                    ComunicacaoGaiola.atualizarTextoCalibracao(app);

                    % Sincroniza imediatamente os sliders/campos numericos
                    % com os limites que o proprio Teensy usa para aceitar
                    % FIELD_REF. Isso elimina discrepancias de arredondamento
                    % ou de criterio entre MATLAB e firmware.
                    CalibracaoGaiola.aplicarLimitesDaCalibracaoLocal(app);
                    OrbitaGaiola.validarLimitesAtuais(app);
                end

                InterfaceGaiola.atualizarDisponibilidade(app);
                return;
            end

            % --------------------------------------------------------
            % REFERENCIA DE CAMPO MANUAL
            % --------------------------------------------------------

            if linha == "FIELD_REF_OK"
                app.ReferenciaRejeitadaDetalhada = false;
                if all(isfinite(app.ReferenciaAguardando_uT))
                    app.ReferenciaAplicada_uT = app.ReferenciaAguardando_uT;
                    app.ReferenciaFoiAplicada = true;
                    app.AlteracaoPendente = false;
                    app.ReferenciaAguardando_uT = [NaN NaN NaN];

                    InterfaceGaiola.atualizarTextosAplicados(app);
                    app.TextoAlteracao.Text = '';

                    if app.EstadoSistema == "CONTROL"
                        app.TextoMensagem.Text = ...
                            'Referências atualizadas durante o controle.';

                        % Confirma por eco do proprio controlador quais
                        % referencias ficaram efetivamente ativas.
                        ComunicacaoGaiola.enviarComando(app, 'CONTROL_STATUS');
                    else
                        app.TextoMensagem.Text = 'Referências aplicadas.';
                    end

                    InterfaceGaiola.atualizarDisponibilidade(app);
                end
                return;
            end

            if linha == "FIELD_REF_REJECTED"
                app.ReferenciaAguardando_uT = [NaN NaN NaN];
                app.TextoMensagem.Text = 'Referência rejeitada. Consulte a janela de erro.';
                if ~app.ReferenciaRejeitadaDetalhada
                    ComunicacaoGaiola.mostrarFalha(app, ...
                        'Referência de campo rejeitada pelo Teensy.', ...
                        'Referência rejeitada', false);
                end
                app.ReferenciaRejeitadaDetalhada = false;
                return;
            end

            % --------------------------------------------------------
            % CONTROLE EM TEMPO REAL
            % --------------------------------------------------------

            if linha == "CONTROL_STARTED"
                ComunicacaoGaiola.limparFalhaMemorizada(app);
                app.DadosControleDisponiveis = false;
                app.DadosControleBaixados = false;
                ComunicacaoGaiola.definirEstadoSistema(app, "CONTROL");
                InterfaceGaiola.iniciarNovoGrafico(app);
                app.TextoMensagem.Text = 'Controle iniciado. Confirmando referências ativas...';

                % Confirma imediatamente o BREF realmente carregado no
                % controlador. Isso evita atribuir uma resposta inesperada do
                % eixo a uma referencia antiga ou nao aplicada.
                ComunicacaoGaiola.enviarComando(app, 'CONTROL_STATUS');
                return;
            end

            if linha == "CONTROL_COMPLETE"
                app.DadosControleDisponiveis = true;
                app.DadosControleBaixados = false;
                app.CronometroControle = [];
                ComunicacaoGaiola.definirEstadoSistema(app, "COMPLETE");
                app.TextoMensagem.Text = ...
                    'Controle finalizado. Os dados podem ser baixados.';
                return;
            end

            if linha == "CONTROL_DOWNLOAD_READY"
                app.DadosControleDisponiveis = true;
                InterfaceGaiola.atualizarDisponibilidade(app);
                return;
            end

            if startsWith(linha, "CONTROL_STATUS;") || ...
                    startsWith(linha, "CONTROL_TELEMETRY;")
                ControleGaiola.processarControlStatus(app, linha);
                return;
            end

            if startsWith(linha, "CONTROL_HARD_LIMIT;") || ...
                    startsWith(linha, "CAL_HARD_LIMIT;") || ...
                    startsWith(linha, "CURRENT_HARD_LIMIT;")
                
                % Mensagem DETALHADA precede o *_FAULT generico; a primeira
                % janela e o diagnostico memorizado nao sao substituidos.
                detalhamento = upper(linha);
                eixo = UtilGaiola.obterCampoTexto(app, detalhamento, 'AXIS');
                corrente = UtilGaiola.obterCampoNumero(app, detalhamento, 'I');
                limite = UtilGaiola.obterCampoNumero(app, detalhamento, 'LIMIT');
                pwm = UtilGaiola.obterCampoNumero(app, detalhamento, 'PWM');
                descricao = sprintf(['Corrente da fonte no eixo %s atingiu o ' ...
                    'hard limit.\nCorrente: %.4f A\nLimite: %.4f A\nPWM: %.0f'], ...
                    char(eixo), corrente, limite, pwm);
                ComunicacaoGaiola.mostrarFalha(app, descricao, ...
                    'Limite crítico de corrente', true);
                return;
            end

            if startsWith(linha, "CONTROL_FAULT;")
                motivo = UtilGaiola.obterCampoTexto(app, linha, 'REASON');
                app.DadosControleDisponiveis = true;
                app.DadosControleBaixados = false;
                ComunicacaoGaiola.definirEstadoSistema(app, "FAULT");
                ComunicacaoGaiola.mostrarFalha(app, ...
                    "Controle interrompido: " + ...
                        ComunicacaoGaiola.descreverCodigo(motivo), ...
                    'Falha no controle', true);
                return;
            end

            % --------------------------------------------------------
            % SIMULACAO ORBITAL
            % --------------------------------------------------------

            if startsWith(linha, "SIM_STARTED;")
                ComunicacaoGaiola.limparFalhaMemorizada(app);
                app.InicioSimulacaoPendente = false;
                app.OperacaoOrbitalEmAndamento = false;
                app.SimulacaoPausada = false;
                app.DadosSimulacaoDisponiveis = false;
                app.DadosSimulacaoBaixados = false;

                aceleracao = UtilGaiola.obterCampoNumero(app, linha, 'ACCEL');
                taxa = UtilGaiola.obterCampoNumero(app, linha, 'MAX_REF_RATE');
                if isfinite(aceleracao), app.FatorAceleracao = aceleracao; end
                if isfinite(taxa), app.TaxaReferenciaAtual_Hz = taxa; end

                ComunicacaoGaiola.definirEstadoSistema(app, "SIMULATION");
                ComunicacaoGaiola.iniciarTemporizadorHeartbeat(app);
                OrbitaGaiola.limparMedicoesGraficoOrbital(app);
                app.TextoMensagemOrbita.Text = 'Simulação orbital iniciada.';
                OrbitaGaiola.anexarEstadoEnsaio(app, 'SIMULATION_STARTED');
                InterfaceGaiola.atualizarDisponibilidade(app);
                return;
            end

            if startsWith(linha, "SIM_PAUSED;")
                app.SimulacaoPausada = true;
                ComunicacaoGaiola.definirEstadoSistema(app, "SIMULATION_PAUSED");
                app.TextoMensagemOrbita.Text = 'Simulação pausada. PWM X/Y/Z = 0.';
                OrbitaGaiola.anexarEstadoEnsaio(app, 'SIMULATION_PAUSED');
                return;
            end

            if linha == "SIM_RESUMED"
                app.SimulacaoPausada = false;
                ComunicacaoGaiola.definirEstadoSistema(app, "SIMULATION");
                app.TextoMensagemOrbita.Text = 'Simulação retomada.';
                OrbitaGaiola.anexarEstadoEnsaio(app, 'SIMULATION_RESUMED');
                return;
            end

            if linha == "SIM_ABORTED"
                app.InicioSimulacaoPendente = false;
                app.OperacaoOrbitalEmAndamento = false;
                app.SimulacaoPausada = false;
                app.DadosSimulacaoDisponiveis = true;
                app.DadosSimulacaoBaixados = false;
                ComunicacaoGaiola.pararTemporizadorHeartbeat(app);
                app.TextoMensagemOrbita.Text = ...
                    'Simulação finalizada pelo usuário. Dados disponíveis.';
                OrbitaGaiola.anexarEstadoEnsaio(app, 'SIMULATION_ABORTED');
                ComunicacaoGaiola.enviarComando(app, 'STATUS');
                InterfaceGaiola.atualizarDisponibilidade(app);
                return;
            end

            if linha == "SIMULATION_COMPLETE"
                app.InicioSimulacaoPendente = false;
                app.OperacaoOrbitalEmAndamento = false;
                app.SimulacaoPausada = false;
                app.DadosSimulacaoDisponiveis = true;
                app.DadosSimulacaoBaixados = false;
                ComunicacaoGaiola.pararTemporizadorHeartbeat(app);
                ComunicacaoGaiola.definirEstadoSistema(app, "COMPLETE");
                app.TextoMensagemOrbita.Text = ...
                    'Simulação concluída. Dados disponíveis para download.';
                OrbitaGaiola.anexarEstadoEnsaio(app, 'SIMULATION_COMPLETE');
                return;
            end

            if linha == "SIM_DOWNLOAD_READY"
                app.DadosSimulacaoDisponiveis = true;
                InterfaceGaiola.atualizarDisponibilidade(app);
                return;
            end

            if startsWith(linha, "SIM_TELEMETRY;") || ...
                    startsWith(linha, "SIM_STATUS;")
                OrbitaGaiola.processarTelemetria(app, linha);
                return;
            end

            if startsWith(linha, "SIM_FAULT;")
                motivo = UtilGaiola.obterCampoTexto(app, linha, 'REASON');
                app.InicioSimulacaoPendente = false;
                app.OperacaoOrbitalEmAndamento = false;
                app.SimulacaoPausada = false;
                app.DadosSimulacaoDisponiveis = true;
                app.DadosSimulacaoBaixados = false;
                ComunicacaoGaiola.pararTemporizadorHeartbeat(app);
                ComunicacaoGaiola.definirEstadoSistema(app, "FAULT");
                app.TextoMensagemOrbita.Text = 'Simulação interrompida por falha.';
                ComunicacaoGaiola.mostrarFalha(app, ...
                    "Simulação interrompida: " + ...
                        ComunicacaoGaiola.descreverCodigo(motivo), ...
                    'Falha na simulação', true);
                OrbitaGaiola.anexarEstadoEnsaio(app, ...
                    ['SIMULATION_FAULT;REASON=' char(motivo)]);
                return;
            end

            if startsWith(linha, "TRAJECTORY_READY;")
                OrbitaGaiola.processarTrajetoriaPronta(app, linha);
                return;
            end

            % --------------------------------------------------------
            % CALIBRACAO
            % --------------------------------------------------------

            if startsWith(linha, "CAL_CONFIG_OK;")
                app.AvisoCalibracaoPendente = "";
                if app.EtapaCalibracao == "AGUARDANDO_CONFIG"
                    app.EtapaCalibracao = "AGUARDANDO_INICIO";
                    app.TextoMensagem.Text = ...
                        'Configuração aceita. Iniciando calibração...';
                    ComunicacaoGaiola.enviarComando(app, 'CAL_START');
                    InterfaceGaiola.atualizarDisponibilidade(app);
                end
                return;
            end

            if startsWith(linha, "CAL_STARTED;")
                ComunicacaoGaiola.limparFalhaMemorizada(app);
                app.EtapaCalibracao = "EXECUTANDO";
                app.BotaoCalibracao.Text = 'Abortar calibração';
                app.TextoMensagem.Text = 'Calibração em andamento.';
                InterfaceGaiola.atualizarDisponibilidade(app);
                return;
            end

            if startsWith(linha, "CAL_STAGE;")
                eixo = UtilGaiola.obterCampoTexto(app, linha, 'AXIS');
                etapa = UtilGaiola.obterCampoTexto(app, linha, 'STAGE');
                app.TextoMensagem.Text = sprintf( ...
                    'Calibração - eixo %s - etapa %s', char(eixo), char(etapa));
                return;
            end

            if startsWith(linha, "CAL_POINT;")
                CalibracaoGaiola.registrarPontoCalibracao(app, linha);
                return;
            end

            if startsWith(linha, "CAL_AXIS_DONE;")
                CalibracaoGaiola.registrarLimitesCalibracao(app, linha);
                return;
            end

            if linha == "CAL_COMPLETE"
                app.EtapaCalibracao = "PROCESSANDO";
                app.BotaoCalibracao.Text = 'Calibração';
                app.TextoMensagem.Text = ...
                    'Calibração adquirida. Processando parâmetros no MATLAB...';
                InterfaceGaiola.atualizarDisponibilidade(app);
                drawnow;
                CalibracaoGaiola.processarCalibracao(app);
                return;
            end

            if startsWith(linha, "CAL_FAULT;")
                motivo = UtilGaiola.obterCampoTexto(app, linha, 'REASON');
                app.EtapaCalibracao = "IDLE";
                app.BotaoCalibracao.Text = 'Calibração';
                app.TextoMensagem.Text = 'Calibração interrompida por falha.';
                ComunicacaoGaiola.mostrarFalha(app, ...
                    "Calibração interrompida: " + ...
                        ComunicacaoGaiola.descreverCodigo(motivo), ...
                    'Falha na calibração', true);
                InterfaceGaiola.atualizarDisponibilidade(app);
                return;
            end

            if startsWith(linha, "CAL_WARNING;")
                tipo = UtilGaiola.obterCampoTexto(app, linha, 'TYPE');
                if tipo == "INA_RANGE"
                    hard = UtilGaiola.obterCampoNumero(app, linha, 'HARD');
                    maxIna = UtilGaiola.obterCampoNumero(app, linha, 'INA_MAX');
                    app.AvisoCalibracaoPendente = sprintf( ...
                        ['Limite hard solicitado: %.4f A\n' ...
                         'Faixa configurada do INA226: %.4f A'], hard, maxIna);
                end
                % Avisos da busca (soft limit, teto de PWM) nao sao falhas;
                % conserva-se a mensagem original na supervisao.
                app.TextoMensagem.Text = char(linha);
                return;
            end

            if linha == "CAL_PARAMS_READY"
                app.TextoMensagem.Text = ...
                    'Parâmetros recebidos pelo Teensy. Salvando no cartão SD...';
                return;
            end

            if linha == "CAL_PARAMS_SAVED"
                CalibracaoGaiola.finalizarSalvamentoCalibracao(app);
                app.EstadoCalibracaoControlador = "CARREGADA";
                ComunicacaoGaiola.enviarComando(app, 'CONTROL_LIMITS');
                return;
            end

            % --------------------------------------------------------
            % ERROS
            % --------------------------------------------------------

            if startsWith(linha, "ERROR;")
                ComunicacaoGaiola.processarErroControlador(app, linha);
                return;
            end
        end

        % ============================================================
        % ERROS DO CONTROLADOR
        % ============================================================

        function limparFalhaMemorizada(app)
            % O evento continua em memoria apos fechar o dialogo; so e
            % zerado quando houve CLEAR, nova execucao ou desconexao.
            app.UltimaFalhaDetalhada = "";
            app.FalhaGraveNotificada = false;
            app.AvisoCalibracaoPendente = "";
            app.ReferenciaRejeitadaDetalhada = false;
        end

        function mostrarFalha(app, mensagem, titulo, falhaGrave)
            mensagem = string(mensagem);
            if falhaGrave && app.FalhaGraveNotificada
                return; % Mensagem detalhada anterior tem prioridade.
            end
            if falhaGrave
                app.FalhaGraveNotificada = true;
            end
            % Um erro posterior nao substitui o diagnostico principal da
            % falha grave antes que o sistema seja liberado.
            if falhaGrave || ~app.FalhaGraveNotificada
                app.UltimaFalhaDetalhada = mensagem;
            end
            if ~isempty(app.JanelaPrincipal) && isvalid(app.JanelaPrincipal)
                % Nao modal: nao bloqueia callback serial nem heartbeat.
                uialert(app.JanelaPrincipal, char(mensagem), char(titulo), ...
                    'Icon', 'error');
            end
        end

        function descricao = descreverCodigo(codigo)
            codigo = upper(string(codigo));
            switch codigo
                case {"HARD_LIMIT", "HARD_CURRENT_LIMIT"}
                    descricao = "limite crítico de corrente atingido";
                case "FIELD_STALE"
                    descricao = "leitura de campo desatualizada";
                case "FIELD_REF_OUT_OF_RANGE"
                    descricao = "referência fora da faixa de campo permitida";
                case "USB_TIMEOUT"
                    descricao = "comunicação USB/heartbeat perdida";
                case "INA226_READ"
                    descricao = "falha de leitura do INA226";
                case "INA226_TIMEOUT"
                    descricao = "tempo limite de leitura do INA226 excedido";
                case "AD7606_READ"
                    descricao = "falha de leitura do AD7606";
                case "SD_FINISH"
                    descricao = "falha ao finalizar gravação no cartão SD";
                case "ZERO_RETURN_TIMEOUT"
                    descricao = "corrente não retornou à região de zero no prazo";
                case "FAULT_CLEAR_REQUIRED"
                    descricao = "é necessário executar CLEAR antes de continuar";
                case "SYSTEM_BUSY"
                    descricao = "o Teensy está ocupado com outra operação";
                case "CALIBRATION_PARAMS_NOT_LOADED"
                    descricao = "parâmetros da calibração não carregados";
                case "CONTROL_CALIBRATION_INVALID"
                    descricao = "parâmetros de calibração inválidos para controle";
                case "SD_NOT_READY"
                    descricao = "cartão SD indisponível";
                case "CONTROL_NOT_RUNNING"
                    descricao = "controle manual não está em execução";
                otherwise
                    descricao = "código " + codigo;
            end
            descricao = descricao + " (" + codigo + ")";
        end

        function processarErroControlador(app, linha)
            erro = extractAfter(linha, "ERROR;");
            codigo = extractBefore(erro + ";", ";");

            if codigo == "CALIBRATION_PARAMS_NOT_LOADED"
                app.EstadoCalibracaoControlador = "NAO_CARREGADA";
                ComunicacaoGaiola.atualizarTextoCalibracao(app);
            end

            if startsWith(codigo, "CAL_") || ...
                    (codigo == "SYSTEM_BUSY" && app.EtapaCalibracao ~= "IDLE")
                app.EtapaCalibracao = "IDLE";
                app.BotaoCalibracao.Text = 'Calibração';
            end

            if codigo == "FAULT_CLEAR_REQUIRED"
                ComunicacaoGaiola.definirEstadoSistema(app, "FAULT");
            end

            erroOrbital = app.InicioSimulacaoPendente || ...
                startsWith(codigo, "SIM_") || startsWith(codigo, "TRAJ_") || ...
                codigo == "TRAJECTORY_NOT_READY" || ...
                codigo == "HEARTBEAT_REQUIRED";

            if erroOrbital
                app.InicioSimulacaoPendente = false;
                app.OperacaoOrbitalEmAndamento = false;
                ComunicacaoGaiola.pararTemporizadorHeartbeat(app);
                app.TextoMensagemOrbita.Text = ...
                    'Comando rejeitado pelo Teensy. Consulte a janela de erro.';
                OrbitaGaiola.anexarEstadoEnsaio(app, ...
                    ['ERROR;' char(erro)]);
            else
                app.TextoMensagem.Text = ...
                    'Comando rejeitado pelo Teensy. Consulte a janela de erro.';
            end

            if codigo == "FIELD_REF_RANGE"
                eixo = UtilGaiola.obterCampoTexto(app, linha, 'AXIS');
                valor = UtilGaiola.obterCampoNumero(app, linha, 'VALUE');
                minimo = UtilGaiola.obterCampoNumero(app, linha, 'MIN');
                maximo = UtilGaiola.obterCampoNumero(app, linha, 'MAX');
                app.ReferenciaRejeitadaDetalhada = true;
                mensagem = sprintf(['Referência fora dos limites no eixo %s.\n' ...
                    'Solicitada: %.6f µT\nFaixa: %.6f a %.6f µT'], ...
                    char(eixo), valor, minimo, maximo);
            elseif codigo == "FIELD_LIMIT_UNAVAILABLE"
                eixo = UtilGaiola.obterCampoTexto(app, linha, 'AXIS');
                mensagem = "Limites de campo indisponíveis no eixo " + eixo;
            elseif codigo == "CAL_CONFIG_REJECTED" && ...
                    strlength(app.AvisoCalibracaoPendente) > 0
                mensagem = "Configuração de calibração rejeitada. " + ...
                    app.AvisoCalibracaoPendente;
                app.AvisoCalibracaoPendente = "";
            else
                mensagem = "Comando rejeitado: " + ...
                    ComunicacaoGaiola.descreverCodigo(codigo);
                if codigo == "SIM_ACCEL_TOO_HIGH"
                    maximo = UtilGaiola.obterCampoNumero(app, linha, 'MAX');
                    mensagem = sprintf( ...
                        'Aceleração orbital acima do limite (máximo %.3g).', maximo);
                elseif contains(linha, ";AXIS=")
                    eixo = UtilGaiola.obterCampoTexto(app, linha, 'AXIS');
                    mensagem = mensagem + newline + "Eixo: " + eixo;
                end
            end
            if ~(codigo == "FAULT_CLEAR_REQUIRED" && app.FalhaGraveNotificada)
                ComunicacaoGaiola.mostrarFalha(app, mensagem, ...
                    'Erro comunicado pelo Teensy', false);
            end
            InterfaceGaiola.atualizarDisponibilidade(app);
        end

        % ============================================================
        % ESTADO DO SISTEMA
        % ============================================================

        function definirEstadoSistema(app, estado)
            estado = upper(string(estado));
            app.EstadoSistema = estado;
            app.TextoEstado.Text = ['Estado: ' char(estado)];

            if estado == "FAULT"
                app.TextoEstado.FontColor = [0.80 0.00 0.00];
                app.TextoEstado.FontWeight = 'bold';
            else
                app.TextoEstado.FontColor = [0 0 0];
                app.TextoEstado.FontWeight = 'normal';
            end

            if ~(estado == "SIMULATION" || estado == "SIMULATION_PAUSED") && ...
                    ~app.InicioSimulacaoPendente
                ComunicacaoGaiola.pararTemporizadorHeartbeat(app);
            end

            if ~isempty(app.TextoEstadoSimulacao) && isvalid(app.TextoEstadoSimulacao)
                if estado == "SIMULATION"
                    app.TextoEstadoSimulacao.Text = 'Estado: executando';
                elseif estado == "SIMULATION_PAUSED"
                    app.TextoEstadoSimulacao.Text = 'Estado: pausada';
                elseif app.SimulacaoGerada
                    app.TextoEstadoSimulacao.Text = ['Estado do sistema: ' char(estado)];
                end
            end

            InterfaceGaiola.atualizarDisponibilidade(app);
        end

        function atualizarTextoCalibracao(app)
            switch app.EstadoCalibracaoControlador
                case "CARREGADA"
                    app.TextoCalibracao.Text = 'Calibração: carregada';
                case "NAO_CARREGADA"
                    app.TextoCalibracao.Text = 'Calibração: não carregada';
                otherwise
                    app.TextoCalibracao.Text = 'Calibração: desconhecida';
            end
        end
    end
end
