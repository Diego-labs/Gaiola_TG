classdef ControleGaiola
    % ============================================================
    % CONTROLEGAIOLA
    % ============================================================
    % Comandos e supervisao do controle direto de campo (PI) manual.
    % ============================================================

    methods (Static)
        function aplicarReferencias(app)
            if ~app.Conectado
                return;
            end

            if ~CalibracaoGaiola.calibracaoLocalCompleta(app)
                uialert(app.JanelaPrincipal, ...
                    'Nao ha calibracao local valida para os tres eixos.', ...
                    'Referencia de campo');
                return;
            end

            referencia = app.ReferenciaEditada_uT;

            if ~all(isfinite(referencia))
                uialert(app.JanelaPrincipal, ...
                    'As três referências de campo devem ser valores finitos.', ...
                    'Referência de campo');
                return;
            end

            % Marca a referencia pendente ANTES de escrever na serial. Isso
            % elimina uma condição de corrida: o Teensy pode responder
            % FIELD_REF_OK muito rapidamente e o callback precisa saber qual
            % conjunto de valores estava aguardando confirmacao.
            app.ReferenciaAguardando_uT = referencia;

            comando = sprintf( ...
                'FIELD_REF;BX=%.6f;BY=%.6f;BZ=%.6f', ...
                referencia(1), referencia(2), referencia(3));

            if ComunicacaoGaiola.enviarComando(app, comando)
                if app.EstadoSistema == "CONTROL"
                    app.TextoMensagem.Text = ...
                        'Atualizando referências durante o controle...';
                else
                    app.TextoMensagem.Text = 'Enviando referências ao Teensy...';
                end
            else
                app.ReferenciaAguardando_uT = [NaN NaN NaN];
            end
        end

        function zerarCampo(app)
            if app.EstadoSistema ~= "CONTROL"
                return;
            end

            InterfaceGaiola.definirValorEditado(app, 'X', 0.0, false);
            InterfaceGaiola.definirValorEditado(app, 'Y', 0.0, false);
            InterfaceGaiola.definirValorEditado(app, 'Z', 0.0, false);

            app.AlteracaoPendente = true;
            app.TextoAlteracao.Text = 'Aplicando campo nulo...';
            ControleGaiola.aplicarReferencias(app);
        end

        function iniciarControle(app)
            if ~app.ReferenciaFoiAplicada
                uialert(app.JanelaPrincipal, ...
                    'Aplique uma referência de campo antes de iniciar o controle.', ...
                    'Controle');
                return;
            end

            if app.AlteracaoPendente
                uialert(app.JanelaPrincipal, ...
                    'Existem alterações de referência ainda não aplicadas.', ...
                    'Controle');
                return;
            end

            if app.DadosControleDisponiveis && ~app.DadosControleBaixados
                resposta = uiconfirm(app.JanelaPrincipal, ...
                    ['Existem dados de um controle anterior no Teensy que ainda ' ...
                     'não foram baixados. Um novo controle substituirá esses arquivos.'], ...
                    'Dados pendentes', ...
                    'Options', {'Continuar', 'Cancelar'}, ...
                    'DefaultOption', 2, ...
                    'CancelOption', 2);

                if ~strcmp(resposta, 'Continuar')
                    return;
                end
            end

            app.TextoMensagem.Text = 'Solicitando início do controle...';
            ComunicacaoGaiola.enviarComando(app, 'CONTROL_START');
        end

        function finalizarControle(app)
            if app.EstadoSistema ~= "CONTROL"
                return;
            end

            app.TextoMensagem.Text = 'Finalizando controle...';
            ComunicacaoGaiola.enviarComando(app, 'CONTROL_STOP');
        end

        function processarControlStatus(app, linha)
            estado = UtilGaiola.obterCampoTexto(app, linha, 'STATE');
            if strlength(estado) > 0
                ComunicacaoGaiola.definirEstadoSistema(app, estado);
            end

            bxRef = UtilGaiola.obterCampoNumero(app, linha, 'X_BREF');
            byRef = UtilGaiola.obterCampoNumero(app, linha, 'Y_BREF');
            bzRef = UtilGaiola.obterCampoNumero(app, linha, 'Z_BREF');

            bx = UtilGaiola.obterCampoNumero(app, linha, 'X_B');
            by = UtilGaiola.obterCampoNumero(app, linha, 'Y_B');
            bz = UtilGaiola.obterCampoNumero(app, linha, 'Z_B');

            ix = UtilGaiola.obterCampoNumero(app, linha, 'X_I');
            iy = UtilGaiola.obterCampoNumero(app, linha, 'Y_I');
            iz = UtilGaiola.obterCampoNumero(app, linha, 'Z_I');

            pwmX = UtilGaiola.obterCampoNumero(app, linha, 'X_PWM');
            pwmY = UtilGaiola.obterCampoNumero(app, linha, 'Y_PWM');
            pwmZ = UtilGaiola.obterCampoNumero(app, linha, 'Z_PWM');

            referencias = [bxRef byRef bzRef];

            if all(isfinite(referencias))
                app.ReferenciaAplicada_uT = referencias;
                app.ReferenciaFoiAplicada = true;
                InterfaceGaiola.atualizarTextosAplicados(app);
            end

            if isfinite(bx), app.TextoMedidoX.Text = sprintf('%.3f µT', bx); end
            if isfinite(by), app.TextoMedidoY.Text = sprintf('%.3f µT', by); end
            if isfinite(bz), app.TextoMedidoZ.Text = sprintf('%.3f µT', bz); end

            if isfinite(ix), app.TextoCorrenteX.Text = sprintf('%.4f A', ix); end
            if isfinite(iy), app.TextoCorrenteY.Text = sprintf('%.4f A', iy); end
            if isfinite(iz), app.TextoCorrenteZ.Text = sprintf('%.4f A', iz); end

            if isfinite(pwmX), app.TextoPWMX.Text = sprintf('%d', round(pwmX)); end
            if isfinite(pwmY), app.TextoPWMY.Text = sprintf('%d', round(pwmY)); end
            if isfinite(pwmZ), app.TextoPWMZ.Text = sprintf('%d', round(pwmZ)); end

            tempo_s = UtilGaiola.obterCampoNumero(app, linha, 'T');

            if app.EstadoSistema == "CONTROL" && ...
                    all(isfinite([bxRef byRef bzRef bx by bz]))
                InterfaceGaiola.atualizarGraficos(app, ...
                    [bxRef byRef bzRef], ...
                    [bx by bz], ...
                    tempo_s);
            end

            InterfaceGaiola.atualizarDisponibilidade(app);
        end
    end
end
