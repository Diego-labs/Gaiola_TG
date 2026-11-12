classdef InterfaceGaiola
    % ============================================================
    % INTERFACEGAIOLA
    % ============================================================
    % Criação e atualização da interface gráfica.
    % ============================================================

    methods (Static)
        function criarInterface(app)
            app.JanelaPrincipal = uifigure( ...
                'Name', 'Gaiola de Helmholtz - Sistema de Controle', ...
                'Position', [100 100 1400 850], ...
                'CloseRequestFcn', @(~, ~) InterfaceGaiola.fecharAplicativo(app));

            app.JanelaPrincipal.UserData = app;

            gradeJanela = uigridlayout(app.JanelaPrincipal, [1 1]);
            gradeJanela.Padding = [8 8 8 8];

            app.GrupoAbas = uitabgroup(gradeJanela);
            app.GrupoAbas.Layout.Row = 1;
            app.GrupoAbas.Layout.Column = 1;

            app.AbaControle = uitab(app.GrupoAbas, 'Title', 'Controle em tempo real');
            app.AbaOrbita = uitab(app.GrupoAbas, 'Title', 'Simulação orbital');

            InterfaceGaiola.criarAbaControle(app);
            InterfaceGaiola.criarAbaOrbita(app);

            movegui(app.JanelaPrincipal, 'center');
        end

        % ============================================================
        % ABA - CONTROLE EM TEMPO REAL
        % ============================================================

        function criarAbaControle(app)
            app.GradePrincipal = uigridlayout(app.AbaControle, [4 1]);
            app.GradePrincipal.RowHeight = {70, 245, '1x', 90};
            app.GradePrincipal.ColumnWidth = {'1x'};
            app.GradePrincipal.Padding = [10 10 10 10];
            app.GradePrincipal.RowSpacing = 8;

            InterfaceGaiola.criarPainelStatus(app);
            InterfaceGaiola.criarPainelReferencias(app);
            InterfaceGaiola.criarPainelGraficos(app);
            InterfaceGaiola.criarPainelComandos(app);
        end

        % ============================================================
        % ABA - SIMULACAO ORBITAL
        % ============================================================

        function criarAbaOrbita(app)
            gradePrincipal = uigridlayout(app.AbaOrbita, [4 1]);
            gradePrincipal.RowHeight = {145, 190, '1x', 95};
            gradePrincipal.ColumnWidth = {'1x'};
            gradePrincipal.Padding = [10 10 10 10];
            gradePrincipal.RowSpacing = 8;

            % --------------------------------------------------------
            % TLE
            % --------------------------------------------------------

            painelTLE = uipanel(gradePrincipal, 'Title', 'TLE');
            painelTLE.Layout.Row = 1;

            gradeTLE = uigridlayout(painelTLE, [3 4]);
            gradeTLE.RowHeight = {30, 30, 32};
            gradeTLE.ColumnWidth = {55, '1x', 135, 190};
            gradeTLE.Padding = [8 6 8 6];
            gradeTLE.RowSpacing = 5;
            gradeTLE.ColumnSpacing = 8;

            rotuloLinha1 = uilabel(gradeTLE, 'Text', 'Linha 1:');
            rotuloLinha1.Layout.Row = 1;
            rotuloLinha1.Layout.Column = 1;

            app.CampoTLE1 = uieditfield( ...
                gradeTLE, 'text', ...
                'ValueChangedFcn', @(~, ~) OrbitaGaiola.tleEditado(app));
            app.CampoTLE1.Value = ...
                '1 40296U 14069A   26269.87826130  .00000263  00000-0  00000-0 0  9992';
            app.CampoTLE1.Layout.Row = 1;
            app.CampoTLE1.Layout.Column = [2 4];

            rotuloLinha2 = uilabel(gradeTLE, 'Text', 'Linha 2:');
            rotuloLinha2.Layout.Row = 2;
            rotuloLinha2.Layout.Column = 1;

            app.CampoTLE2 = uieditfield( ...
                gradeTLE, 'text', ...
                'ValueChangedFcn', @(~, ~) OrbitaGaiola.tleEditado(app));
            app.CampoTLE2.Value = ...
                '2 40296  63.4472 205.4519 6613019 269.8631  20.2660  2.00606495 87278';
            app.CampoTLE2.Layout.Row = 2;
            app.CampoTLE2.Layout.Column = [2 4];

            app.BotaoConfirmarTLE = uibutton( ...
                gradeTLE, ...
                'Text', 'Confirmar TLE', ...
                'ButtonPushedFcn', @(~, ~) OrbitaGaiola.confirmarTLE(app));
            app.BotaoConfirmarTLE.Layout.Row = 3;
            app.BotaoConfirmarTLE.Layout.Column = 3;

            app.TextoEstadoTLE = uilabel( ...
                gradeTLE, ...
                'Text', 'TLE: não confirmado', ...
                'HorizontalAlignment', 'left');
            app.TextoEstadoTLE.Layout.Row = 3;
            app.TextoEstadoTLE.Layout.Column = [1 2];

            app.TextoConexaoOrbita = uilabel( ...
                gradeTLE, ...
                'Text', 'Teensy: desconectado', ...
                'HorizontalAlignment', 'right');
            app.TextoConexaoOrbita.Layout.Row = 3;
            app.TextoConexaoOrbita.Layout.Column = 4;

            % --------------------------------------------------------
            % Informacoes da orbita + configuracao do ensaio
            % --------------------------------------------------------

            gradeSuperior = uigridlayout(gradePrincipal, [1 2]);
            gradeSuperior.Layout.Row = 2;
            gradeSuperior.ColumnWidth = {'3x', '2x'};
            gradeSuperior.Padding = [0 0 0 0];
            gradeSuperior.ColumnSpacing = 8;

            painelInfo = uipanel(gradeSuperior, 'Title', 'Informações da órbita');
            painelInfo.Layout.Column = 1;

            gradeInfo = uigridlayout(painelInfo, [5 4]);
            gradeInfo.RowHeight = {25, 25, 25, 25, 25};
            gradeInfo.ColumnWidth = {130, '1x', 145, '1x'};
            gradeInfo.Padding = [8 6 8 6];
            gradeInfo.RowSpacing = 3;
            gradeInfo.ColumnSpacing = 8;

            [~, app.TextoEpocaOrbita] = InterfaceGaiola.criarParInfo( ...
                gradeInfo, 1, 1, 'Época:', '--');
            [~, app.TextoPeriodoOrbita] = InterfaceGaiola.criarParInfo( ...
                gradeInfo, 1, 3, 'Período orbital:', '--');
            [~, app.TextoInclinacaoOrbita] = InterfaceGaiola.criarParInfo( ...
                gradeInfo, 2, 1, 'Inclinação:', '--');
            [~, app.TextoExcentricidadeOrbita] = InterfaceGaiola.criarParInfo( ...
                gradeInfo, 2, 3, 'Excentricidade:', '--');
            [~, app.TextoARNAOrbita] = InterfaceGaiola.criarParInfo( ...
                gradeInfo, 3, 1, 'ARNA:', '--');
            [~, app.TextoArgumentoPeriastroOrbita] = InterfaceGaiola.criarParInfo( ...
                gradeInfo, 3, 3, 'Arg. periastro:', '--');
            [~, app.TextoAnomaliaVerdadeiraOrbita] = InterfaceGaiola.criarParInfo( ...
                gradeInfo, 4, 1, 'Anomalia verdadeira:', '--');
            [~, app.TextoSemiEixoMaiorOrbita] = InterfaceGaiola.criarParInfo( ...
                gradeInfo, 4, 3, 'Semi-eixo maior:', '--');
            [~, app.TextoQuantidadePontosOrbita] = InterfaceGaiola.criarParInfo( ...
                gradeInfo, 5, 1, 'Pontos:', '--');
            [~, app.TextoPassoOrbita] = InterfaceGaiola.criarParInfo( ...
                gradeInfo, 5, 3, 'Passo mínimo:', '--');

            painelConfig = uipanel(gradeSuperior, 'Title', 'Configuração da simulação');
            painelConfig.Layout.Column = 2;

            gradeConfig = uigridlayout(painelConfig, [6 4]);
            gradeConfig.RowHeight = {25, 25, 25, 28, 25, '1x'};
            gradeConfig.ColumnWidth = {115, '1x', 120, '1x'};
            gradeConfig.Padding = [8 6 8 6];
            gradeConfig.RowSpacing = 4;
            gradeConfig.ColumnSpacing = 7;

            rotuloMapaX = uilabel(gradeConfig, 'Text', 'Eixo X da gaiola:');
            rotuloMapaX.Layout.Row = 1;
            rotuloMapaX.Layout.Column = 1;
            app.ListaMapeamentoX = uidropdown( ...
                gradeConfig, ...
                'Items', {'North', 'East', 'Down'}, ...
                'Value', 'North', ...
                'ValueChangedFcn', @(~, ~) OrbitaGaiola.mapeamentoAlterado(app));
            app.ListaMapeamentoX.Layout.Row = 1;
            app.ListaMapeamentoX.Layout.Column = 2;

            rotuloMapaY = uilabel(gradeConfig, 'Text', 'Eixo Y da gaiola:');
            rotuloMapaY.Layout.Row = 2;
            rotuloMapaY.Layout.Column = 1;
            app.ListaMapeamentoY = uidropdown( ...
                gradeConfig, ...
                'Items', {'North', 'East', 'Down'}, ...
                'Value', 'East', ...
                'ValueChangedFcn', @(~, ~) OrbitaGaiola.mapeamentoAlterado(app));
            app.ListaMapeamentoY.Layout.Row = 2;
            app.ListaMapeamentoY.Layout.Column = 2;

            rotuloMapaZ = uilabel(gradeConfig, 'Text', 'Eixo Z da gaiola:');
            rotuloMapaZ.Layout.Row = 3;
            rotuloMapaZ.Layout.Column = 1;
            app.ListaMapeamentoZ = uidropdown( ...
                gradeConfig, ...
                'Items', {'North', 'East', 'Down'}, ...
                'Value', 'Down', ...
                'ValueChangedFcn', @(~, ~) OrbitaGaiola.mapeamentoAlterado(app));
            app.ListaMapeamentoZ.Layout.Row = 3;
            app.ListaMapeamentoZ.Layout.Column = 2;

            rotuloAceleracao = uilabel(gradeConfig, 'Text', 'Aceleração:');
            rotuloAceleracao.Layout.Row = 1;
            rotuloAceleracao.Layout.Column = 3;

            app.CampoAceleracaoOrbita = uieditfield( ...
                gradeConfig, ...
                'numeric', ...
                'Limits', [0.001 Inf], ...
                'Value', 1.0, ...
                'ValueDisplayFormat', '%.3f', ...
                'ValueChangedFcn', @(~, ~) OrbitaGaiola.aceleracaoAlterada(app));
            app.CampoAceleracaoOrbita.Layout.Row = 1;
            app.CampoAceleracaoOrbita.Layout.Column = 4;

            app.TextoTaxaReferenciaOrbita = uilabel( ...
                gradeConfig, ...
                'Text', 'Taxa de referência: --', ...
                'HorizontalAlignment', 'left');
            app.TextoTaxaReferenciaOrbita.Layout.Row = 2;
            app.TextoTaxaReferenciaOrbita.Layout.Column = [3 4];

            app.TextoValidacaoOrbita = uilabel( ...
                gradeConfig, ...
                'Text', 'Validação de limites: aguardando simulação', ...
                'HorizontalAlignment', 'left');
            app.TextoValidacaoOrbita.Layout.Row = 3;
            app.TextoValidacaoOrbita.Layout.Column = [3 4];

            textoNota = uilabel( ...
                gradeConfig, ...
                'Text', ['Mapeamento deve preservar a regra da mão direita. ' ...
                         'A simulação é enviada ao Teensy antes do início.'], ...
                'WordWrap', 'on', ...
                'FontSize', 10);
            textoNota.Layout.Row = [4 6];
            textoNota.Layout.Column = [1 4];

            % --------------------------------------------------------
            % Grafico do campo orbital
            % --------------------------------------------------------

            painelGrafico = uipanel(gradePrincipal, ...
                'Title', 'Campo magnético - referência e medição');
            painelGrafico.Layout.Row = 3;

            gradeGrafico = uigridlayout(painelGrafico, [1 1]);
            gradeGrafico.Padding = [8 6 8 6];

            app.GraficoOrbita = uiaxes(gradeGrafico);
            xlabel(app.GraficoOrbita, 'Tempo orbital [s]');
            ylabel(app.GraficoOrbita, 'Campo [µT]');
            grid(app.GraficoOrbita, 'on');
            hold(app.GraficoOrbita, 'on');
            title(app.GraficoOrbita, ...
                'Referências orbitais e campo medido pela gaiola');

            cores = get(groot, 'defaultAxesColorOrder');

            app.LinhaOrbitaReferenciaX = plot(app.GraficoOrbita, NaN, NaN, ...
                'Color', cores(1, :), ...
                'LineStyle', '--', ...
                'LineWidth', 1.2, ...
                'DisplayName', 'X ref');
            app.LinhaOrbitaMedidaX = animatedline(app.GraficoOrbita, ...
                'Color', cores(1, :), ...
                'LineStyle', 'none', ...
                'Marker', '.', ...
                'MarkerSize', 8, ...
                'DisplayName', 'X med');

            app.LinhaOrbitaReferenciaY = plot(app.GraficoOrbita, NaN, NaN, ...
                'Color', cores(2, :), ...
                'LineStyle', '--', ...
                'LineWidth', 1.2, ...
                'DisplayName', 'Y ref');
            app.LinhaOrbitaMedidaY = animatedline(app.GraficoOrbita, ...
                'Color', cores(2, :), ...
                'LineStyle', 'none', ...
                'Marker', '.', ...
                'MarkerSize', 8, ...
                'DisplayName', 'Y med');

            app.LinhaOrbitaReferenciaZ = plot(app.GraficoOrbita, NaN, NaN, ...
                'Color', cores(3, :), ...
                'LineStyle', '--', ...
                'LineWidth', 1.2, ...
                'DisplayName', 'Z ref');
            app.LinhaOrbitaMedidaZ = animatedline(app.GraficoOrbita, ...
                'Color', cores(3, :), ...
                'LineStyle', 'none', ...
                'Marker', '.', ...
                'MarkerSize', 8, ...
                'DisplayName', 'Z med');

            legend(app.GraficoOrbita, 'show', 'Location', 'best');

            % --------------------------------------------------------
            % Comandos da simulacao
            % --------------------------------------------------------

            painelComandos = uipanel(gradePrincipal, 'Title', 'Simulação');
            painelComandos.Layout.Row = 4;

            gradeComandos = uigridlayout(painelComandos, [2 12]);
            gradeComandos.RowHeight = {34, 28};
            gradeComandos.ColumnWidth = { ...
                15, 130, 8, 135, 8, 105, 8, 105, 8, 115, '1x', 15};
            gradeComandos.Padding = [8 4 8 4];
            gradeComandos.ColumnSpacing = 0;

            app.BotaoGerarSimulacao = uibutton( ...
                gradeComandos, ...
                'Text', 'Gerar simulação', ...
                'ButtonPushedFcn', @(~, ~) OrbitaGaiola.gerarSimulacao(app));
            app.BotaoGerarSimulacao.Layout.Row = 1;
            app.BotaoGerarSimulacao.Layout.Column = 2;

            app.BotaoIniciarSimulacao = uibutton( ...
                gradeComandos, ...
                'Text', 'Iniciar simulação', ...
                'ButtonPushedFcn', @(~, ~) OrbitaGaiola.iniciarSimulacao(app));
            app.BotaoIniciarSimulacao.Layout.Row = 1;
            app.BotaoIniciarSimulacao.Layout.Column = 4;

            app.BotaoPausarSimulacao = uibutton( ...
                gradeComandos, ...
                'Text', 'Pausar', ...
                'ButtonPushedFcn', @(~, ~) OrbitaGaiola.pausarOuRetomarSimulacao(app));
            app.BotaoPausarSimulacao.Layout.Row = 1;
            app.BotaoPausarSimulacao.Layout.Column = 6;

            app.BotaoFinalizarSimulacao = uibutton( ...
                gradeComandos, ...
                'Text', 'Finalizar', ...
                'ButtonPushedFcn', @(~, ~) OrbitaGaiola.finalizarSimulacao(app));
            app.BotaoFinalizarSimulacao.Layout.Row = 1;
            app.BotaoFinalizarSimulacao.Layout.Column = 8;

            app.BotaoBaixarSimulacao = uibutton( ...
                gradeComandos, ...
                'Text', 'Baixar dados', ...
                'ButtonPushedFcn', @(~, ~) DownloadGaiola.baixarDadosSimulacao(app));
            app.BotaoBaixarSimulacao.Layout.Row = 1;
            app.BotaoBaixarSimulacao.Layout.Column = 10;

            app.TextoEstadoSimulacao = uilabel( ...
                gradeComandos, ...
                'Text', 'Estado: aguardando TLE', ...
                'HorizontalAlignment', 'left');
            app.TextoEstadoSimulacao.Layout.Row = 2;
            app.TextoEstadoSimulacao.Layout.Column = [1 3];

            app.TextoProgressoSimulacao = uilabel( ...
                gradeComandos, ...
                'Text', 'Progresso: 0,0%', ...
                'HorizontalAlignment', 'center');
            app.TextoProgressoSimulacao.Layout.Row = 2;
            app.TextoProgressoSimulacao.Layout.Column = [4 5];

            app.TextoExecucaoSimulacao = uilabel( ...
                gradeComandos, ...
                'Text', 't = -- s | ponto = --', ...
                'HorizontalAlignment', 'center');
            app.TextoExecucaoSimulacao.Layout.Row = 2;
            app.TextoExecucaoSimulacao.Layout.Column = [6 8];

            app.TextoMensagemOrbita = uilabel( ...
                gradeComandos, ...
                'Text', 'Insira ou confirme o TLE.', ...
                'HorizontalAlignment', 'right');
            app.TextoMensagemOrbita.Layout.Row = 2;
            app.TextoMensagemOrbita.Layout.Column = [9 12];
        end

        function [rotulo, valor] = criarParInfo(grade, linha, coluna, textoRotulo, textoValor)
            rotulo = uilabel(grade, 'Text', textoRotulo, 'HorizontalAlignment', 'right');
            rotulo.Layout.Row = linha;
            rotulo.Layout.Column = coluna;

            valor = uilabel(grade, 'Text', textoValor, 'FontWeight', 'bold');
            valor.Layout.Row = linha;
            valor.Layout.Column = coluna + 1;
        end

        % ============================================================
        % PAINEL - STATUS DO SISTEMA
        % ============================================================

        function criarPainelStatus(app)
            painel = uipanel(app.GradePrincipal, 'Title', 'Sistema');
            painel.Layout.Row = 1;
            painel.Layout.Column = 1;

            grade = uigridlayout(painel, [1 7]);
            grade.ColumnWidth = {45, 125, 70, 100, '1x', '1x', '1.35x'};
            grade.RowHeight = {'1x'};
            grade.Padding = [8 5 8 5];
            grade.ColumnSpacing = 8;

            textoPorta = uilabel(grade, 'Text', 'Porta:');
            textoPorta.Layout.Column = 1;

            app.ListaPortas = uidropdown(grade);
            app.ListaPortas.Items = {'Nenhuma porta'};
            app.ListaPortas.Layout.Column = 2;

            app.BotaoAtualizarPortas = uibutton( ...
                grade, 'Text', 'Atualizar', ...
                'ButtonPushedFcn', @(~, ~) ComunicacaoGaiola.atualizarPortasSeriais(app));
            app.BotaoAtualizarPortas.Layout.Column = 3;

            app.BotaoConectar = uibutton( ...
                grade, 'Text', 'Conectar', ...
                'ButtonPushedFcn', @(~, ~) ComunicacaoGaiola.conectarOuDesconectar(app));
            app.BotaoConectar.Layout.Column = 4;

            app.TextoTeensy = uilabel(grade, ...
                'Text', 'Teensy: Desconectado', ...
                'HorizontalAlignment', 'center');
            app.TextoTeensy.Layout.Column = 5;

            app.TextoEstado = uilabel(grade, ...
                'Text', 'Estado: IDLE', ...
                'HorizontalAlignment', 'center');
            app.TextoEstado.Layout.Column = 6;

            app.TextoCalibracao = uilabel(grade, ...
                'Text', 'Calibração: desconhecida', ...
                'HorizontalAlignment', 'center');
            app.TextoCalibracao.Layout.Column = 7;
        end

        % ============================================================
        % PAINEL - REFERENCIAS DE CAMPO
        % ============================================================

        function criarPainelReferencias(app)
            painel = uipanel(app.GradePrincipal, 'Title', 'Referências de campo magnético');
            painel.Layout.Row = 2;
            painel.Layout.Column = 1;

            grade = uigridlayout(painel, [1 3]);
            grade.RowHeight = {'1x'};
            grade.ColumnWidth = {'1x', '1x', '1x'};
            grade.Padding = [8 6 8 6];
            grade.ColumnSpacing = 10;

            componentesX = InterfaceGaiola.criarPainelEixo(app, grade, 'X', 1);
            componentesY = InterfaceGaiola.criarPainelEixo(app, grade, 'Y', 2);
            componentesZ = InterfaceGaiola.criarPainelEixo(app, grade, 'Z', 3);

            app.ControleCampoX = componentesX.slider;
            app.ValorCampoX = componentesX.campoNumerico;
            app.TextoFaixaX = componentesX.textoFaixa;
            app.TextoAplicadoX = componentesX.textoAplicado;
            app.TextoMedidoX = componentesX.textoMedido;
            app.TextoCorrenteX = componentesX.textoCorrente;
            app.TextoPWMX = componentesX.textoPWM;

            app.ControleCampoY = componentesY.slider;
            app.ValorCampoY = componentesY.campoNumerico;
            app.TextoFaixaY = componentesY.textoFaixa;
            app.TextoAplicadoY = componentesY.textoAplicado;
            app.TextoMedidoY = componentesY.textoMedido;
            app.TextoCorrenteY = componentesY.textoCorrente;
            app.TextoPWMY = componentesY.textoPWM;

            app.ControleCampoZ = componentesZ.slider;
            app.ValorCampoZ = componentesZ.campoNumerico;
            app.TextoFaixaZ = componentesZ.textoFaixa;
            app.TextoAplicadoZ = componentesZ.textoAplicado;
            app.TextoMedidoZ = componentesZ.textoMedido;
            app.TextoCorrenteZ = componentesZ.textoCorrente;
            app.TextoPWMZ = componentesZ.textoPWM;
        end

        function componentes = criarPainelEixo(app, gradePai, eixo, coluna)
            painel = uipanel(gradePai, 'Title', ['Eixo ' eixo]);
            painel.Layout.Row = 1;
            painel.Layout.Column = coluna;

            grade = uigridlayout(painel, [4 1]);
            grade.RowHeight = {22, 42, 20, '1x'};
            grade.ColumnWidth = {'1x'};
            grade.Padding = [8 5 8 5];
            grade.RowSpacing = 3;

            textoReferencia = uilabel(grade, ...
                'Text', 'Referência desejada', ...
                'HorizontalAlignment', 'center', ...
                'FontWeight', 'bold');
            textoReferencia.Layout.Row = 1;

            slider = uislider(grade, 'Limits', [-1 1], 'Value', 0);
            slider.Layout.Row = 2;
            slider.ValueChangingFcn = ...
                @(~, evento) InterfaceGaiola.sliderAlterando(app, eixo, evento.Value);
            slider.ValueChangedFcn = ...
                @(~, ~) InterfaceGaiola.sliderConfirmado(app, eixo);

            textoFaixa = uilabel(grade, ...
                'Text', 'Faixa: indisponível', ...
                'HorizontalAlignment', 'center');
            textoFaixa.Layout.Row = 3;

            gradeInferior = uigridlayout(grade, [1 2]);
            gradeInferior.Layout.Row = 4;
            gradeInferior.ColumnWidth = {110, '1x'};
            gradeInferior.Padding = [0 0 0 0];
            gradeInferior.ColumnSpacing = 10;

            gradeValor = uigridlayout(gradeInferior, [2 1]);
            gradeValor.Layout.Column = 1;
            gradeValor.RowHeight = {20, 28};
            gradeValor.Padding = [0 1 0 0];
            gradeValor.RowSpacing = 2;

            textoValor = uilabel(gradeValor, ...
                'Text', 'Valor desejado', ...
                'HorizontalAlignment', 'left', ...
                'FontSize', 10);
            textoValor.Layout.Row = 1;

            gradeCampoNumerico = uigridlayout(gradeValor, [1 3]);
            gradeCampoNumerico.Layout.Row = 2;
            gradeCampoNumerico.ColumnWidth = {70, 24, '1x'};
            gradeCampoNumerico.Padding = [0 0 0 0];
            gradeCampoNumerico.ColumnSpacing = 3;

            campoNumerico = uieditfield(gradeCampoNumerico, ...
                'numeric', ...
                'Limits', [-1 1], ...
                'Value', 0, ...
                'ValueDisplayFormat', '%.2f', ...
                'HorizontalAlignment', 'center', ...
                'ValueChangedFcn', ...
                @(origem, ~) InterfaceGaiola.campoNumericoAlterado(app, eixo, origem.Value));
            campoNumerico.Layout.Column = 1;

            textoUnidade = uilabel(gradeCampoNumerico, ...
                'Text', 'µT', 'HorizontalAlignment', 'left');
            textoUnidade.Layout.Column = 2;

            gradeLeituras = uigridlayout(gradeInferior, [4 2]);
            gradeLeituras.Layout.Column = 2;
            gradeLeituras.RowHeight = {16, 20, 16, 20};
            gradeLeituras.ColumnWidth = {'1x', '1x'};
            gradeLeituras.Padding = [0 0 0 0];
            gradeLeituras.RowSpacing = 0;
            gradeLeituras.ColumnSpacing = 8;

            rotuloAplicado = uilabel(gradeLeituras, ...
                'Text', 'Aplicado', 'HorizontalAlignment', 'center', 'FontSize', 10);
            rotuloAplicado.Layout.Row = 1;
            rotuloAplicado.Layout.Column = 1;

            rotuloMedido = uilabel(gradeLeituras, ...
                'Text', 'Medido', 'HorizontalAlignment', 'center', 'FontSize', 10);
            rotuloMedido.Layout.Row = 1;
            rotuloMedido.Layout.Column = 2;

            textoAplicado = uilabel(gradeLeituras, ...
                'Text', '-- µT', 'HorizontalAlignment', 'center', ...
                'FontWeight', 'bold', 'FontSize', 11);
            textoAplicado.Layout.Row = 2;
            textoAplicado.Layout.Column = 1;

            textoMedido = uilabel(gradeLeituras, ...
                'Text', '-- µT', 'HorizontalAlignment', 'center', ...
                'FontWeight', 'bold', 'FontSize', 11);
            textoMedido.Layout.Row = 2;
            textoMedido.Layout.Column = 2;

            rotuloCorrente = uilabel(gradeLeituras, ...
                'Text', 'I fonte (A)', 'HorizontalAlignment', 'center', 'FontSize', 10);
            rotuloCorrente.Layout.Row = 3;
            rotuloCorrente.Layout.Column = 1;

            rotuloPWM = uilabel(gradeLeituras, ...
                'Text', 'PWM', 'HorizontalAlignment', 'center', 'FontSize', 10);
            rotuloPWM.Layout.Row = 3;
            rotuloPWM.Layout.Column = 2;

            textoCorrente = uilabel(gradeLeituras, ...
                'Text', '-- A', 'HorizontalAlignment', 'center', ...
                'FontWeight', 'bold', 'FontSize', 11);
            textoCorrente.Layout.Row = 4;
            textoCorrente.Layout.Column = 1;

            textoPWM = uilabel(gradeLeituras, ...
                'Text', '--', 'HorizontalAlignment', 'center', ...
                'FontWeight', 'bold', 'FontSize', 11);
            textoPWM.Layout.Row = 4;
            textoPWM.Layout.Column = 2;

            componentes = struct( ...
                'slider', slider, ...
                'campoNumerico', campoNumerico, ...
                'textoFaixa', textoFaixa, ...
                'textoAplicado', textoAplicado, ...
                'textoMedido', textoMedido, ...
                'textoCorrente', textoCorrente, ...
                'textoPWM', textoPWM);
        end

        % ============================================================
        % GRAFICOS DO CONTROLE
        % ============================================================

        function criarPainelGraficos(app)
            painel = uipanel(app.GradePrincipal, ...
                'Title', 'Monitoramento do campo magnético');
            painel.Layout.Row = 3;
            painel.Layout.Column = 1;

            grade = uigridlayout(painel, [1 1]);
            grade.ColumnWidth = {'1x'};
            grade.Padding = [8 6 8 6];

            app.GraficoControle = uiaxes(grade);
            title(app.GraficoControle, ...
                'Referência e campo medido - eixos X, Y e Z');
            xlabel(app.GraficoControle, 'Tempo [s]');
            ylabel(app.GraficoControle, 'Campo [µT]');
            grid(app.GraficoControle, 'on');
            hold(app.GraficoControle, 'on');

            cores = get(groot, 'defaultAxesColorOrder');

            app.LinhaReferenciaX = animatedline(app.GraficoControle, ...
                'Color', cores(1, :), ...
                'LineStyle', '--', ...
                'LineWidth', 1.2, ...
                'DisplayName', 'X ref');
            app.LinhaMedidaX = animatedline(app.GraficoControle, ...
                'Color', cores(1, :), ...
                'LineStyle', 'none', ...
                'Marker', '.', ...
                'MarkerSize', 8, ...
                'DisplayName', 'X med');

            app.LinhaReferenciaY = animatedline(app.GraficoControle, ...
                'Color', cores(2, :), ...
                'LineStyle', '--', ...
                'LineWidth', 1.2, ...
                'DisplayName', 'Y ref');
            app.LinhaMedidaY = animatedline(app.GraficoControle, ...
                'Color', cores(2, :), ...
                'LineStyle', 'none', ...
                'Marker', '.', ...
                'MarkerSize', 8, ...
                'DisplayName', 'Y med');

            app.LinhaReferenciaZ = animatedline(app.GraficoControle, ...
                'Color', cores(3, :), ...
                'LineStyle', '--', ...
                'LineWidth', 1.2, ...
                'DisplayName', 'Z ref');
            app.LinhaMedidaZ = animatedline(app.GraficoControle, ...
                'Color', cores(3, :), ...
                'LineStyle', 'none', ...
                'Marker', '.', ...
                'MarkerSize', 8, ...
                'DisplayName', 'Z med');

            legend(app.GraficoControle, 'show', 'Location', 'best');
            xlim(app.GraficoControle, [0 app.JanelaGrafico_s]);
        end

        % ============================================================
        % PAINEL - COMANDOS DO CONTROLE
        % ============================================================

        function criarPainelComandos(app)
            painel = uipanel(app.GradePrincipal, 'Title', 'Comandos');
            painel.Layout.Row = 4;
            painel.Layout.Column = 1;

            grade = uigridlayout(painel, [2 12]);
            grade.RowHeight = {34, 24};
            grade.ColumnWidth = { ...
                15, 120, 8, 105, 8, 140, 8, 105, 8, 120, '1x', 155};
            grade.Padding = [8 4 8 4];
            grade.ColumnSpacing = 0;

            app.BotaoIniciar = uibutton(grade, ...
                'Text', 'Iniciar controle', ...
                'ButtonPushedFcn', @(~, ~) ControleGaiola.iniciarControle(app));
            app.BotaoIniciar.Layout.Row = 1;
            app.BotaoIniciar.Layout.Column = 2;

            app.BotaoFinalizar = uibutton(grade, ...
                'Text', 'Finalizar', ...
                'ButtonPushedFcn', @(~, ~) ControleGaiola.finalizarControle(app));
            app.BotaoFinalizar.Layout.Row = 1;
            app.BotaoFinalizar.Layout.Column = 4;

            app.BotaoZerarCampo = uibutton(grade, ...
                'Text', 'Zerar campo (0 µT)', ...
                'ButtonPushedFcn', @(~, ~) ControleGaiola.zerarCampo(app));
            app.BotaoZerarCampo.Layout.Row = 1;
            app.BotaoZerarCampo.Layout.Column = 6;

            app.BotaoCalibracao = uibutton(grade, ...
                'Text', 'Calibração', ...
                'ButtonPushedFcn', @(~, ~) CalibracaoGaiola.acaoCalibracao(app));
            app.BotaoCalibracao.Layout.Row = 1;
            app.BotaoCalibracao.Layout.Column = 8;

            app.BotaoBaixarDados = uibutton(grade, ...
                'Text', 'Baixar controle', ...
                'ButtonPushedFcn', @(~, ~) DownloadGaiola.baixarDados(app));
            app.BotaoBaixarDados.Layout.Row = 1;
            app.BotaoBaixarDados.Layout.Column = 10;

            app.BotaoAplicar = uibutton(grade, ...
                'Text', 'Aplicar referências', ...
                'FontWeight', 'bold', ...
                'ButtonPushedFcn', @(~, ~) ControleGaiola.aplicarReferencias(app));
            app.BotaoAplicar.Layout.Row = 1;
            app.BotaoAplicar.Layout.Column = 12;

            app.TextoMensagem = uilabel(grade, ...
                'Text', 'Sistema desconectado', ...
                'HorizontalAlignment', 'center');
            app.TextoMensagem.Layout.Row = 2;
            app.TextoMensagem.Layout.Column = [1 8];

            app.TextoAlteracao = uilabel(grade, ...
                'Text', '', ...
                'HorizontalAlignment', 'right', ...
                'FontSize', 10);
            app.TextoAlteracao.Layout.Row = 2;
            app.TextoAlteracao.Layout.Column = [9 12];
        end

        % ============================================================
        % FECHAMENTO
        % ============================================================

        function fecharAplicativo(app)
            if app.DownloadAtivo || ...
                    app.OperacaoOrbitalEmAndamento
                uialert(app.JanelaPrincipal, ...
                    'Aguarde a operação de transferência em andamento terminar.', ...
                    'Operação em andamento');
                return;
            end

            sistemaAtivo = ...
                app.EstadoSistema == "CONTROL" || ...
                app.EstadoSistema == "SIMULATION" || ...
                app.EstadoSistema == "SIMULATION_PAUSED" || ...
                app.EtapaCalibracao == "EXECUTANDO";

            if sistemaAtivo && app.Conectado
                resposta = uiconfirm( ...
                    app.JanelaPrincipal, ...
                    ['Existe uma operação ativa. Fechar o aplicativo irá solicitar ' ...
                     'a parada segura do sistema. Deseja continuar?'], ...
                    'Fechar aplicativo', ...
                    'Options', {'Fechar', 'Cancelar'}, ...
                    'DefaultOption', 2, ...
                    'CancelOption', 2);

                if ~strcmp(resposta, 'Fechar')
                    return;
                end

                try
                    if app.EstadoSistema == "CONTROL"
                        ComunicacaoGaiola.enviarComando(app, 'CONTROL_STOP');
                    elseif app.EstadoSistema == "SIMULATION" || ...
                            app.EstadoSistema == "SIMULATION_PAUSED"
                        ComunicacaoGaiola.enviarComando(app, 'SIM_ABORT');
                    elseif app.EtapaCalibracao == "EXECUTANDO"
                        ComunicacaoGaiola.enviarComando(app, 'CAL_ABORT');
                    end
                    pause(0.10);
                catch
                end
            end

            ComunicacaoGaiola.encerrarTemporizadorHeartbeat(app);
            ComunicacaoGaiola.desconectarSerial(app, false);

            if ~isempty(app.JanelaPrincipal) && isvalid(app.JanelaPrincipal)
                delete(app.JanelaPrincipal);
            end
        end

        % ============================================================
        % DISPONIBILIDADE DOS CONTROLES
        % ============================================================

        function atualizarDisponibilidade(app)
            conectado = app.Conectado;
            emControle = app.EstadoSistema == "CONTROL";
            emSimulacao = app.EstadoSistema == "SIMULATION" || ...
                app.EstadoSistema == "SIMULATION_PAUSED" || ...
                app.InicioSimulacaoPendente;
            emFault = app.EstadoSistema == "FAULT";
            emRunningLegacy = app.EstadoSistema == "RUNNING";
            emCalibracao = app.EtapaCalibracao ~= "IDLE";
            baixando = app.DownloadAtivo;
            operacaoOrbital = app.OperacaoOrbitalEmAndamento;

            calibracaoLocalCompleta = CalibracaoGaiola.calibracaoLocalCompleta(app);

            % As referencias podem ser alteradas tambem enquanto o
            % controle em tempo real esta executando. O firmware aceita
            % FIELD_REF durante o estado CONTROL e aplica a nova referencia
            % sem reiniciar as malhas. Durante a simulacao orbital, porem,
            % a trajetoria permanece como unica fonte de referencia.
            podeEditarCampo = conectado && calibracaoLocalCompleta && ...
                app.EstadoCalibracaoControlador == "CARREGADA" && ...
                ~emSimulacao && ~emCalibracao && ~emRunningLegacy && ...
                ~baixando && ~emFault && ~operacaoOrbital;

            InterfaceGaiola.definirEnable(app, app.ControleCampoX, podeEditarCampo);
            InterfaceGaiola.definirEnable(app, app.ValorCampoX, podeEditarCampo);
            InterfaceGaiola.definirEnable(app, app.ControleCampoY, podeEditarCampo);
            InterfaceGaiola.definirEnable(app, app.ValorCampoY, podeEditarCampo);
            InterfaceGaiola.definirEnable(app, app.ControleCampoZ, podeEditarCampo);
            InterfaceGaiola.definirEnable(app, app.ValorCampoZ, podeEditarCampo);
            InterfaceGaiola.definirEnable(app, app.BotaoAplicar, podeEditarCampo);

            podeIniciar = conectado && ~emControle && ~emSimulacao && ...
                ~emFault && ~emRunningLegacy && ~emCalibracao && ~baixando && ...
                ~operacaoOrbital && calibracaoLocalCompleta && ...
                app.EstadoCalibracaoControlador == "CARREGADA" && ...
                app.ReferenciaFoiAplicada && ~app.AlteracaoPendente;

            InterfaceGaiola.definirEnable(app, app.BotaoIniciar, podeIniciar);
            InterfaceGaiola.definirEnable(app, app.BotaoFinalizar, ...
                conectado && emControle && ~baixando);
            InterfaceGaiola.definirEnable(app, app.BotaoZerarCampo, ...
                conectado && emControle && ~baixando);

            podeCalibrar = conectado && ~emControle && ~emSimulacao && ...
                ~emRunningLegacy && ~baixando && ~emFault && ~operacaoOrbital;

            if app.EtapaCalibracao == "EXECUTANDO"
                podeCalibrar = true;
            elseif app.EtapaCalibracao ~= "IDLE"
                podeCalibrar = false;
            end

            InterfaceGaiola.definirEnable(app, app.BotaoCalibracao, podeCalibrar);

            % O download permanece disponivel sempre que o sistema esta
            % ocioso. Assim ainda e possivel recuperar arquivos existentes no
            % SD depois de reconectar o MATLAB, mesmo sem uma flag local de
            % CONTROL_DOWNLOAD_READY da sessao anterior.
            podeBaixarControle = conectado && ~emControle && ~emSimulacao && ...
                ~emRunningLegacy && ~emCalibracao && ~baixando && ~operacaoOrbital;
            InterfaceGaiola.definirEnable(app, app.BotaoBaixarDados, podeBaixarControle);

            InterfaceGaiola.definirEnable(app, app.ListaPortas, ~conectado);
            InterfaceGaiola.definirEnable(app, app.BotaoAtualizarPortas, ~conectado);

            podeConectarOuDesconectar = ~baixando && ...
                ~operacaoOrbital && ...
                ~emControle && ~emSimulacao && ~emRunningLegacy && ~emCalibracao;
            if ~conectado
                podeConectarOuDesconectar = true;
            end
            InterfaceGaiola.definirEnable( ...
                app, app.BotaoConectar, podeConectarOuDesconectar);

            OrbitaGaiola.atualizarDisponibilidade(app);
        end

        function definirEnable(~, componente, habilitar)
            if isempty(componente) || ~isvalid(componente)
                return;
            end

            if habilitar
                componente.Enable = 'on';
            else
                componente.Enable = 'off';
            end
        end

        % ============================================================
        % SLIDERS E CAMPOS NUMERICOS
        % ============================================================

        function sliderAlterando(app, eixo, valor)
            InterfaceGaiola.definirValorEditado(app, eixo, valor, true);
        end

        function sliderConfirmado(app, eixo)
            switch eixo
                case 'X'
                    valor = app.ControleCampoX.Value;
                case 'Y'
                    valor = app.ControleCampoY.Value;
                otherwise
                    valor = app.ControleCampoZ.Value;
            end
            InterfaceGaiola.definirValorEditado(app, eixo, valor, true);
        end

        function campoNumericoAlterado(app, eixo, valor)
            limites = CalibracaoGaiola.obterLimitesCampoEixo(app, eixo);
            valor = min(max(valor, limites(1)), limites(2));
            InterfaceGaiola.definirValorEditado(app, eixo, valor, true);
        end

        function definirValorEditado(app, eixo, valor, marcarPendente)
            indice = UtilGaiola.indiceEixo(app, eixo);
            limites = CalibracaoGaiola.obterLimitesCampoEixo(app, eixo);
            valor = min(max(valor, limites(1)), limites(2));

            app.ReferenciaEditada_uT(indice) = valor;

            switch eixo
                case 'X'
                    app.ControleCampoX.Value = valor;
                    app.ValorCampoX.Value = valor;
                case 'Y'
                    app.ControleCampoY.Value = valor;
                    app.ValorCampoY.Value = valor;
                case 'Z'
                    app.ControleCampoZ.Value = valor;
                    app.ValorCampoZ.Value = valor;
            end

            if marcarPendente
                app.AlteracaoPendente = true;
                app.TextoAlteracao.Text = 'Alterações não aplicadas';
                InterfaceGaiola.atualizarDisponibilidade(app);
            end
        end

        function configurarEscalaSliderCampo(~, slider, limites)
            % Mantem os limites fisicos exatos, mas apresenta marcas limpas
            % na interface. Assim a visualizacao nao fica poluida por varios
            % algarismos decimais vindos da regressao da calibracao.
            if isempty(slider) || ~isvalid(slider) || ...
                    numel(limites) ~= 2 || any(~isfinite(limites)) || ...
                    limites(2) <= limites(1)
                return;
            end

            minimo = limites(1);
            maximo = limites(2);
            amplitude = maximo - minimo;

            if minimo < 0 && maximo > 0
                marcas = [minimo, minimo/2, 0, maximo/2, maximo];
            else
                marcas = linspace(minimo, maximo, 5);
            end

            % Remove eventuais repeticoes numericas em faixas muito estreitas.
            marcas = unique(marcas, 'stable');

            if amplitude >= 20
                formato = '%.1f';
            elseif amplitude >= 2
                formato = '%.2f';
            else
                formato = '%.3f';
            end

            rotulos = arrayfun(@(valor) sprintf(formato, valor), ...
                marcas, 'UniformOutput', false);

            slider.MajorTicks = marcas;
            slider.MajorTickLabels = rotulos;
            slider.MinorTicks = [];
        end

        function atualizarTextosAplicados(app)
            r = app.ReferenciaAplicada_uT;
            if isfinite(r(1)), app.TextoAplicadoX.Text = sprintf('%.3f µT', r(1)); end
            if isfinite(r(2)), app.TextoAplicadoY.Text = sprintf('%.3f µT', r(2)); end
            if isfinite(r(3)), app.TextoAplicadoZ.Text = sprintf('%.3f µT', r(3)); end
        end

        % ============================================================
        % GRAFICOS - CONTROLE EM TEMPO REAL
        % ============================================================

        function iniciarNovoGrafico(app)
            clearpoints(app.LinhaReferenciaX);
            clearpoints(app.LinhaMedidaX);
            clearpoints(app.LinhaReferenciaY);
            clearpoints(app.LinhaMedidaY);
            clearpoints(app.LinhaReferenciaZ);
            clearpoints(app.LinhaMedidaZ);

            xlim(app.GraficoControle, [0 app.JanelaGrafico_s]);

            app.TempoAcumuladoControle_s = 0.0;
            app.CronometroControle = tic;
        end

        function atualizarGraficos(app, referencia_uT, medido_uT, tempo_s)
            if nargin < 4 || ~isfinite(tempo_s)
                if isempty(app.CronometroControle)
                    app.CronometroControle = tic;
                end
                tempo_s = app.TempoAcumuladoControle_s + toc(app.CronometroControle);
            end

            addpoints(app.LinhaReferenciaX, tempo_s, referencia_uT(1));
            addpoints(app.LinhaMedidaX, tempo_s, medido_uT(1));
            addpoints(app.LinhaReferenciaY, tempo_s, referencia_uT(2));
            addpoints(app.LinhaMedidaY, tempo_s, medido_uT(2));
            addpoints(app.LinhaReferenciaZ, tempo_s, referencia_uT(3));
            addpoints(app.LinhaMedidaZ, tempo_s, medido_uT(3));

            if tempo_s > app.JanelaGrafico_s
                limitesX = [tempo_s - app.JanelaGrafico_s, tempo_s];
            else
                limitesX = [0, app.JanelaGrafico_s];
            end

            xlim(app.GraficoControle, limitesX);
            drawnow limitrate nocallbacks;
        end
    end
end
