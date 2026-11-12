classdef CalibracaoGaiola
    % ============================================================
    % Aquisição, processamento e armazenamento da calibração.


    methods (Static)
        function acaoCalibracao(app)
            if app.EtapaCalibracao == "EXECUTANDO"
                resposta = uiconfirm( ...
                    app.JanelaPrincipal, ...
                    'Deseja abortar a calibracao em andamento?', ...
                    'Abortar calibracao', ...
                    'Options', {'Abortar', 'Cancelar'}, ...
                    'DefaultOption', 2, ...
                    'CancelOption', 2);

                if strcmp(resposta, 'Abortar')
                    ComunicacaoGaiola.enviarComando(app, 'CAL_ABORT');
                end

                return;
            end

            if app.EtapaCalibracao ~= "IDLE"
                return;
            end

            if CalibracaoGaiola.calibracaoLocalCompleta(app)
                resposta = uiconfirm( ...
                    app.JanelaPrincipal, ...
                    ['Existe uma calibracao processada armazenada localmente. ' ...
                     'Voce pode executar uma nova calibracao ou reenviar os ' ...
                     'parametros locais ao Teensy.'], ...
                    'Calibração', ...
                    'Options', { ...
                        'Nova calibracao', ...
                        'Enviar calibracao local', ...
                        'Cancelar'}, ...
                    'DefaultOption', 1, ...
                    'CancelOption', 3);

                if strcmp(resposta, 'Enviar calibracao local')
                    CalibracaoGaiola.enviarCalibracaoLocal(app);
                    return;
                end

                if ~strcmp(resposta, 'Nova calibracao')
                    return;
                end
            end

            CalibracaoGaiola.iniciarNovaCalibracao(app);
        end


        % ============================================================
        % NOVA CALIBRACAO
        % ============================================================

        function iniciarNovaCalibracao(app)
            aviso = uiconfirm( ...
                app.JanelaPrincipal, ...
                ['A calibração automática percorre PWM positivo e negativo ' ...
                 'e ajusta diretamente B(PWM). O INA mede somente ' ...
                 'a corrente da fonte (nao a corrente da bobina). ' ...
                 'Confirme que a gaiola e a fonte estão preparadas para ' ...
                 'uma varredura bidirecional.'], ...
                'Iniciar calibração', ...
                'Options', {'Continuar', 'Cancelar'}, ...
                'DefaultOption', 2, ...
                'CancelOption', 2);

            if ~strcmp(aviso, 'Continuar')
                return;
            end

            resposta = inputdlg( ...
                { ...
                    'Eixos a calibrar (padrão: XYZ):', ...
                    'Limite soft de corrente da FONTE [A]:', ...
                    'Limite hard de corrente da FONTE [A]:' ...
                }, ...
                'Calibração automática - Teensy 4.1', ...
                [1 50], ...
                {'XYZ', '', ''});

            if isempty(resposta)
                return;
            end

            textoEixos = upper(string(resposta{1}));
            limiteSoft_A = str2double(resposta{2});
            limiteHard_A = str2double(resposta{3});

            mascara = UtilGaiola.converterEixosParaMascara(app, textoEixos);

            if mascara < 1 || mascara > 7
                uialert( ...
                    app.JanelaPrincipal, ...
                    'Informe os eixos usando X, Y, Z ou uma combinacao como XYZ.', ...
                    'Calibração');
                return;
            end

            if ~isfinite(limiteSoft_A) || limiteSoft_A <= 0
                uialert( ...
                    app.JanelaPrincipal, ...
                    'O limite soft deve ser um valor positivo.', ...
                    'Calibração');
                return;
            end

            if ~isfinite(limiteHard_A) || limiteHard_A <= limiteSoft_A
                uialert( ...
                    app.JanelaPrincipal, ...
                    'O limite hard deve ser maior que o limite soft.', ...
                    'Calibração');
                return;
            end

            app.ConfiguracaoCalibracao = struct( ...
                'mascaraEixos', mascara, ...
                'limiteSoft_A', limiteSoft_A, ...
                'limiteHard_A', limiteHard_A);

            app.PontosCalibracaoX = zeros(0, 6);
            app.PontosCalibracaoY = zeros(0, 6);
            app.PontosCalibracaoZ = zeros(0, 6);

            app.LimitesCalibracaoX = CalibracaoGaiola.criarLimitesCalibracaoVazios(app);
            app.LimitesCalibracaoY = CalibracaoGaiola.criarLimitesCalibracaoVazios(app);
            app.LimitesCalibracaoZ = CalibracaoGaiola.criarLimitesCalibracaoVazios(app);

            comando = sprintf( ...
                'CAL_CONFIG;AXIS=%d;SOFT=%.6f;HARD=%.6f', ...
                mascara, limiteSoft_A, limiteHard_A);

            app.EtapaCalibracao = "AGUARDANDO_CONFIG";
            app.TextoMensagem.Text = ...
                'Enviando configuracao de calibracao...';
            InterfaceGaiola.atualizarDisponibilidade(app);

            if ~ComunicacaoGaiola.enviarComando(app, comando)
                app.EtapaCalibracao = "IDLE";
                InterfaceGaiola.atualizarDisponibilidade(app);
            end
        end


        % ============================================================
        % REGISTRO DOS PONTOS DE CALIBRACAO
        % ============================================================

        function registrarPontoCalibracao(app, linha)
            eixo = char(UtilGaiola.obterCampoTexto(app, linha, 'AXIS'));
            indice = UtilGaiola.obterCampoNumero(app, linha, 'INDEX');
            pwm = UtilGaiola.obterCampoNumero(app, linha, 'PWM');
            corrente_A = UtilGaiola.obterCampoNumero(app, linha, 'I');
            bx_uT = UtilGaiola.obterCampoNumero(app, linha, 'BX');
            by_uT = UtilGaiola.obterCampoNumero(app, linha, 'BY');
            bz_uT = UtilGaiola.obterCampoNumero(app, linha, 'BZ');

            valores = [indice pwm corrente_A bx_uT by_uT bz_uT];

            if ~all(isfinite(valores))
                return;
            end

            switch eixo
                case 'X'
                    app.PontosCalibracaoX(end + 1, :) = valores;

                case 'Y'
                    app.PontosCalibracaoY(end + 1, :) = valores;

                case 'Z'
                    app.PontosCalibracaoZ(end + 1, :) = valores;
            end
        end


        % ============================================================
        % LIMITES DE PWM OBTIDOS NA CALIBRACAO
        % ============================================================

        function registrarLimitesCalibracao(app, linha)
            eixo = char(UtilGaiola.obterCampoTexto(app, linha, 'AXIS'));

            limites = struct( ...
                'valido', true, ...
                'pwmSoftPositivo', UtilGaiola.obterCampoNumero(app, linha, 'PWM_SOFT_POS'), ...
                'pwmSoftNegativo', UtilGaiola.obterCampoNumero(app, linha, 'PWM_SOFT_NEG'), ...
                'pwmCalPositivo', UtilGaiola.obterCampoNumero(app, linha, 'PWM_CAL_POS'), ...
                'pwmCalNegativo', UtilGaiola.obterCampoNumero(app, linha, 'PWM_CAL_NEG'));

            campos = [ ...
                limites.pwmSoftPositivo, ...
                limites.pwmSoftNegativo, ...
                limites.pwmCalPositivo, ...
                limites.pwmCalNegativo];

            if ~all(isfinite(campos))
                limites.valido = false;
            end

            switch eixo
                case 'X'
                    app.LimitesCalibracaoX = limites;
                case 'Y'
                    app.LimitesCalibracaoY = limites;
                case 'Z'
                    app.LimitesCalibracaoZ = limites;
            end
        end


        % ============================================================
        % PROCESSAMENTO DA CALIBRACAO NO MATLAB
        % ============================================================

        function processarCalibracao(app)
            try
                mascara = app.ConfiguracaoCalibracao.mascaraEixos;

                novosParametros = CalibracaoGaiola.criarEstruturaCalibracaoVazia(app);
                novosParametros.mascaraEixos = mascara;

                for indice = 1:3
                    bit = 2^(indice - 1);

                    if bitand(mascara, bit) == 0
                        continue;
                    end

                    [eixo, pontos, limites] = ...
                        CalibracaoGaiola.obterDadosCalibracaoEixo(app, indice);

                    if size(pontos, 1) < 3
                        error( ...
                            'Nao ha pontos suficientes para o eixo %s.', ...
                            eixo);
                    end

                    pwm = pontos(:, 2);
                    campo_uT = pontos(:, 3 + indice);

                    positivo = pwm > 0;
                    negativo = pwm < 0;
                    zero = pwm == 0;

                    if nnz(positivo) < 2 || nnz(negativo) < 2
                        error( ...
                            'Pontos positivos/negativos insuficientes no eixo %s.', ...
                            eixo);
                    end

                    if ~any(zero)
                        error(['Calibracao B(PWM) exige pelo menos um ponto ' ...
                               'PWM=0 para o eixo %s.'], eixo);
                    end
                    b0_uT = mean(campo_uT(zero));

                    % Ajuste experimental DIRETO de B(PWM), dois sentidos.
                    % Ambos os ramos passam pelo B0 medido em PWM=0;
                    % portanto nao existe salto artificial na inversao.
                    [bpPosA, qualidadePos] = ...
                        CalibracaoGaiola.ajustarRamoAncorado(pwm(positivo), ...
                            campo_uT(positivo), b0_uT);
                    [bpNegA, qualidadeNeg] = ...
                        CalibracaoGaiola.ajustarRamoAncorado(pwm(negativo), ...
                            campo_uT(negativo), b0_uT);
                    if bpPosA <= 1e-8 || bpNegA <= 1e-8
                        error(['B(PWM) nao apresenta inclinacoes positivas ' ...
                               'nos dois sentidos do eixo %s; verifique o ' ...
                               'sentido dos fios e do magnetometro.'], eixo);
                    end
                    if ~limites.valido
                        error('Limites PWM do eixo %s nao recebidos.', eixo);
                    end

                    % Nao extrapolar alem da faixa REALMENTE varrida.
                    % O limite de corrente INA e apenas da fonte do DRV8871.
                    pwmPosUsavel = min(limites.pwmSoftPositivo, limites.pwmCalPositivo);
                    pwmNegUsavel = max(limites.pwmSoftNegativo, limites.pwmCalNegativo);
                    if pwmPosUsavel <= 0 || pwmNegUsavel >= 0
                        error('Limites PWM insuficientes no eixo %s.', eixo);
                    end
                    if qualidadePos.rmse > 2 || qualidadeNeg.rmse > 2 || ...
                            qualidadePos.r2 < 0.98 || qualidadeNeg.r2 < 0.98
                        warning(['Eixo %s: qualidade insuficiente no ' ...
                                 'ajuste B(PWM), especialmente perto ' ...
                                 'de zero. Verifique os pontos antes ' ...
                                 'de aplicar a malha fechada.'], eixo);
                    end
                    parametro = CalibracaoGaiola.criarParametroEixoVazio(app);
                    parametro.valido = true;
                    parametro.soft_A = app.ConfiguracaoCalibracao.limiteSoft_A;
                    parametro.hard_A = app.ConfiguracaoCalibracao.limiteHard_A;
                    parametro.pwmSoftPositivo = limites.pwmSoftPositivo;
                    parametro.pwmSoftNegativo = limites.pwmSoftNegativo;
                    parametro.pwmCalPositivo = limites.pwmCalPositivo;
                    parametro.pwmCalNegativo = limites.pwmCalNegativo;
                    parametro.b0_uT = b0_uT;
                    parametro.bpPosA = bpPosA;
                    parametro.bpNegA = bpNegA;
                    parametro.bpPosR2 = qualidadePos.r2;
                    parametro.bpPosRMSE = qualidadePos.rmse;
                    parametro.bpPosMax = qualidadePos.maxAbsError;
                    parametro.bpNegR2 = qualidadeNeg.r2;
                    parametro.bpNegRMSE = qualidadeNeg.rmse;
                    parametro.bpNegMax = qualidadeNeg.maxAbsError;

                    % Campos armazenados refletem a faixa de referencias,
                    % nao os extremos nominais usados no ajuste linear.
                    campoPermitido = CalibracaoGaiola.calcularLimitesCampoControle(parametro);
                    parametro.campoMin_uT = campoPermitido(1);
                    parametro.campoMax_uT = campoPermitido(2);
                    fprintf(['Calibracao direta B(PWM), eixo %s: B0=%.4f uT; ' ...
                             'k+=%.7g, k-=%.7g uT/PWM; ' ...
                             'RMSE+=%.3f, RMSE-=%.3f uT\n'], ...
                             eixo, b0_uT, bpPosA, bpNegA, ...
                             qualidadePos.rmse, qualidadeNeg.rmse);

                    novosParametros.eixos(indice) = parametro;
                end

                parametrosMesclados = app.ParametrosCalibracaoLocal;

                for indice = 1:3
                    bit = 2^(indice - 1);

                    if bitand(mascara, bit) ~= 0
                        parametrosMesclados.eixos(indice) = ...
                            novosParametros.eixos(indice);
                    end
                end

                parametrosMesclados.mascaraEixos = ...
                    CalibracaoGaiola.calcularMascaraParametrosValidos(app, parametrosMesclados);

                app.ParametrosCalibracaoEnvio = novosParametros;
                app.ParametrosCalibracaoPendentes = parametrosMesclados;

                app.EtapaCalibracao = "ENVIANDO_PARAMETROS";
                app.TextoMensagem.Text = ...
                    'Enviando parametros processados ao Teensy...';
                InterfaceGaiola.atualizarDisponibilidade(app);

                CalibracaoGaiola.enviarParametrosCalibracao(app,  ...
                    novosParametros, ...
                    mascara);

            catch erro
                app.EtapaCalibracao = "IDLE";
                app.BotaoCalibracao.Text = 'Calibração';
                app.TextoMensagem.Text = ...
                    ['Falha ao processar calibracao: ' erro.message];
                InterfaceGaiola.atualizarDisponibilidade(app);

                uialert( ...
                    app.JanelaPrincipal, ...
                    erro.message, ...
                    'Processamento da calibracao');
            end
        end


        % ============================================================
        % AJUSTE LINEAR E QUALIDADE
        % ============================================================

        function [k, qualidade] = ajustarRamoAncorado(pwm, campo, b0)
            pwm = double(pwm(:));
            campo = double(campo(:));
            if numel(pwm) < 2 || any(~isfinite([pwm; campo])) || ...
                    ~isfinite(b0) || sum(pwm.^2) <= 0
                error('Pontos insuficientes para ajuste direto B(PWM).');
            end

            % Minimos quadrados B = B0 + k*PWM, com B0 medido.
            k = sum(pwm .* (campo - b0)) / sum(pwm.^2);
            residuo = campo - (b0 + k*pwm);
            sse = sum(residuo.^2);
            sst = sum((campo - mean(campo)).^2);
            if sst > eps
                r2 = 1 - sse / sst;
            else
                r2 = double(sse <= eps);
            end
            qualidade = struct('r2', r2, ...
                'rmse', sqrt(mean(residuo.^2)), ...
                'maxAbsError', max(abs(residuo)));
            if ~all(isfinite([k qualidade.r2 qualidade.rmse qualidade.maxAbsError]))
                error('Falha numerica no ajuste B(PWM).');
            end
        end

        % ============================================================
        % ENVIO DOS PARAMETROS DE CALIBRACAO
        % ============================================================

        function enviarParametrosCalibracao(app, parametros, mascara)
            ComunicacaoGaiola.enviarComando(app, sprintf( ...
                'CAL_PARAMS_BEGIN;AXIS_MASK=%d;VERSION=2', ...
                mascara));

            for indice = 1:3
                bit = 2^(indice - 1);

                if bitand(mascara, bit) == 0
                    continue;
                end

                eixo = UtilGaiola.caractereEixo(app, indice);
                p = parametros.eixos(indice);

                comandoLimites = sprintf( ...
                    ['CAL_PARAMS_LIMITS;AXIS=%s;SOFT=%.9g;HARD=%.9g;' ...
                     'PWM_SOFT_POS=%d;PWM_SOFT_NEG=%d;' ...
                     'PWM_CAL_POS=%d;PWM_CAL_NEG=%d'], ...
                    eixo, ...
                    p.soft_A, ...
                    p.hard_A, ...
                    round(p.pwmSoftPositivo), ...
                    round(p.pwmSoftNegativo), ...
                    round(p.pwmCalPositivo), ...
                    round(p.pwmCalNegativo));

                comandoBP = sprintf( ...
                    ['CAL_PARAMS_BP;AXIS=%s;B0=%.9g;' ...
                     'POS_A=%.9g;NEG_A=%.9g;' ...
                     'POS_R2=%.9g;POS_RMSE=%.9g;POS_MAX=%.9g;' ...
                     'NEG_R2=%.9g;NEG_RMSE=%.9g;NEG_MAX=%.9g'], ...
                    eixo, p.b0_uT, p.bpPosA, p.bpNegA, ...
                    p.bpPosR2, p.bpPosRMSE, p.bpPosMax, ...
                    p.bpNegR2, p.bpNegRMSE, p.bpNegMax);
                ComunicacaoGaiola.enviarComando(app, comandoLimites);
                ComunicacaoGaiola.enviarComando(app, comandoBP);
            end

            ComunicacaoGaiola.enviarComando(app, 'CAL_PARAMS_COMMIT');
        end


        % ============================================================
        % REENVIO DA CALIBRACAO LOCAL
        % ============================================================

        function enviarCalibracaoLocal(app)
            mascara = CalibracaoGaiola.calcularMascaraParametrosValidos(app,  ...
                app.ParametrosCalibracaoLocal);

            if mascara == 0
                uialert( ...
                    app.JanelaPrincipal, ...
                    'Nao existe calibracao local valida.', ...
                    'Calibração');
                return;
            end

            if ~CalibracaoGaiola.validarEstruturaCalibracao(app, app.ParametrosCalibracaoLocal)
                uialert(app.JanelaPrincipal, ...
                    'A calibracao antiga nao e compativel. Execute nova calibracao B(PWM).', ...
                    'Calibracao versao 2');
                return;
            end
            app.ParametrosCalibracaoEnvio = app.ParametrosCalibracaoLocal;
            app.ParametrosCalibracaoPendentes = app.ParametrosCalibracaoLocal;
            app.EtapaCalibracao = "ENVIANDO_PARAMETROS";
            app.TextoMensagem.Text = ...
                'Enviando calibracao local ao Teensy...';
            InterfaceGaiola.atualizarDisponibilidade(app);

            CalibracaoGaiola.enviarParametrosCalibracao(app,  ...
                app.ParametrosCalibracaoLocal, ...
                mascara);
        end


        % ============================================================
        % CALIBRACAO SALVA NO TEENSY
        % ============================================================

        function finalizarSalvamentoCalibracao(app)
            app.ParametrosCalibracaoLocal = ...
                app.ParametrosCalibracaoPendentes;

            app.ParametrosCalibracaoLocal.mascaraEixos = ...
                CalibracaoGaiola.calcularMascaraParametrosValidos(app,  ...
                    app.ParametrosCalibracaoLocal);

            CalibracaoGaiola.salvarCalibracaoLocal(app);

            % A resposta CONTROL_LIMITS anterior pertence a outra
            % calibracao. Descarta-a ANTES de atualizar os sliders locais.
            % O Teensy enviara os novos limites apos CAL_PARAMS_SAVED.
            app.LimitesControleTeensy_uT = nan(3, 2);
            CalibracaoGaiola.aplicarLimitesDaCalibracaoLocal(app);
            OrbitaGaiola.validarLimitesAtuais(app);

            % As referencias da calibracao anterior precisam ser
            % explicitamente reaplicadas apos mudar os limites.
            app.ReferenciaAplicada_uT = [NaN NaN NaN];
            app.ReferenciaAguardando_uT = [NaN NaN NaN];
            app.ReferenciaFoiAplicada = false;
            app.AlteracaoPendente = true;
            app.TextoAplicadoX.Text = '-- µT';
            app.TextoAplicadoY.Text = '-- µT';
            app.TextoAplicadoZ.Text = '-- µT';
            app.TextoAlteracao.Text = 'Aplique referências após recalibrar';

            app.EstadoCalibracaoControlador = "CARREGADA";
            app.EtapaCalibracao = "IDLE";
            app.BotaoCalibracao.Text = 'Calibração';

            ComunicacaoGaiola.atualizarTextoCalibracao(app);
            app.TextoMensagem.Text = ...
                'Calibracao processada, enviada e salva.';
            InterfaceGaiola.atualizarDisponibilidade(app);
        end


        % ============================================================
        % CALIBRACAO LOCAL
        % ============================================================

        function definirCaminhoCalibracaoLocal(app)
            pasta = fileparts(mfilename('fullpath'));

            if isempty(pasta)
                pasta = pwd;
            end

            app.CaminhoCalibracaoLocal = string( ...
                fullfile(pasta, 'calibracao_gaiola.mat'));
        end


        function carregarCalibracaoLocal(app)
            caminho = char(app.CaminhoCalibracaoLocal);

            if ~isfile(caminho)
                return;
            end

            try
                dados = load(caminho, 'parametrosCalibracao');

                if isfield(dados, 'parametrosCalibracao') && ...
                        CalibracaoGaiola.validarEstruturaCalibracao(app, dados.parametrosCalibracao)
                    app.ParametrosCalibracaoLocal = ...
                        dados.parametrosCalibracao;
                end
            catch
                app.ParametrosCalibracaoLocal = ...
                    CalibracaoGaiola.criarEstruturaCalibracaoVazia(app);
            end
        end


        function salvarCalibracaoLocal(app)
            parametrosCalibracao = app.ParametrosCalibracaoLocal; 

            try
                save( ...
                    char(app.CaminhoCalibracaoLocal), ...
                    'parametrosCalibracao');
            catch erro
                app.TextoMensagem.Text = ...
                    ['Calibracao salva no Teensy, mas nao foi possivel ' ...
                     'salvar o arquivo local: ' erro.message];
            end
        end


        function aplicarLimitesDaCalibracaoLocal(app)
            if isempty(app.ControleCampoX)
                return;
            end

            for indice = 1:3
                p = app.ParametrosCalibracaoLocal.eixos(indice);
                eixo = UtilGaiola.caractereEixo(app, indice);

                % Se o Teensy ja informou CONTROL_LIMITS, ele e a fonte
                % autoritativa. Sem conexao, usa-se a mesma equacao local.
                limites = CalibracaoGaiola.obterLimitesCampoEixo(app, eixo);

                if ~p.valido || any(~isfinite(limites)) || ...
                        limites(2) <= limites(1)
                    continue;
                end

                switch eixo
                    case 'X'
                        app.ControleCampoX.Limits = limites;
                        app.ValorCampoX.Limits = limites;
                        app.TextoFaixaX.Text = sprintf( ...
                            'Faixa controle: %.2f a %.2f µT', ...
                            limites(1), limites(2));
                        InterfaceGaiola.configurarEscalaSliderCampo( ...
                            app, app.ControleCampoX, limites);

                    case 'Y'
                        app.ControleCampoY.Limits = limites;
                        app.ValorCampoY.Limits = limites;
                        app.TextoFaixaY.Text = sprintf( ...
                            'Faixa controle: %.2f a %.2f µT', ...
                            limites(1), limites(2));
                        InterfaceGaiola.configurarEscalaSliderCampo( ...
                            app, app.ControleCampoY, limites);

                    case 'Z'
                        app.ControleCampoZ.Limits = limites;
                        app.ValorCampoZ.Limits = limites;
                        app.TextoFaixaZ.Text = sprintf( ...
                            'Faixa controle: %.2f a %.2f µT', ...
                            limites(1), limites(2));
                        InterfaceGaiola.configurarEscalaSliderCampo( ...
                            app, app.ControleCampoZ, limites);
                end

                valorAtual = app.ReferenciaEditada_uT(indice);
                valorAtual = min(max(valorAtual, limites(1)), limites(2));
                InterfaceGaiola.definirValorEditado(app, eixo, valorAtual, false);
            end
        end


        function completo = calibracaoLocalCompleta(app)
            completo = true;

            if ~isfield(app.ParametrosCalibracaoLocal, 'eixos') || ...
                    numel(app.ParametrosCalibracaoLocal.eixos) < 3
                completo = false;
                return;
            end

            for indice = 1:3
                p = app.ParametrosCalibracaoLocal.eixos(indice);

                if ~isfield(p, 'valido') || ~p.valido
                    completo = false;
                    return;
                end
            end
        end


        function valido = validarEstruturaCalibracao(~, parametros)
            valido = isstruct(parametros) && ...
                isfield(parametros, 'versao') && ...
                parametros.versao == 2 && ...
                isfield(parametros, 'eixos') && ...
                numel(parametros.eixos) == 3 && ...
                all(isfield(parametros.eixos, ...
                    {'bpPosA', 'bpNegA', 'b0_uT'}));
        end


        % ============================================================
        % DOWNLOAD DOS DADOS
        % ============================================================

        function limites = obterLimitesCampoEixo(app, eixo)
            indice = UtilGaiola.indiceEixo(app, eixo);
            limites = [NaN NaN];

            % Quando conectado, CONTROL_LIMITS vem diretamente do mesmo
            % controlador que valida FIELD_REF. Ele deve ter prioridade
            % para evitar qualquer diferenca entre slider e firmware.
            if size(app.LimitesControleTeensy_uT, 1) >= indice
                remoto = app.LimitesControleTeensy_uT(indice, :);
                if all(isfinite(remoto)) && remoto(2) > remoto(1)
                    limites = remoto;
                    return;
                end
            end

            % Sem limite remoto valido, reproduz localmente a equacao do
            % firmware a partir dos parametros de calibracao armazenados.
            if isfield(app.ParametrosCalibracaoLocal, 'eixos') && ...
                    numel(app.ParametrosCalibracaoLocal.eixos) >= indice
                p = app.ParametrosCalibracaoLocal.eixos(indice);
                if isfield(p, 'valido') && p.valido
                    limites = CalibracaoGaiola.calcularLimitesCampoControle(p);
                end
            end

            if any(~isfinite(limites)) || limites(2) <= limites(1)
                limites = [-1 1];
            end
        end


        function limites = calcularLimitesCampoControle(p)
            limites = [NaN NaN];
            campos = {'b0_uT','bpPosA','bpNegA', ...
                'pwmSoftPositivo','pwmSoftNegativo', ...
                'pwmCalPositivo','pwmCalNegativo'};
            if ~all(isfield(p, campos)), return; end
            valores = [p.b0_uT p.bpPosA p.bpNegA ...
                p.pwmSoftPositivo p.pwmSoftNegativo ...
                p.pwmCalPositivo p.pwmCalNegativo];
            if any(~isfinite(valores)) || p.bpPosA <= 0 || p.bpNegA <= 0
                return;
            end
            pwmNeg = max(p.pwmSoftNegativo, p.pwmCalNegativo);
            pwmPos = min(p.pwmSoftPositivo, p.pwmCalPositivo);
            if pwmNeg >= 0 || pwmPos <= 0, return; end

            % Mesma margem do ControladorCampo.cpp: erro absoluto maximo
            % experimental de cada ramo + reserva de 1% do PWM varrido.
            reservaPwm = 0.01;
            erroNeg = 0;
            erroPos = 0;
            if isfield(p, 'bpNegMax') && isfinite(p.bpNegMax) && p.bpNegMax >= 0
                erroNeg = p.bpNegMax;
            end
            if isfield(p, 'bpPosMax') && isfinite(p.bpPosMax) && p.bpPosMax >= 0
                erroPos = p.bpPosMax;
            end
            minimo = p.b0_uT + p.bpNegA * pwmNeg * (1 - reservaPwm) + erroNeg;
            maximo = p.b0_uT + p.bpPosA * pwmPos * (1 - reservaPwm) - erroPos;
            
            % Arredondamento para dentro, igual ao firmware.
            limites = [ceil(minimo * 1000) / 1000, ...
                       floor(maximo * 1000) / 1000];
            if ~all(isfinite(limites)) || limites(2) <= limites(1)
                limites = [NaN NaN];
            end
        end

        function [eixo, pontos, limites] = ...
                obterDadosCalibracaoEixo(app, indice)

            eixo = UtilGaiola.caractereEixo(app, indice);

            switch eixo
                case 'X'
                    pontos = app.PontosCalibracaoX;
                    limites = app.LimitesCalibracaoX;

                case 'Y'
                    pontos = app.PontosCalibracaoY;
                    limites = app.LimitesCalibracaoY;

                case 'Z'
                    pontos = app.PontosCalibracaoZ;
                    limites = app.LimitesCalibracaoZ;
            end
        end


        % ============================================================
        % ESTRUTURAS DE CALIBRACAO
        % ============================================================

        function parametros = criarEstruturaCalibracaoVazia(app)
            parametroVazio = CalibracaoGaiola.criarParametroEixoVazio(app);

            parametros = struct( ...
                'versao', 2, ...
                'mascaraEixos', 0, ...
                'eixos', repmat(parametroVazio, 1, 3));
        end


        function parametro = criarParametroEixoVazio(~)
            parametro = struct( ...
                'valido', false, ...
                'soft_A', NaN, ...
                'hard_A', NaN, ...
                'pwmSoftPositivo', NaN, ...
                'pwmSoftNegativo', NaN, ...
                'pwmCalPositivo', NaN, ...
                'pwmCalNegativo', NaN, ...
                'bpPosA', NaN, ...
                'bpNegA', NaN, ...
                'bpPosR2', NaN, ...
                'bpPosRMSE', NaN, ...
                'bpPosMax', NaN, ...
                'bpNegR2', NaN, ...
                'bpNegRMSE', NaN, ...
                'bpNegMax', NaN, ...
                'b0_uT', NaN, ...
                'campoMin_uT', NaN, ...
                'campoMax_uT', NaN);
        end


        function limites = criarLimitesCalibracaoVazios(~)
            limites = struct( ...
                'valido', false, ...
                'pwmSoftPositivo', NaN, ...
                'pwmSoftNegativo', NaN, ...
                'pwmCalPositivo', NaN, ...
                'pwmCalNegativo', NaN);
        end


        function mascara = calcularMascaraParametrosValidos(~, parametros)
            mascara = 0;

            if ~isfield(parametros, 'eixos')
                return;
            end

            for indice = 1:min(3, numel(parametros.eixos))
                if parametros.eixos(indice).valido
                    mascara = bitor(mascara, 2^(indice - 1));
                end
            end
        end
    end
end
