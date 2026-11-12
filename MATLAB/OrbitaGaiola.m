classdef OrbitaGaiola
    % ============================================================
    % ORBITAGAIOlA
    % ============================================================
    %
    % Preparacao e supervisao da simulacao orbital:
    %
    % - validacao/interpretação do TLE;
    % - execucao da propagacao orbital existente;
    % - mapeamento North/East/Down -> X/Y/Z da gaiola;
    % - validacao de toda a serie contra os limites calibrados;
    % - envio da trajetoria para o Teensy 4.1;
    % - fator de aceleracao;
    % - inicio, pausa, retomada e finalizacao da simulacao;
    % - criacao da pasta local e preservacao dos dados do ensaio.
    %
    % O Teensy executa a temporizacao da trajetoria. O MATLAB envia a
    % trajetoria completa antes do inicio e permanece como supervisor.
    % ============================================================

    methods (Static)
        % ============================================================
        % TLE
        % ============================================================

        function tleEditado(app)
            if app.EstadoSistema == "SIMULATION" || ...
                    app.EstadoSistema == "SIMULATION_PAUSED" || ...
                    app.OperacaoOrbitalEmAndamento
                return;
            end

            % O conteudo digitado so passa a ser usado depois de uma nova
            % confirmacao. Isso evita gerar uma orbita com um TLE antigo
            % enquanto a tela mostra linhas diferentes.
            app.TLEConfirmado = false;
            app.SimulacaoGerada = false;
            app.TrajetoriaEnviada = false;
            app.DadosOrbitaInicial = struct();
            app.DadosSimulacaoOrbital = struct();
            app.ReferenciasOrbitaXYZ_uT = zeros(0, 3);
            app.TemposOrbita_s = zeros(0, 1);
            app.AceleracaoMaximaTeensy = NaN;

            app.TextoEstadoTLE.Text = 'TLE: alterado; confirme novamente';
            app.TextoEstadoTLE.FontWeight = 'normal';
            app.TextoEstadoTLE.FontColor = [0 0 0];
            app.TextoEstadoSimulacao.Text = 'Estado: aguardando confirmação do TLE';
            app.TextoMensagemOrbita.Text = 'Confirme o TLE antes de gerar a simulação.';

            OrbitaGaiola.limparInformacoesOrbita(app);
            OrbitaGaiola.limparGraficoOrbital(app);
            OrbitaGaiola.atualizarResumoAceleracao(app);
            InterfaceGaiola.atualizarDisponibilidade(app);
        end

        function confirmarTLE(app)
            if app.OperacaoOrbitalEmAndamento || ...
                    app.EstadoSistema == "SIMULATION" || ...
                    app.EstadoSistema == "SIMULATION_PAUSED"
                return;
            end

            linha1 = strtrim(string(app.CampoTLE1.Value));
            linha2 = strtrim(string(app.CampoTLE2.Value));

            if strlength(linha1) < 2 || strlength(linha2) < 2 || ...
                    ~startsWith(linha1, "1 ") || ~startsWith(linha2, "2 ")
                OrbitaGaiola.marcarTLEInvalido(app, ...
                    'Formato inválido. O TLE deve possuir as linhas 1 e 2.');
                return;
            end

            tle = char([linha1; linha2]);

            app.TextoMensagemOrbita.Text = 'Interpretando TLE...';
            app.BotaoConfirmarTLE.Enable = 'off';
            drawnow;

            try
                dados = Rel2_Atvi2(tle, false);

                app.TLEAtual = [linha1 linha2];
                app.TLEConfirmado = true;
                app.DadosOrbitaInicial = dados;

                % Uma alteracao de TLE invalida qualquer simulacao anterior.
                app.SimulacaoGerada = false;
                app.DadosSimulacaoOrbital = struct();
                app.ReferenciasOrbitaXYZ_uT = zeros(0, 3);
                app.TemposOrbita_s = zeros(0, 1);
                app.TrajetoriaEnviada = false;
                app.AceleracaoMaximaTeensy = NaN;
                app.TaxaReferenciaAtual_Hz = NaN;
                app.TextoEstadoTLE.Text = 'TLE: confirmado';
                app.TextoEstadoTLE.FontWeight = 'bold';
                app.TextoEstadoTLE.FontColor = [0.00 0.45 0.00];
                app.TextoEstadoSimulacao.Text = 'Estado: TLE confirmado';
                app.TextoMensagemOrbita.Text = ...
                    'TLE confirmado. Gere a simulação orbital.';

                OrbitaGaiola.atualizarInformacoesOrbita(app, dados, false);
                OrbitaGaiola.limparGraficoOrbital(app);
                OrbitaGaiola.atualizarResumoAceleracao(app);

            catch erro
                OrbitaGaiola.marcarTLEInvalido(app, erro.message);
            end

            InterfaceGaiola.atualizarDisponibilidade(app);
        end

        function marcarTLEInvalido(app, mensagem)
            app.TLEConfirmado = false;
            app.SimulacaoGerada = false;
            app.TrajetoriaEnviada = false;
            app.DadosOrbitaInicial = struct();
            app.DadosSimulacaoOrbital = struct();
            app.ReferenciasOrbitaXYZ_uT = zeros(0, 3);
            app.TemposOrbita_s = zeros(0, 1);

            app.TextoEstadoTLE.Text = 'TLE: inválido';
            app.TextoEstadoTLE.FontWeight = 'bold';
            app.TextoEstadoTLE.FontColor = [0.80 0.00 0.00];
            app.TextoEstadoSimulacao.Text = 'Estado: aguardando TLE válido';
            app.TextoMensagemOrbita.Text = char(string(mensagem));

            OrbitaGaiola.limparInformacoesOrbita(app);
            OrbitaGaiola.limparGraficoOrbital(app);
            InterfaceGaiola.atualizarDisponibilidade(app);
        end

        % ============================================================
        % GERACAO DA SIMULACAO
        % ============================================================

        function gerarSimulacao(app)
            if ~app.TLEConfirmado || app.OperacaoOrbitalEmAndamento
                return;
            end

            app.OperacaoOrbitalEmAndamento = true;
            app.TextoMensagemOrbita.Text = ...
                'Gerando órbita e campo magnético. Aguarde...';
            app.TextoEstadoSimulacao.Text = 'Estado: gerando simulação';
            InterfaceGaiola.atualizarDisponibilidade(app);
            drawnow;

            try
                tle = char([app.TLEAtual(1); app.TLEAtual(2)]);
                dados = Rel2_Atvi2(tle, true);

                if ~isfield(dados, 'simulacao') || ...
                        ~isfield(dados.simulacao, 't') || ...
                        numel(dados.simulacao.t) < 2
                    error('A rotina orbital não retornou uma trajetória válida.');
                end

                app.DadosOrbitaInicial = dados;
                app.DadosSimulacaoOrbital = dados.simulacao;
                app.TemposOrbita_s = dados.simulacao.t(:);
                app.SimulacaoGerada = true;
                app.TrajetoriaEnviada = false;
                app.AceleracaoMaximaTeensy = NaN;

                OrbitaGaiola.atualizarInformacoesOrbita(app, dados, true);
                OrbitaGaiola.atualizarReferenciasMapeadas(app);
                OrbitaGaiola.atualizarGraficoOrbital(app);
                OrbitaGaiola.atualizarResumoAceleracao(app);
                OrbitaGaiola.validarLimitesAtuais(app);

                app.TextoEstadoSimulacao.Text = 'Estado: simulação gerada';
                app.TextoMensagemOrbita.Text = sprintf( ...
                    'Simulação gerada com %d pontos.', ...
                    numel(app.TemposOrbita_s));

            catch erro
                app.SimulacaoGerada = false;
                app.TrajetoriaEnviada = false;
                app.DadosSimulacaoOrbital = struct();
                app.ReferenciasOrbitaXYZ_uT = zeros(0, 3);
                app.TemposOrbita_s = zeros(0, 1);
                app.TextoEstadoSimulacao.Text = 'Estado: falha ao gerar';
                app.TextoMensagemOrbita.Text = ['Falha: ' erro.message];

                uialert(app.JanelaPrincipal, erro.message, ...
                    'Simulação orbital');
            end

            app.OperacaoOrbitalEmAndamento = false;
            InterfaceGaiola.atualizarDisponibilidade(app);
        end

        % ============================================================
        % MAPEAMENTO NED -> XYZ
        % ============================================================

        function mapeamentoAlterado(app)
            app.MapeamentoOrbita = [ ...
                string(app.ListaMapeamentoX.Value), ...
                string(app.ListaMapeamentoY.Value), ...
                string(app.ListaMapeamentoZ.Value)];

            app.TrajetoriaEnviada = false;
            app.AceleracaoMaximaTeensy = NaN;

            if app.SimulacaoGerada
                OrbitaGaiola.atualizarReferenciasMapeadas(app);
                OrbitaGaiola.atualizarGraficoOrbital(app);
                OrbitaGaiola.validarLimitesAtuais(app);
            else
                [valido, mensagem] = OrbitaGaiola.validarMapeamento(app);
                if ~valido
                    app.TextoValidacaoOrbita.Text = mensagem;
                    app.TextoValidacaoOrbita.FontColor = [0.80 0.00 0.00];
                end
            end

            InterfaceGaiola.atualizarDisponibilidade(app);
        end

        function [valido, mensagem] = validarMapeamento(app)
            mapa = [ ...
                string(app.ListaMapeamentoX.Value), ...
                string(app.ListaMapeamentoY.Value), ...
                string(app.ListaMapeamentoZ.Value)];

            valido = false;
            mensagem = '';

            permitidos = ["North" "East" "Down"];

            if any(~ismember(mapa, permitidos)) || numel(unique(mapa)) ~= 3
                mensagem = 'Mapeamento inválido: use North, East e Down uma vez cada.';
                return;
            end

            % Cada linha descreve qual eixo NED passa a representar o eixo
            % X/Y/Z da gaiola. Como NED e XYZ sao sistemas destros, a matriz
            % de permutacao deve ter determinante +1.
            P = zeros(3, 3);

            for linha = 1:3
                coluna = find(permitidos == mapa(linha), 1);
                P(linha, coluna) = 1;
            end

            if det(P) < 0.5
                mensagem = ...
                    'Mapeamento inválido: a permutação não preserva a regra da mão direita.';
                return;
            end

            valido = true;
            mensagem = 'Mapeamento NED → XYZ válido.';
        end

        function atualizarReferenciasMapeadas(app)
            if ~app.SimulacaoGerada || isempty(app.DadosSimulacaoOrbital)
                app.ReferenciasOrbitaXYZ_uT = zeros(0, 3);
                return;
            end

            [valido, ~] = OrbitaGaiola.validarMapeamento(app);

            if ~valido
                app.ReferenciasOrbitaXYZ_uT = zeros(0, 3);
                return;
            end

            sim = app.DadosSimulacaoOrbital;
            ned = [sim.Bn_uT(:), sim.Be_uT(:), sim.Bd_uT(:)];
            nomes = ["North" "East" "Down"];
            mapa = [ ...
                string(app.ListaMapeamentoX.Value), ...
                string(app.ListaMapeamentoY.Value), ...
                string(app.ListaMapeamentoZ.Value)];

            referencias = zeros(size(ned));

            for eixo = 1:3
                indice = find(nomes == mapa(eixo), 1);
                referencias(:, eixo) = ned(:, indice);
            end

            app.MapeamentoOrbita = mapa;
            app.ReferenciasOrbitaXYZ_uT = referencias;
        end

        % ============================================================
        % ACELERACAO
        % ============================================================

        function aceleracaoAlterada(app)
            valor = app.CampoAceleracaoOrbita.Value;

            if ~isfinite(valor) || valor <= 0
                valor = 1.0;
                app.CampoAceleracaoOrbita.Value = valor;
            end

            app.FatorAceleracao = valor;
            OrbitaGaiola.atualizarResumoAceleracao(app);
            InterfaceGaiola.atualizarDisponibilidade(app);
        end

        function atualizarResumoAceleracao(app)
            if ~app.SimulacaoGerada || numel(app.TemposOrbita_s) < 2
                app.TextoTaxaReferenciaOrbita.Text = ...
                    'Taxa de referência: --';
                return;
            end

            dt = min(diff(app.TemposOrbita_s));
            aceleracao = app.CampoAceleracaoOrbita.Value;

            if ~isfinite(dt) || dt <= 0 || ...
                    ~isfinite(aceleracao) || aceleracao <= 0
                app.TextoTaxaReferenciaOrbita.Text = ...
                    'Taxa de referência: inválida';
                return;
            end

            taxa = aceleracao / dt;
            intervaloReal = dt / aceleracao;
            app.TaxaReferenciaAtual_Hz = taxa;

            texto = sprintf( ...
                'Taxa: %.3f Hz | intervalo real: %.4f s', ...
                taxa, intervaloReal);

            if isfinite(app.AceleracaoMaximaTeensy)
                texto = sprintf('%s | máx. Teensy: %.3fx', ...
                    texto, app.AceleracaoMaximaTeensy);
            end

            app.TextoTaxaReferenciaOrbita.Text = texto;
        end

        % ============================================================
        % VALIDACAO DE LIMITES
        % ============================================================

        function [valido, mensagem] = validarLimitesAtuais(app)
            valido = false;
            mensagem = 'Validação de limites: aguardando simulação';

            if ~app.SimulacaoGerada
                app.TextoValidacaoOrbita.Text = mensagem;
                app.TextoValidacaoOrbita.FontColor = [0 0 0];
                return;
            end

            [mapaValido, mensagemMapa] = OrbitaGaiola.validarMapeamento(app);
            if ~mapaValido
                mensagem = mensagemMapa;
                app.TextoValidacaoOrbita.Text = mensagem;
                app.TextoValidacaoOrbita.FontColor = [0.80 0.00 0.00];
                return;
            end

            if isempty(app.ReferenciasOrbitaXYZ_uT)
                mensagem = 'Validação de limites: referências XYZ não disponíveis.';
                app.TextoValidacaoOrbita.Text = mensagem;
                app.TextoValidacaoOrbita.FontColor = [0.80 0.00 0.00];
                return;
            end

            if ~CalibracaoGaiola.calibracaoLocalCompleta(app)
                mensagem = 'Validação de limites: calibração local incompleta.';
                app.TextoValidacaoOrbita.Text = mensagem;
                app.TextoValidacaoOrbita.FontColor = [0.80 0.00 0.00];
                return;
            end

            limitesLocais = zeros(3, 2);
            eixos = 'XYZ';

            for eixo = 1:3
                limitesLocais(eixo, :) = ...
                    CalibracaoGaiola.obterLimitesCampoEixo(app, eixos(eixo));
            end

            [okLocal, detalhe] = OrbitaGaiola.validarSerieContraLimites( ...
                app.ReferenciasOrbitaXYZ_uT, limitesLocais);

            if ~okLocal
                mensagem = ['Fora dos limites locais: ' detalhe];
                app.TextoValidacaoOrbita.Text = mensagem;
                app.TextoValidacaoOrbita.FontColor = [0.80 0.00 0.00];
                return;
            end

            if all(isfinite(app.LimitesControleTeensy_uT(:)))
                [okTeensy, detalheTeensy] = ...
                    OrbitaGaiola.validarSerieContraLimites( ...
                    app.ReferenciasOrbitaXYZ_uT, ...
                    app.LimitesControleTeensy_uT);

                if ~okTeensy
                    mensagem = ['Fora dos limites do Teensy: ' detalheTeensy];
                    app.TextoValidacaoOrbita.Text = mensagem;
                    app.TextoValidacaoOrbita.FontColor = [0.80 0.00 0.00];
                    return;
                end

                mensagem = 'Validação de limites: OK (MATLAB + Teensy).';
            else
                mensagem = ...
                    'Validação de limites: OK local; Teensy será verificado ao iniciar.';
            end

            valido = true;
            app.TextoValidacaoOrbita.Text = mensagem;
            app.TextoValidacaoOrbita.FontColor = [0.00 0.45 0.00];
        end

        function [valido, detalhe] = validarSerieContraLimites(referencias, limites)
            valido = false;
            detalhe = '';

            if isempty(referencias) || size(referencias, 2) ~= 3 || ...
                    ~isequal(size(limites), [3 2]) || ...
                    any(~isfinite(limites(:)))
                detalhe = 'dados de validação inválidos.';
                return;
            end

            eixos = 'XYZ';

            for eixo = 1:3
                minimoSerie = min(referencias(:, eixo));
                maximoSerie = max(referencias(:, eixo));

                if ~isfinite(minimoSerie) || ~isfinite(maximoSerie)
                    detalhe = sprintf('eixo %c contém valores não finitos.', eixos(eixo));
                    return;
                end

                if minimoSerie < limites(eixo, 1) || ...
                        maximoSerie > limites(eixo, 2)
                    detalhe = sprintf( ...
                        ['eixo %c requer [%.3f, %.3f] µT, ' ...
                         'disponível [%.3f, %.3f] µT.'], ...
                        eixos(eixo), ...
                        minimoSerie, maximoSerie, ...
                        limites(eixo, 1), limites(eixo, 2));
                    return;
                end
            end

            valido = true;
            detalhe = 'OK';
        end

        % ============================================================
        % INICIO DA SIMULACAO
        % ============================================================

        function iniciarSimulacao(app)
            if ~app.Conectado || app.OperacaoOrbitalEmAndamento || ...
                    ~app.SimulacaoGerada
                return;
            end

            if app.EstadoSistema == "FAULT"
                uialert(app.JanelaPrincipal, ...
                    'O Teensy está em FAULT. Recupere o sistema antes de iniciar.', ...
                    'Simulação orbital');
                return;
            end

            if ~CalibracaoGaiola.calibracaoLocalCompleta(app) || ...
                    app.EstadoCalibracaoControlador ~= "CARREGADA"
                uialert(app.JanelaPrincipal, ...
                    'A calibração deve estar válida no MATLAB e carregada no Teensy.', ...
                    'Simulação orbital');
                return;
            end

            [mapaValido, mensagemMapa] = OrbitaGaiola.validarMapeamento(app);
            if ~mapaValido
                uialert(app.JanelaPrincipal, mensagemMapa, 'Mapeamento NED → XYZ');
                return;
            end

            OrbitaGaiola.atualizarReferenciasMapeadas(app);
            [limitesValidos, mensagemLimites] = ...
                OrbitaGaiola.validarLimitesAtuais(app);

            if ~limitesValidos
                uialert(app.JanelaPrincipal, mensagemLimites, ...
                    'Limites da gaiola');
                return;
            end

            aceleracao = app.CampoAceleracaoOrbita.Value;
            if ~isfinite(aceleracao) || aceleracao <= 0
                uialert(app.JanelaPrincipal, ...
                    'Informe um fator de aceleração maior que zero.', ...
                    'Simulação orbital');
                return;
            end

            if app.DadosSimulacaoDisponiveis && ~app.DadosSimulacaoBaixados
                resposta = uiconfirm(app.JanelaPrincipal, ...
                    ['Existem dados de uma simulação anterior no Teensy que ainda ' ...
                     'não foram baixados. Uma nova simulação substituirá esses arquivos.'], ...
                    'Dados pendentes', ...
                    'Options', {'Continuar', 'Cancelar'}, ...
                    'DefaultOption', 2, ...
                    'CancelOption', 2);

                if ~strcmp(resposta, 'Continuar')
                    return;
                end
            end

            % A partir daqui o usuario confirmou o inicio de um novo ensaio.
            % Se havia um ensaio anterior, sua pasta local permanece no disco,
            % mas deixa de ser a pasta ativa da interface.
            app.IdentificadorEnsaio = "";
            app.PastaEnsaioAtual = "";

            app.OperacaoOrbitalEmAndamento = true;
            app.TextoEstadoSimulacao.Text = 'Estado: preparando Teensy';
            app.TextoMensagemOrbita.Text = ...
                'Validando limites e enviando trajetória ao Teensy...';
            InterfaceGaiola.atualizarDisponibilidade(app);
            drawnow;

            try
                [limitesTeensy, aceleracaoMaxima] = ...
                    OrbitaGaiola.prepararTrajetoriaNoTeensy(app);

                app.LimitesControleTeensy_uT = limitesTeensy;
                app.AceleracaoMaximaTeensy = aceleracaoMaxima;
                app.EstadoCalibracaoControlador = "CARREGADA";
                ComunicacaoGaiola.atualizarTextoCalibracao(app);

                [okRemoto, detalheRemoto] = ...
                    OrbitaGaiola.validarSerieContraLimites( ...
                    app.ReferenciasOrbitaXYZ_uT, limitesTeensy);

                if ~okRemoto
                    error('A trajetória excede os limites do Teensy: %s', detalheRemoto);
                end

                if isfinite(aceleracaoMaxima) && aceleracao > aceleracaoMaxima + 1e-6
                    error( ...
                        ['Fator de aceleração %.3fx acima do máximo permitido ' ...
                         'para esta trajetória (%.3fx).'], ...
                        aceleracao, aceleracaoMaxima);
                end

                % A pasta definitiva só é criada depois que todas as
                % validações passaram e a trajetória foi aceita pelo Teensy.
                OrbitaGaiola.criarPastaEnsaio(app);
                OrbitaGaiola.salvarArquivosEnsaio(app);
                OrbitaGaiola.atualizarResumoAceleracao(app);
                OrbitaGaiola.validarLimitesAtuais(app);

                app.DadosSimulacaoDisponiveis = false;
                app.DadosSimulacaoBaixados = false;
                app.InicioSimulacaoPendente = true;
                app.OperacaoOrbitalEmAndamento = false;

                % O firmware V7 exige heartbeat recente para SIM_START.
                ComunicacaoGaiola.enviarComando(app, 'HEARTBEAT');
                pause(0.03);
                ComunicacaoGaiola.iniciarTemporizadorHeartbeat(app);

                comando = sprintf('SIM_START;ACCEL=%.6f', aceleracao);

                if ~ComunicacaoGaiola.enviarComando(app, comando)
                    app.InicioSimulacaoPendente = false;
                    ComunicacaoGaiola.pararTemporizadorHeartbeat(app);
                    error('Não foi possível enviar SIM_START ao Teensy.');
                end

                app.TextoEstadoSimulacao.Text = 'Estado: aguardando início';
                app.TextoMensagemOrbita.Text = ...
                    'Trajetória enviada. Aguardando confirmação do Teensy...';
                OrbitaGaiola.anexarEstadoEnsaio(app, ...
                    sprintf('SIM_START_REQUEST;ACCEL=%.6f', aceleracao));

            catch erro
                app.OperacaoOrbitalEmAndamento = false;
                app.InicioSimulacaoPendente = false;
                ComunicacaoGaiola.pararTemporizadorHeartbeat(app);
                app.TextoEstadoSimulacao.Text = 'Estado: simulação preparada';
                app.TextoMensagemOrbita.Text = ['Falha ao iniciar: ' erro.message];
                OrbitaGaiola.anexarEstadoEnsaio(app, ...
                    ['START_FAILED;' erro.message]);

                uialert(app.JanelaPrincipal, erro.message, ...
                    'Início da simulação');
            end

            InterfaceGaiola.atualizarDisponibilidade(app);
        end

        % ============================================================
        % ENVIO DA TRAJETORIA AO TEENSY
        % ============================================================

        function [limitesTeensy, aceleracaoMaxima] = ...
                prepararTrajetoriaNoTeensy(app)

            if isempty(app.PortaSerial)
                error('Porta serial não disponível.');
            end

            referencias = app.ReferenciasOrbitaXYZ_uT;
            tempos = app.TemposOrbita_s(:);

            if size(referencias, 1) ~= numel(tempos) || numel(tempos) < 2
                error('Trajetória orbital inconsistente. Gere novamente a simulação.');
            end

            if any(diff(tempos) <= 0)
                error('Os tempos da trajetória devem ser estritamente crescentes.');
            end

            % Identificador usado tanto no Teensy quanto na pasta local.
            app.IdentificadorEnsaio = OrbitaGaiola.gerarIdentificadorEnsaio();

            configureCallback(app.PortaSerial, "off");
            timeoutAnterior = app.PortaSerial.Timeout;
            app.PortaSerial.Timeout = 2.0;

            restaurar = onCleanup(@() ...
                OrbitaGaiola.restaurarSerialAposOperacao(app, timeoutAnterior));

            flush(app.PortaSerial, "input");
            uploadIniciado = false;

            try
            % --------------------------------------------------------
            % 1) Consulta os limites que o Teensy calcula a partir da
            %    calibracao realmente carregada no controlador.
            % --------------------------------------------------------

            writeline(app.PortaSerial, "CONTROL_LIMITS");
            linhaLimites = OrbitaGaiola.aguardarLinhaSerial( ...
                app, "CONTROL_LIMITS", 3.0);

            limitesTeensy = OrbitaGaiola.extrairLimitesControle( ...
                app, linhaLimites);

            [okRemoto, detalheRemoto] = ...
                OrbitaGaiola.validarSerieContraLimites( ...
                referencias, limitesTeensy);

            if ~okRemoto
                error('A trajetória excede os limites do Teensy: %s', detalheRemoto);
            end

            % --------------------------------------------------------
            % 2) Inicio do upload da trajetória.
            % --------------------------------------------------------

            quantidade = numel(tempos);
            comandoInicio = sprintf( ...
                'TRAJ_BEGIN;ID=%s;N=%d', ...
                char(app.IdentificadorEnsaio), quantidade);

            writeline(app.PortaSerial, comandoInicio);
            OrbitaGaiola.aguardarLinhaSerial(app, "TRAJ_BEGIN_OK;", 5.0);
            uploadIniciado = true;

            % O firmware confirma o progresso a cada 64 pontos. Enviar
            % em blocos evita inundar a recepcao e permite atualizar a UI.
            tamanhoBloco = 64;

            for inicioBloco = 1:tamanhoBloco:quantidade
                fimBloco = min(inicioBloco + tamanhoBloco - 1, quantidade);

                for indice = inicioBloco:fimBloco
                    ref = referencias(indice, :);
                    comandoPonto = sprintf( ...
                        ['TRAJ_POINT;I=%d;T=%.6f;' ...
                         'BX=%.6f;BY=%.6f;BZ=%.6f'], ...
                        indice - 1, ...
                        tempos(indice), ...
                        ref(1), ref(2), ref(3));

                    writeline(app.PortaSerial, comandoPonto);
                end

                linhaProgresso = OrbitaGaiola.aguardarLinhaSerial( ...
                    app, "TRAJ_PROGRESS;", 15.0);

                recebidos = UtilGaiola.obterCampoNumero( ...
                    app, linhaProgresso, 'RECEIVED');

                if ~isfinite(recebidos) || round(recebidos) ~= fimBloco
                    error( ...
                        'Confirmação de upload inconsistente no ponto %d.', ...
                        fimBloco);
                end

                app.TextoMensagemOrbita.Text = sprintf( ...
                    'Enviando trajetória: %d/%d pontos (%.1f%%)', ...
                    fimBloco, quantidade, 100 * fimBloco / quantidade);
                app.TextoProgressoSimulacao.Text = sprintf( ...
                    'Upload: %.1f%%', 100 * fimBloco / quantidade);
                drawnow limitrate;
            end

            % --------------------------------------------------------
            % 3) Finaliza o upload e recebe o limite de aceleracao.
            % --------------------------------------------------------

            writeline(app.PortaSerial, "TRAJ_END");
            linhaPronta = OrbitaGaiola.aguardarLinhaSerial( ...
                app, "TRAJECTORY_READY;", 8.0);

            aceleracaoMaxima = UtilGaiola.obterCampoNumero( ...
                app, linhaPronta, 'MAX_ACCEL');

            if ~isfinite(aceleracaoMaxima) || aceleracaoMaxima <= 0
                error('O Teensy não informou um limite de aceleração válido.');
            end

            uploadIniciado = false;
            app.TrajetoriaEnviada = true;
            app.AceleracaoMaximaTeensy = aceleracaoMaxima;
            app.TextoProgressoSimulacao.Text = 'Progresso: 0,0%';

            catch erro
                if uploadIniciado && ~isempty(app.PortaSerial)
                    try
                        writeline(app.PortaSerial, "TRAJ_ABORT");
                        pause(0.05);
                    catch
                    end
                end
                rethrow(erro);
            end

            clear restaurar;
        end

        function linha = aguardarLinhaSerial(app, prefixo, tempoLimite_s)
            linha = "";
            inicio = tic;

            while toc(inicio) < tempoLimite_s
                if isempty(app.PortaSerial)
                    error('A porta serial foi desconectada.');
                end

                if app.PortaSerial.NumBytesAvailable > 0
                    try
                        recebida = strtrim(string(readline(app.PortaSerial)));
                    catch
                        recebida = "";
                    end

                    if strlength(recebida) == 0
                        continue;
                    end

                    if startsWith(recebida, "ERROR;") || ...
                            startsWith(recebida, "SIM_FAULT;") || ...
                            startsWith(recebida, "CONTROL_FAULT;")
                        error('Teensy respondeu: %s', char(recebida));
                    end

                    if startsWith(recebida, prefixo)
                        linha = recebida;
                        return;
                    end

                    % Mensagens residuais que nao pertencem a operacao
                    % sincrona sao apenas ignoradas aqui. Fora desta fase
                    % o callback serial volta a trata-las normalmente.
                end

                pause(0.002);
                drawnow limitrate;
            end

            error('Tempo limite aguardando resposta "%s" do Teensy.', ...
                char(prefixo));
        end

        function limites = extrairLimitesControle(app, linha)
            limites = nan(3, 2);
            eixos = 'XYZ';

            for indice = 1:3
                minimo = UtilGaiola.obterCampoNumero( ...
                    app, linha, [eixos(indice) '_MIN']);
                maximo = UtilGaiola.obterCampoNumero( ...
                    app, linha, [eixos(indice) '_MAX']);

                if ~isfinite(minimo) || ~isfinite(maximo) || maximo <= minimo
                    error('Limites inválidos recebidos para o eixo %c.', ...
                        eixos(indice));
                end

                limites(indice, :) = [minimo maximo];
            end
        end

        function restaurarSerialAposOperacao(app, timeoutAnterior)
            if isempty(app.PortaSerial)
                return;
            end

            try
                app.PortaSerial.Timeout = timeoutAnterior;
            catch
            end

            if app.Conectado
                try
                    configureCallback( ...
                        app.PortaSerial, ...
                        "terminator", ...
                        @(origem, ~) ComunicacaoGaiola.receberLinhaSerial(app, origem));
                catch
                end
            end
        end

        function processarTrajetoriaPronta(app, linha)
            aceleracaoMaxima = UtilGaiola.obterCampoNumero( ...
                app, linha, 'MAX_ACCEL');

            if isfinite(aceleracaoMaxima) && aceleracaoMaxima > 0
                app.AceleracaoMaximaTeensy = aceleracaoMaxima;
            end

            app.TrajetoriaEnviada = true;
            OrbitaGaiola.atualizarResumoAceleracao(app);
            InterfaceGaiola.atualizarDisponibilidade(app);
        end

        % ============================================================
        % PAUSA / RETOMADA / FINALIZACAO
        % ============================================================

        function pausarOuRetomarSimulacao(app)
            if app.EstadoSistema == "SIMULATION"
                app.TextoMensagemOrbita.Text = 'Solicitando pausa...';
                ComunicacaoGaiola.enviarComando(app, 'SIM_PAUSE');
                return;
            end

            if app.EstadoSistema == "SIMULATION_PAUSED"
                app.TextoMensagemOrbita.Text = 'Solicitando retomada...';
                ComunicacaoGaiola.enviarComando(app, 'SIM_RESUME');
            end
        end

        function finalizarSimulacao(app)
            if ~(app.EstadoSistema == "SIMULATION" || ...
                    app.EstadoSistema == "SIMULATION_PAUSED")
                return;
            end

            resposta = uiconfirm(app.JanelaPrincipal, ...
                ['Finalizar antes do último ponto encerrará o ensaio no Teensy ' ...
                 'como uma simulação abortada. Os dados já adquiridos serão preservados.'], ...
                'Finalizar simulação', ...
                'Options', {'Finalizar', 'Cancelar'}, ...
                'DefaultOption', 2, ...
                'CancelOption', 2);

            if ~strcmp(resposta, 'Finalizar')
                return;
            end

            app.TextoMensagemOrbita.Text = 'Finalizando simulação...';
            ComunicacaoGaiola.enviarComando(app, 'SIM_ABORT');
        end

        % ============================================================
        % TELEMETRIA
        % ============================================================

        function processarTelemetria(app, linha)
            estado = UtilGaiola.obterCampoTexto(app, linha, 'STATE');

            if strlength(estado) > 0
                estado = upper(string(estado));

                if estado == "SIMULATION" || estado == "SIMULATION_PAUSED"
                    ComunicacaoGaiola.definirEstadoSistema(app, estado);
                end
            end

            tempo = UtilGaiola.obterCampoNumero(app, linha, 'T');
            indice = UtilGaiola.obterCampoNumero(app, linha, 'K');
            progresso = UtilGaiola.obterCampoNumero(app, linha, 'P');

            if ~isfinite(progresso)
                progresso = UtilGaiola.obterCampoNumero(app, linha, 'PROGRESS');
            end

            if isfinite(tempo)
                app.TempoSimuladoAtual_s = tempo;
            end

            if isfinite(indice)
                app.IndiceSimulacaoAtual = round(indice);
            end

            if isfinite(progresso)
                app.ProgressoSimulacao = progresso;
            end

            app.TextoProgressoSimulacao.Text = sprintf( ...
                'Progresso: %.1f%%', app.ProgressoSimulacao);

            app.TextoExecucaoSimulacao.Text = sprintf( ...
                't = %.1f s | ponto = %d', ...
                app.TempoSimuladoAtual_s, app.IndiceSimulacaoAtual);

            aceleracao = UtilGaiola.obterCampoNumero(app, linha, 'ACCEL');
            if isfinite(aceleracao) && aceleracao > 0
                app.FatorAceleracao = aceleracao;
                app.CampoAceleracaoOrbita.Value = aceleracao;
                OrbitaGaiola.atualizarResumoAceleracao(app);
            end

            % A telemetria SIM_TELEMETRY da Versao 7 fornece o campo
            % medido dos tres eixos. A referencia orbital completa ja esta
            % desenhada como linha continua; aqui acrescentamos somente os
            % pontos medidos no instante de tempo simulado informado pelo
            % Teensy.
            bx = UtilGaiola.obterCampoNumero(app, linha, 'X_B');
            by = UtilGaiola.obterCampoNumero(app, linha, 'Y_B');
            bz = UtilGaiola.obterCampoNumero(app, linha, 'Z_B');

            if isfinite(tempo) && all(isfinite([bx by bz]))
                OrbitaGaiola.adicionarMedicaoGraficoOrbital( ...
                    app, tempo, [bx by bz]);
            end
        end

        % ============================================================
        % PASTA E ARQUIVOS LOCAIS
        % ============================================================

        function identificador = gerarIdentificadorEnsaio()
            instante = datetime('now', 'Format', 'yyyyMMdd_HHmmss');
            identificador = "ENS_" + string(instante);
        end

        function criarPastaEnsaio(app)
            pastaPrograma = fileparts(mfilename('fullpath'));
            pastaDados = fullfile(pastaPrograma, 'Dados_salvos');

            if ~isfolder(pastaDados)
                mkdir(pastaDados);
            end

            % Usa o mesmo instante codificado no identificador enviado
            % ao Teensy. Assim ID e nome da pasta sempre se referem ao
            % mesmo ensaio, mesmo quando o upload da trajetoria e longo.
            id = char(app.IdentificadorEnsaio);
            partes = regexp(id, ...
                '^ENS_(\d{4})(\d{2})(\d{2})_(\d{2})(\d{2})(\d{2})$', ...
                'tokens', 'once');

            if isempty(partes)
                instante = datetime('now', 'Format', 'yyyy-MM-dd_HH-mm-ss');
                nomeBase = "Ensaio_" + string(instante);
            else
                nomeBase = string(sprintf( ...
                    'Ensaio_%s-%s-%s_%s-%s-%s', ...
                    partes{1}, partes{2}, partes{3}, ...
                    partes{4}, partes{5}, partes{6}));
            end

            pasta = fullfile(pastaDados, char(nomeBase));
            contador = 2;

            while isfolder(pasta)
                pasta = fullfile(pastaDados, ...
                    sprintf('%s_%02d', char(nomeBase), contador));
                contador = contador + 1;
            end

            [ok, mensagem] = mkdir(pasta);
            if ~ok
                error('Não foi possível criar a pasta do ensaio: %s', mensagem);
            end

            app.PastaEnsaioAtual = string(pasta);
        end

        function salvarArquivosEnsaio(app)
            if strlength(app.PastaEnsaioAtual) == 0 || ...
                    ~isfolder(char(app.PastaEnsaioAtual))
                error('Pasta do ensaio não disponível.');
            end

            pasta = char(app.PastaEnsaioAtual);
            sim = app.DadosSimulacaoOrbital;
            aceleracao = app.CampoAceleracaoOrbita.Value;

            writetable(sim.tabela_estado, ...
                fullfile(pasta, 'vetor_estado.csv'));

            writetable(sim.tabela_campo, ...
                fullfile(pasta, 'campo_magnetico.csv'));

            quantidade = numel(app.TemposOrbita_s);
            indice = uint32((0:(quantidade - 1)).');
            tempoOrbital_s = app.TemposOrbita_s(:);
            tempoExecucaoPrevisto_s = ...
                (tempoOrbital_s - tempoOrbital_s(1)) ./ aceleracao;

            referencias = app.ReferenciasOrbitaXYZ_uT;

            tabelaReferencias = table( ...
                indice, ...
                tempoOrbital_s, ...
                tempoExecucaoPrevisto_s, ...
                referencias(:, 1), ...
                referencias(:, 2), ...
                referencias(:, 3), ...
                'VariableNames', { ...
                    'Indice', ...
                    'Tempo_orbital_s', ...
                    'Tempo_execucao_previsto_s', ...
                    'Bx_ref_uT', ...
                    'By_ref_uT', ...
                    'Bz_ref_uT'});

            writetable(tabelaReferencias, ...
                fullfile(pasta, 'referencias_enviadas.csv'));

            OrbitaGaiola.gravarInformacoesOrbita(app);
        end

        function gravarInformacoesOrbita(app)
            caminho = fullfile( ...
                char(app.PastaEnsaioAtual), ...
                'informacoes_orbita.txt');

            arquivo = fopen(caminho, 'w');
            if arquivo < 0
                error('Não foi possível criar informacoes_orbita.txt.');
            end

            fechar = onCleanup(@() fclose(arquivo));

            dados = app.DadosOrbitaInicial;
            sim = app.DadosSimulacaoOrbital;
            elementos = dados.elementos_orbitais_iniciais;
            epoca = OrbitaGaiola.formatarEpoca(dados.data, dados.tempo);
            mapa = app.MapeamentoOrbita;
            aceleracao = app.CampoAceleracaoOrbita.Value;
            dt = min(diff(app.TemposOrbita_s));
            duracaoPrevista = ...
                (app.TemposOrbita_s(end) - app.TemposOrbita_s(1)) / aceleracao;

            fprintf(arquivo, 'GAIOLA DE HELMHOLTZ - INFORMACOES DA ORBITA\n');
            fprintf(arquivo, '=============================================\n\n');
            fprintf(arquivo, 'ID_ENSAIO=%s\n', char(app.IdentificadorEnsaio));
            fprintf(arquivo, 'DATA_GERACAO=%s\n', ...
                char(string(datetime('now', 'Format', 'yyyy-MM-dd HH:mm:ss'))));
            fprintf(arquivo, '\n[TLE]\n');
            fprintf(arquivo, 'LINHA_1=%s\n', char(app.TLEAtual(1)));
            fprintf(arquivo, 'LINHA_2=%s\n', char(app.TLEAtual(2)));
            fprintf(arquivo, 'EPOCA=%s\n', epoca);
            fprintf(arquivo, 'ANO_DECIMAL=%.10f\n', dados.ano_decimal);

            r0 = dados.R_km(:);
            v0 = dados.V_km_s(:);
            fprintf(arquivo, '\n[VETOR_ESTADO_INICIAL]\n');
            fprintf(arquivo, 'R0_X_KM=%.12f\n', r0(1));
            fprintf(arquivo, 'R0_Y_KM=%.12f\n', r0(2));
            fprintf(arquivo, 'R0_Z_KM=%.12f\n', r0(3));
            fprintf(arquivo, 'V0_X_KM_S=%.12f\n', v0(1));
            fprintf(arquivo, 'V0_Y_KM_S=%.12f\n', v0(2));
            fprintf(arquivo, 'V0_Z_KM_S=%.12f\n', v0(3));

            fprintf(arquivo, '\n[ELEMENTOS_ORBITAIS_INICIAIS]\n');
            fprintf(arquivo, 'INCLINACAO_DEG=%.10f\n', rad2deg(elementos(2)));
            fprintf(arquivo, 'EXCENTRICIDADE=%.12f\n', elementos(3));
            fprintf(arquivo, 'ARNA_DEG=%.10f\n', rad2deg(elementos(4)));
            fprintf(arquivo, 'ARGUMENTO_PERIASTRO_DEG=%.10f\n', rad2deg(elementos(5)));
            fprintf(arquivo, 'ANOMALIA_VERDADEIRA_DEG=%.10f\n', rad2deg(elementos(6)));
            fprintf(arquivo, 'SEMI_EIXO_MAIOR_KM=%.10f\n', elementos(7));
            fprintf(arquivo, 'PERIODO_ORBITAL_S=%.10f\n', dados.periodo_s);
            fprintf(arquivo, 'PERIODO_ORBITAL_MIN=%.10f\n', dados.periodo_min);

            fprintf(arquivo, '\n[PROPAGACAO]\n');
            fprintf(arquivo, 'MODELO=DOIS_CORPOS_ODE45\n');
            fprintf(arquivo, 'RELTOL=1e-6\n');
            fprintf(arquivo, 'ABSTOL=1e-10\n');
            fprintf(arquivo, 'QUANTIDADE_PONTOS=%d\n', numel(sim.t));
            fprintf(arquivo, 'PRIMEIRO_TEMPO_S=%.9f\n', sim.t(1));
            fprintf(arquivo, 'ULTIMO_TEMPO_S=%.9f\n', sim.t(end));
            fprintf(arquivo, 'MENOR_INTERVALO_S=%.9f\n', dt);
            fprintf(arquivo, 'MODELO_CAMPO=IGRF\n');

            fprintf(arquivo, '\n[MAPEAMENTO_NED_XYZ]\n');
            fprintf(arquivo, 'X=%s\n', char(mapa(1)));
            fprintf(arquivo, 'Y=%s\n', char(mapa(2)));
            fprintf(arquivo, 'Z=%s\n', char(mapa(3)));

            fprintf(arquivo, '\n[EXECUCAO]\n');
            fprintf(arquivo, 'FATOR_ACELERACAO=%.9f\n', aceleracao);
            fprintf(arquivo, 'TAXA_REFERENCIA_HZ=%.9f\n', aceleracao / dt);
            fprintf(arquivo, 'INTERVALO_REAL_REFERENCIA_S=%.9f\n', dt / aceleracao);
            fprintf(arquivo, 'DURACAO_REAL_PREVISTA_S=%.9f\n', duracaoPrevista);

            if isfinite(app.AceleracaoMaximaTeensy)
                fprintf(arquivo, 'ACELERACAO_MAXIMA_TEENSY=%.9f\n', ...
                    app.AceleracaoMaximaTeensy);
            end

            fprintf(arquivo, '\n[LIMITES_CAMPO_UT]\n');
            eixos = 'XYZ';
            for eixo = 1:3
                local = CalibracaoGaiola.obterLimitesCampoEixo(app, eixos(eixo));
                fprintf(arquivo, '%c_LOCAL_MIN=%.9f\n', eixos(eixo), local(1));
                fprintf(arquivo, '%c_LOCAL_MAX=%.9f\n', eixos(eixo), local(2));

                if all(isfinite(app.LimitesControleTeensy_uT(eixo, :)))
                    fprintf(arquivo, '%c_TEENSY_MIN=%.9f\n', ...
                        eixos(eixo), app.LimitesControleTeensy_uT(eixo, 1));
                    fprintf(arquivo, '%c_TEENSY_MAX=%.9f\n', ...
                        eixos(eixo), app.LimitesControleTeensy_uT(eixo, 2));
                end
            end

            fprintf(arquivo, '\n[ARQUIVOS_MATLAB]\n');
            fprintf(arquivo, 'VETOR_ESTADO=vetor_estado.csv\n');
            fprintf(arquivo, 'CAMPO_MAGNETICO=campo_magnetico.csv\n');
            fprintf(arquivo, 'REFERENCIAS_ENVIADAS=referencias_enviadas.csv\n');

            fprintf(arquivo, '\n[EVENTOS_LOCAIS]\n');
            fprintf(arquivo, '%s : ENSAIO_PREPARADO\n', ...
                char(string(datetime('now', 'Format', 'yyyy-MM-dd HH:mm:ss.SSS'))));

            clear fechar;
        end

        function anexarEstadoEnsaio(app, evento)
            if strlength(app.PastaEnsaioAtual) == 0 || ...
                    ~isfolder(char(app.PastaEnsaioAtual))
                return;
            end

            caminho = fullfile( ...
                char(app.PastaEnsaioAtual), ...
                'informacoes_orbita.txt');

            arquivo = fopen(caminho, 'a');
            if arquivo < 0
                return;
            end

            fprintf(arquivo, '%s : %s\n', ...
                char(string(datetime('now', ...
                    'Format', 'yyyy-MM-dd HH:mm:ss.SSS'))), ...
                char(string(evento)));
            fclose(arquivo);
        end

        % ============================================================
        % INTERFACE - INFORMACOES E GRAFICO
        % ============================================================

        function atualizarInformacoesOrbita(app, dados, simulacaoCompleta)
            elementos = dados.elementos_orbitais_iniciais;

            app.TextoEpocaOrbita.Text = ...
                OrbitaGaiola.formatarEpoca(dados.data, dados.tempo);
            app.TextoPeriodoOrbita.Text = sprintf('%.2f min', dados.periodo_min);
            app.TextoInclinacaoOrbita.Text = sprintf('%.4f °', rad2deg(elementos(2)));
            app.TextoExcentricidadeOrbita.Text = sprintf('%.8f', elementos(3));
            app.TextoARNAOrbita.Text = sprintf('%.4f °', rad2deg(elementos(4)));
            app.TextoArgumentoPeriastroOrbita.Text = ...
                sprintf('%.4f °', rad2deg(elementos(5)));
            app.TextoAnomaliaVerdadeiraOrbita.Text = ...
                sprintf('%.4f °', rad2deg(elementos(6)));
            app.TextoSemiEixoMaiorOrbita.Text = sprintf('%.3f km', elementos(7));

            if simulacaoCompleta && isfield(dados.simulacao, 't')
                app.TextoQuantidadePontosOrbita.Text = ...
                    sprintf('%d', numel(dados.simulacao.t));
                app.TextoPassoOrbita.Text = ...
                    sprintf('%.6f s', dados.simulacao.passo_minimo_s);
            else
                app.TextoQuantidadePontosOrbita.Text = '--';
                app.TextoPassoOrbita.Text = '1 s (configurado)';
            end
        end

        function texto = formatarEpoca(data, tempo)
            texto = '--';

            try
                data = double(data(:));
                tempo = double(tempo(:));

                if numel(data) >= 3 && numel(tempo) >= 3
                    texto = sprintf( ...
                        '%02d/%02d/%04d %02d:%02d:%06.3f', ...
                        round(data(1)), round(data(2)), round(data(3)), ...
                        round(tempo(1)), round(tempo(2)), tempo(3));
                end
            catch
            end
        end

        function limparInformacoesOrbita(app)
            app.TextoEpocaOrbita.Text = '--';
            app.TextoPeriodoOrbita.Text = '--';
            app.TextoInclinacaoOrbita.Text = '--';
            app.TextoExcentricidadeOrbita.Text = '--';
            app.TextoARNAOrbita.Text = '--';
            app.TextoArgumentoPeriastroOrbita.Text = '--';
            app.TextoAnomaliaVerdadeiraOrbita.Text = '--';
            app.TextoSemiEixoMaiorOrbita.Text = '--';
            app.TextoQuantidadePontosOrbita.Text = '--';
            app.TextoPassoOrbita.Text = '--';
        end

        function limparGraficoOrbital(app)
            if isempty(app.LinhaOrbitaReferenciaX) || ...
                    ~isvalid(app.LinhaOrbitaReferenciaX)
                return;
            end

            app.LinhaOrbitaReferenciaX.XData = NaN;
            app.LinhaOrbitaReferenciaX.YData = NaN;
            clearpoints(app.LinhaOrbitaMedidaX);

            app.LinhaOrbitaReferenciaY.XData = NaN;
            app.LinhaOrbitaReferenciaY.YData = NaN;
            clearpoints(app.LinhaOrbitaMedidaY);

            app.LinhaOrbitaReferenciaZ.XData = NaN;
            app.LinhaOrbitaReferenciaZ.YData = NaN;
            clearpoints(app.LinhaOrbitaMedidaZ);
        end

        function limparMedicoesGraficoOrbital(app)
            if isempty(app.LinhaOrbitaMedidaX) || ...
                    ~isvalid(app.LinhaOrbitaMedidaX)
                return;
            end

            clearpoints(app.LinhaOrbitaMedidaX);
            clearpoints(app.LinhaOrbitaMedidaY);
            clearpoints(app.LinhaOrbitaMedidaZ);
        end

        function atualizarGraficoOrbital(app)
            if ~app.SimulacaoGerada || isempty(app.ReferenciasOrbitaXYZ_uT)
                OrbitaGaiola.limparGraficoOrbital(app);
                return;
            end

            t = app.TemposOrbita_s;
            referencias = app.ReferenciasOrbitaXYZ_uT;

            app.LinhaOrbitaReferenciaX.XData = t;
            app.LinhaOrbitaReferenciaX.YData = referencias(:, 1);
            app.LinhaOrbitaReferenciaY.XData = t;
            app.LinhaOrbitaReferenciaY.YData = referencias(:, 2);
            app.LinhaOrbitaReferenciaZ.XData = t;
            app.LinhaOrbitaReferenciaZ.YData = referencias(:, 3);

            % Uma nova trajetoria/regeracao comeca sem pontos medidos.
            OrbitaGaiola.limparMedicoesGraficoOrbital(app);

            if numel(t) >= 2
                xlim(app.GraficoOrbita, [t(1) t(end)]);
            end

            drawnow limitrate;
        end

        function adicionarMedicaoGraficoOrbital(app, tempo_s, medido_uT)
            if isempty(app.LinhaOrbitaMedidaX) || ...
                    ~isvalid(app.LinhaOrbitaMedidaX) || ...
                    ~isfinite(tempo_s) || numel(medido_uT) ~= 3 || ...
                    ~all(isfinite(medido_uT))
                return;
            end

            addpoints(app.LinhaOrbitaMedidaX, tempo_s, medido_uT(1));
            addpoints(app.LinhaOrbitaMedidaY, tempo_s, medido_uT(2));
            addpoints(app.LinhaOrbitaMedidaZ, tempo_s, medido_uT(3));

            drawnow limitrate nocallbacks;
        end

        % ============================================================
        % DISPONIBILIDADE DOS CONTROLES ORBITAIS
        % ============================================================

        function atualizarDisponibilidade(app)
            if isempty(app.BotaoConfirmarTLE) || ~isvalid(app.BotaoConfirmarTLE)
                return;
            end

            conectado = app.Conectado;
            ativo = app.EstadoSistema == "SIMULATION" || ...
                app.EstadoSistema == "SIMULATION_PAUSED" || ...
                app.InicioSimulacaoPendente;
            bloqueado = app.DownloadAtivo || app.OperacaoOrbitalEmAndamento || ...
                app.EtapaCalibracao ~= "IDLE" || ...
                app.EstadoSistema == "CONTROL" || ...
                app.EstadoSistema == "RUNNING";
            emFault = app.EstadoSistema == "FAULT";

            InterfaceGaiola.definirEnable(app, app.CampoTLE1, ~ativo && ~bloqueado);
            InterfaceGaiola.definirEnable(app, app.CampoTLE2, ~ativo && ~bloqueado);
            InterfaceGaiola.definirEnable(app, app.BotaoConfirmarTLE, ~ativo && ~bloqueado);

            InterfaceGaiola.definirEnable(app, app.BotaoGerarSimulacao, ...
                app.TLEConfirmado && ~ativo && ~bloqueado);

            podeConfigurar = app.SimulacaoGerada && ~ativo && ~bloqueado;
            InterfaceGaiola.definirEnable(app, app.ListaMapeamentoX, podeConfigurar);
            InterfaceGaiola.definirEnable(app, app.ListaMapeamentoY, podeConfigurar);
            InterfaceGaiola.definirEnable(app, app.ListaMapeamentoZ, podeConfigurar);
            InterfaceGaiola.definirEnable(app, app.CampoAceleracaoOrbita, podeConfigurar);

            [limitesValidos, ~] = OrbitaGaiola.validarLimitesAtuais(app);
            [mapaValido, ~] = OrbitaGaiola.validarMapeamento(app);

            podeIniciar = conectado && app.SimulacaoGerada && ...
                ~ativo && ~bloqueado && ~emFault && ...
                mapaValido && limitesValidos && ...
                CalibracaoGaiola.calibracaoLocalCompleta(app) && ...
                app.EstadoCalibracaoControlador == "CARREGADA";

            InterfaceGaiola.definirEnable(app, app.BotaoIniciarSimulacao, podeIniciar);

            pausavel = conectado && ...
                (app.EstadoSistema == "SIMULATION" || ...
                 app.EstadoSistema == "SIMULATION_PAUSED") && ...
                ~app.DownloadAtivo;
            InterfaceGaiola.definirEnable(app, app.BotaoPausarSimulacao, pausavel);
            InterfaceGaiola.definirEnable(app, app.BotaoFinalizarSimulacao, pausavel);

            if app.EstadoSistema == "SIMULATION_PAUSED"
                app.BotaoPausarSimulacao.Text = 'Retomar';
            else
                app.BotaoPausarSimulacao.Text = 'Pausar';
            end

            % O download fica disponivel sempre que o Teensy estiver
            % conectado e fora de uma operacao ativa. Isso permite recuperar
            % uma simulacao que ja terminou mesmo se o MATLAB foi fechado,
            % reconectado ou nao recebeu SIM_DOWNLOAD_READY.
            podeBaixar = conectado && ~ativo && ~bloqueado;
            InterfaceGaiola.definirEnable(app, app.BotaoBaixarSimulacao, podeBaixar);
        end
    end
end
