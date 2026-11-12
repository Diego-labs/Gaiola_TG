classdef DownloadGaiola
    % ============================================================
    % DOWNLOADGAIOLA
    % ============================================================
    %
    % Recepcao dos arquivos do cartao SD com verificacao CRC32.
    %
    % Firmware V8:
    %   CONTROL_DOWNLOAD -> control_info.txt, control_events.txt, control.bin
    %   SIM_DOWNLOAD     -> trajectory.bin, trajectory.csv,
    %                       trajectory_info.txt, simulation.bin,
    %                       simulation_info.txt, simulation_events.txt
    % ============================================================

    methods (Static)
        % ============================================================
        % CONTROLE EM TEMPO REAL
        % ============================================================

        function baixarDados(app)
            if ~app.Conectado || app.DownloadAtivo
                return;
            end

            pastaInicial = fullfile( ...
                fileparts(mfilename('fullpath')), ...
                'Dados_salvos');

            if ~isfolder(pastaInicial)
                mkdir(pastaInicial);
            end

            pasta = uigetdir(pastaInicial, ...
                'Selecione a pasta para salvar os dados do controle');

            if isequal(pasta, 0)
                return;
            end

            sucesso = DownloadGaiola.executarDownload( ...
                app, ...
                'CONTROL_DOWNLOAD', ...
                pasta, ...
                'controle em tempo real', ...
                app.TextoMensagem);

            if sucesso
                app.DadosControleDisponiveis = true;
                app.DadosControleBaixados = true;

                try
                    DownloadGaiola.converterControleBinarioParaCSV(pasta);
                    app.TextoMensagem.Text = ...
                        ['Download concluído. control_dados.csv criado em: ' pasta];
                catch erroConversao
                    app.TextoMensagem.Text = ...
                        ['Download concluído, mas a conversão de control.bin falhou: ' ...
                         erroConversao.message];
                end
            end

            InterfaceGaiola.atualizarDisponibilidade(app);
        end

        % ============================================================
        % SIMULACAO ORBITAL
        % ============================================================

        function baixarDadosSimulacao(app)
            if ~app.Conectado || app.DownloadAtivo
                return;
            end

            if strlength(app.PastaEnsaioAtual) > 0 && ...
                    isfolder(char(app.PastaEnsaioAtual))
                pasta = char(app.PastaEnsaioAtual);
            else
                pastaInicial = fullfile( ...
                    fileparts(mfilename('fullpath')), ...
                    'Dados_salvos');

                if ~isfolder(pastaInicial)
                    mkdir(pastaInicial);
                end

                pasta = uigetdir(pastaInicial, ...
                    'Selecione a pasta para salvar os dados da simulação');

                if isequal(pasta, 0)
                    return;
                end
            end

            sucesso = DownloadGaiola.executarDownload( ...
                app, ...
                'SIM_DOWNLOAD', ...
                pasta, ...
                'simulação orbital', ...
                app.TextoMensagemOrbita);

            if sucesso
                app.DadosSimulacaoDisponiveis = true;
                app.DadosSimulacaoBaixados = true;

                try
                    DownloadGaiola.converterSimulacaoBinariaParaCSV(pasta);
                    app.TextoMensagemOrbita.Text = ...
                        ['Download concluído. simulation_dados.csv criado em: ' pasta];
                catch erroConversao
                    app.TextoMensagemOrbita.Text = ...
                        ['Download concluído, mas a conversão de simulation.bin falhou: ' ...
                         erroConversao.message];
                end

                OrbitaGaiola.anexarEstadoEnsaio(app, 'SIM_DOWNLOAD_COMPLETE');
            end

            InterfaceGaiola.atualizarDisponibilidade(app);
        end

        % ============================================================
        % DOWNLOAD GENERICO
        % ============================================================

        function sucesso = executarDownload( ...
                app, comando, pasta, descricao, rotuloMensagem)

            sucesso = false;
            app.DownloadAtivo = true;

            if ~isempty(rotuloMensagem) && isvalid(rotuloMensagem)
                rotuloMensagem.Text = ...
                    ['Iniciando download: ' descricao '...'];
            end

            InterfaceGaiola.atualizarDisponibilidade(app);
            drawnow;

            try
                % Nenhum callback ou heartbeat pode disputar bytes com o
                % protocolo binário de download.
                ComunicacaoGaiola.pararTemporizadorHeartbeat(app);
                configureCallback(app.PortaSerial, "off");
                pause(0.05);
                flush(app.PortaSerial, "input");

                timeoutAnterior = app.PortaSerial.Timeout;
                app.PortaSerial.Timeout = 15.0;

                limpeza = onCleanup( ...
                    @() DownloadGaiola.finalizarModoDownload( ...
                    app, timeoutAnterior));

                writeline(app.PortaSerial, string(comando));

                % Aguarda especificamente DOWNLOAD_BEGIN. Mensagens de
                % estado residuais podem chegar entre o fim do ensaio e o
                % início da transferência e não devem abortar o download.
                linha = DownloadGaiola.aguardarInicioDownload(app, 15.0);

                numeroArquivos = ...
                    UtilGaiola.obterCampoNumero(app, linha, 'FILES');

                if ~isfinite(numeroArquivos) || numeroArquivos < 1
                    error('Cabeçalho DOWNLOAD_BEGIN inválido.');
                end

                numeroArquivos = round(numeroArquivos);

                for indiceArquivo = 1:numeroArquivos
                    DownloadGaiola.receberArquivoDownload( ...
                        app, ...
                        pasta, ...
                        indiceArquivo, ...
                        numeroArquivos, ...
                        descricao, ...
                        rotuloMensagem);
                end

                linhaFinal = DownloadGaiola.lerLinhaDownload(app);

                if linhaFinal ~= "DOWNLOAD_COMPLETE"
                    error('Final de download inesperado: %s', char(linhaFinal));
                end

                if ~isempty(rotuloMensagem) && isvalid(rotuloMensagem)
                    rotuloMensagem.Text = ...
                        ['Download concluído. Pasta: ' pasta];
                end

                sucesso = true;
                clear limpeza;

            catch erro
                if ~isempty(rotuloMensagem) && isvalid(rotuloMensagem)
                    rotuloMensagem.Text = ['Falha no download: ' erro.message];
                end

                uialert(app.JanelaPrincipal, erro.message, 'Download');
            end
        end

        function receberArquivoDownload( ...
                app, pasta, indiceArquivo, numeroArquivos, descricao, rotuloMensagem)

            cabecalho = DownloadGaiola.lerLinhaDownload(app);

            if startsWith(cabecalho, "DOWNLOAD_ERROR;") || ...
                    startsWith(cabecalho, "ERROR;")
                error('%s', char(cabecalho));
            end

            if ~startsWith(cabecalho, "FILE_BEGIN;")
                error('Esperado FILE_BEGIN e recebido: %s', char(cabecalho));
            end

            nome = UtilGaiola.obterCampoTexto(app, cabecalho, 'NAME');
            tamanho = UtilGaiola.obterCampoNumero(app, cabecalho, 'SIZE');
            numeroBlocos = UtilGaiola.obterCampoNumero(app, cabecalho, 'BLOCKS');

            if strlength(nome) == 0 || ~isfinite(tamanho) || ...
                    tamanho < 0 || ~isfinite(numeroBlocos) || numeroBlocos < 0
                error('Cabeçalho FILE_BEGIN inválido.');
            end

            nome = string(nome);
            caminhoFinal = fullfile(pasta, char(nome));
            caminhoParcial = [caminhoFinal '.part'];

            identificador = fopen(caminhoParcial, 'wb');

            if identificador < 0
                error('Não foi possível criar %s.', caminhoParcial);
            end

            fecharArquivo = onCleanup(@() DownloadGaiola.fecharArquivoSeguro(identificador));
            bytesRecebidos = 0;

            for blocoEsperado = 0:(round(numeroBlocos) - 1)
                blocoValido = false;

                while ~blocoValido
                    cabecalhoBloco = DownloadGaiola.lerLinhaDownload(app);

                    if startsWith(cabecalhoBloco, "DOWNLOAD_ERROR;") || ...
                            startsWith(cabecalhoBloco, "ERROR;")
                        error('%s', char(cabecalhoBloco));
                    end

                    if ~startsWith(cabecalhoBloco, "BLOCK_BEGIN;")
                        error( ...
                            'Esperado BLOCK_BEGIN e recebido: %s', ...
                            char(cabecalhoBloco));
                    end

                    indiceRecebido = UtilGaiola.obterCampoNumero( ...
                        app, cabecalhoBloco, 'INDEX');
                    tamanhoBloco = UtilGaiola.obterCampoNumero( ...
                        app, cabecalhoBloco, 'SIZE');
                    crcTexto = UtilGaiola.obterCampoTexto( ...
                        app, cabecalhoBloco, 'CRC');

                    if ~isfinite(indiceRecebido) || ...
                            round(indiceRecebido) ~= blocoEsperado || ...
                            ~isfinite(tamanhoBloco) || tamanhoBloco < 0 || ...
                            strlength(crcTexto) == 0
                        error('Cabeçalho de bloco inválido.');
                    end

                    tamanhoBloco = round(tamanhoBloco);
                    dados = read(app.PortaSerial, tamanhoBloco, "uint8");
                    dados = uint8(dados(:));

                    if numel(dados) ~= tamanhoBloco
                        error('Bloco recebido com tamanho incorreto.');
                    end

                    crcCalculado = DownloadGaiola.calcularCRC32(app, dados);
                    crcEsperado = uint32(hex2dec(char(crcTexto)));

                    if crcCalculado == crcEsperado
                        quantidade = fwrite(identificador, dados, 'uint8');

                        if quantidade ~= numel(dados)
                            error('Falha ao gravar o arquivo no computador.');
                        end

                        bytesRecebidos = bytesRecebidos + numel(dados);
                        writeline(app.PortaSerial, ...
                            sprintf('ACK;INDEX=%d', blocoEsperado));
                        blocoValido = true;
                    else
                        writeline(app.PortaSerial, ...
                            sprintf('NACK;INDEX=%d', blocoEsperado));
                    end
                end

                if ~isempty(rotuloMensagem) && isvalid(rotuloMensagem)
                    rotuloMensagem.Text = sprintf( ...
                        '%s: arquivo %d/%d - %s - %d/%d bytes', ...
                        descricao, ...
                        indiceArquivo, ...
                        numeroArquivos, ...
                        char(nome), ...
                        bytesRecebidos, ...
                        round(tamanho));
                end

                drawnow limitrate;
            end

            if bytesRecebidos ~= round(tamanho)
                error('Tamanho final incorreto em %s.', char(nome));
            end

            fimArquivo = DownloadGaiola.lerLinhaDownload(app);

            if ~startsWith(fimArquivo, "FILE_END;")
                error('Esperado FILE_END e recebido: %s', char(fimArquivo));
            end

            nomeFinal = UtilGaiola.obterCampoTexto(app, fimArquivo, 'NAME');

            if string(nomeFinal) ~= nome
                error('Nome do FILE_END não corresponde ao FILE_BEGIN.');
            end

            clear fecharArquivo;

            if isfile(caminhoFinal)
                delete(caminhoFinal);
            end

            [ok, mensagem] = movefile(caminhoParcial, caminhoFinal, 'f');

            if ~ok
                error('Falha ao finalizar %s: %s', caminhoFinal, mensagem);
            end

            writeline(app.PortaSerial, "FILE_ACK;NAME=" + nome);
        end

        function linha = lerLinhaDownload(app)
            inicio = tic;
            linha = "";

            while toc(inicio) < max(10.0, app.PortaSerial.Timeout + 2.0)
                if app.PortaSerial.NumBytesAvailable > 0
                    linha = strtrim(string(readline(app.PortaSerial)));
                    if strlength(linha) > 0
                        return;
                    end
                else
                    pause(0.002);
                end
            end

            error('Tempo limite aguardando dados do Teensy.');
        end

        function linha = aguardarInicioDownload(app, tempoLimite_s)
            inicio = tic;
            linha = "";

            while toc(inicio) < tempoLimite_s
                recebida = DownloadGaiola.lerLinhaDownload(app);

                if startsWith(recebida, "DOWNLOAD_BEGIN;")
                    linha = recebida;
                    return;
                end

                if startsWith(recebida, "ERROR;") || ...
                        startsWith(recebida, "DOWNLOAD_ERROR;")
                    error('Controlador respondeu: %s', char(recebida));
                end

                % Respostas assíncronas/residuais conhecidas são ignoradas.
                if recebida == "CONTROL_DOWNLOAD_READY" || ...
                        recebida == "SIM_DOWNLOAD_READY" || ...
                        recebida == "CONTROL_COMPLETE" || ...
                        recebida == "SIMULATION_COMPLETE" || ...
                        recebida == "HEARTBEAT_OK" || ...
                        startsWith(recebida, "STATUS;") || ...
                        startsWith(recebida, "CONTROL_STATUS;") || ...
                        startsWith(recebida, "SIM_STATUS;")
                    continue;
                end

                % Qualquer outra linha textual anterior ao cabeçalho é
                % ignorada, pois o próximo dado binário só aparece depois
                % de DOWNLOAD_BEGIN e FILE_BEGIN.
            end

            error('Tempo limite aguardando DOWNLOAD_BEGIN do Teensy.');
        end

        % ============================================================
        % CONVERSAO DOS REGISTROS BINARIOS PARA CSV
        % ============================================================

        function converterControleBinarioParaCSV(pasta)
            caminho = fullfile(pasta, 'control.bin');
            if ~isfile(caminho)
                error('control.bin não foi encontrado.');
            end

            tamanhoRegistro = 60;
            bytes = DownloadGaiola.lerRegistrosBinarios(caminho, tamanhoRegistro);
            quantidade = size(bytes, 2);

            tempo_ms = double(DownloadGaiola.extrairUint32(bytes, 1:4));
            bref = double(DownloadGaiola.extrairSingle(bytes, 5:16));
            bmed = double(DownloadGaiola.extrairSingle(bytes, 17:28));
            % PI campo direto: IREF = NaN (sem referencia de corrente).
            % Ix_medido representa corrente da FONTE VM, nao da bobina.
            iref = double(DownloadGaiola.extrairSingle(bytes, 29:40));
            imed = double(DownloadGaiola.extrairSingle(bytes, 41:52));
            pwm = double(DownloadGaiola.extrairInt16(bytes, 53:58));
            indicadores = DownloadGaiola.extrairUint16(bytes, 59:60);

            tempo_s = tempo_ms / 1000.0;
            correnteXValida = logical(bitget(indicadores, 1));
            correnteYValida = logical(bitget(indicadores, 2));
            correnteZValida = logical(bitget(indicadores, 3));
            campoXValido = logical(bitget(indicadores, 4));
            campoYValido = logical(bitget(indicadores, 5));
            campoZValido = logical(bitget(indicadores, 6));

            if quantidade == 0
                error('control.bin está vazio.');
            end

            tabela = table( ...
                tempo_s, ...
                bref(:,1), bref(:,2), bref(:,3), ...
                bmed(:,1), bmed(:,2), bmed(:,3), ...
                iref(:,1), iref(:,2), iref(:,3), ...
                imed(:,1), imed(:,2), imed(:,3), ...
                pwm(:,1), pwm(:,2), pwm(:,3), ...
                indicadores, ...
                correnteXValida, correnteYValida, correnteZValida, ...
                campoXValido, campoYValido, campoZValido, ...
                'VariableNames', { ...
                    'Tempo_s', ...
                    'Bx_ref_uT','By_ref_uT','Bz_ref_uT', ...
                    'Bx_medido_uT','By_medido_uT','Bz_medido_uT', ...
                    'Ix_ref_A','Iy_ref_A','Iz_ref_A', ...
                    'Ix_medido_A','Iy_medido_A','Iz_medido_A', ...
                    'PWM_X','PWM_Y','PWM_Z', ...
                    'Indicadores', ...
                    'CorrenteX_valida','CorrenteY_valida','CorrenteZ_valida', ...
                    'CampoX_valido','CampoY_valido','CampoZ_valido'});

            writetable(tabela, fullfile(pasta, 'control_dados.csv'));
        end

        function converterSimulacaoBinariaParaCSV(pasta)
            caminho = fullfile(pasta, 'simulation.bin');
            if ~isfile(caminho)
                error('simulation.bin não foi encontrado.');
            end

            tamanhoRegistro = 68;
            bytes = DownloadGaiola.lerRegistrosBinarios(caminho, tamanhoRegistro);
            quantidade = size(bytes, 2);

            tempoReal_ms = double(DownloadGaiola.extrairUint32(bytes, 1:4));
            indiceTrajetoria = double(DownloadGaiola.extrairUint32(bytes, 5:8));
            tempoSimulado_s = double(DownloadGaiola.extrairSingle(bytes, 9:12));
            bref = double(DownloadGaiola.extrairSingle(bytes, 13:24));
            bmed = double(DownloadGaiola.extrairSingle(bytes, 25:36));
            % PI campo direto: IREF = NaN; I medida e corrente em VM.
            iref = double(DownloadGaiola.extrairSingle(bytes, 37:48));
            imed = double(DownloadGaiola.extrairSingle(bytes, 49:60));
            pwm = double(DownloadGaiola.extrairInt16(bytes, 61:66));
            indicadores = DownloadGaiola.extrairUint16(bytes, 67:68);

            tempoReal_s = tempoReal_ms / 1000.0;
            correnteXValida = logical(bitget(indicadores, 1));
            correnteYValida = logical(bitget(indicadores, 2));
            correnteZValida = logical(bitget(indicadores, 3));
            campoXValido = logical(bitget(indicadores, 4));
            campoYValido = logical(bitget(indicadores, 5));
            campoZValido = logical(bitget(indicadores, 6));

            if quantidade == 0
                error('simulation.bin está vazio.');
            end

            tabela = table( ...
                tempoReal_s, indiceTrajetoria, tempoSimulado_s, ...
                bref(:,1), bref(:,2), bref(:,3), ...
                bmed(:,1), bmed(:,2), bmed(:,3), ...
                iref(:,1), iref(:,2), iref(:,3), ...
                imed(:,1), imed(:,2), imed(:,3), ...
                pwm(:,1), pwm(:,2), pwm(:,3), ...
                indicadores, ...
                correnteXValida, correnteYValida, correnteZValida, ...
                campoXValido, campoYValido, campoZValido, ...
                'VariableNames', { ...
                    'Tempo_real_s','Indice_trajetoria','Tempo_simulado_s', ...
                    'Bx_ref_uT','By_ref_uT','Bz_ref_uT', ...
                    'Bx_medido_uT','By_medido_uT','Bz_medido_uT', ...
                    'Ix_ref_A','Iy_ref_A','Iz_ref_A', ...
                    'Ix_medido_A','Iy_medido_A','Iz_medido_A', ...
                    'PWM_X','PWM_Y','PWM_Z', ...
                    'Indicadores', ...
                    'CorrenteX_valida','CorrenteY_valida','CorrenteZ_valida', ...
                    'CampoX_valido','CampoY_valido','CampoZ_valido'});

            writetable(tabela, fullfile(pasta, 'simulation_dados.csv'));
        end

        function bytes = lerRegistrosBinarios(caminho, tamanhoRegistro)
            info = dir(caminho);
            if isempty(info)
                error('Arquivo não encontrado: %s', caminho);
            end

            if mod(info.bytes, tamanhoRegistro) ~= 0
                error( ...
                    ['Tamanho de %s (%d bytes) não é múltiplo do tamanho ' ...
                     'de registro esperado (%d bytes).'], ...
                    caminho, info.bytes, tamanhoRegistro);
            end

            quantidade = info.bytes / tamanhoRegistro;
            identificador = fopen(caminho, 'rb');
            if identificador < 0
                error('Não foi possível abrir %s.', caminho);
            end

            fechar = onCleanup(@() fclose(identificador));
            bytes = fread(identificador, [tamanhoRegistro quantidade], '*uint8');

            if size(bytes, 2) ~= quantidade
                error('Leitura incompleta de %s.', caminho);
            end
        end

        function valores = extrairSingle(bytes, faixa)
            dados = reshape(bytes(faixa, :), 1, []);
            valores = typecast(uint8(dados), 'single');
            valores = DownloadGaiola.corrigirEndian(valores);
            valores = reshape(valores, numel(faixa) / 4, []).';
        end

        function valores = extrairUint32(bytes, faixa)
            dados = reshape(bytes(faixa, :), 1, []);
            valores = typecast(uint8(dados), 'uint32');
            valores = DownloadGaiola.corrigirEndian(valores).';
        end

        function valores = extrairInt16(bytes, faixa)
            dados = reshape(bytes(faixa, :), 1, []);
            valores = typecast(uint8(dados), 'int16');
            valores = DownloadGaiola.corrigirEndian(valores);
            valores = reshape(valores, numel(faixa) / 2, []).';
        end

        function valores = extrairUint16(bytes, faixa)
            dados = reshape(bytes(faixa, :), 1, []);
            valores = typecast(uint8(dados), 'uint16');
            valores = DownloadGaiola.corrigirEndian(valores).';
        end

        function valores = corrigirEndian(valores)
            [~, ~, endian] = computer;
            if endian == 'B'
                valores = swapbytes(valores);
            end
        end

        function finalizarModoDownload(app, timeoutAnterior)
            if ~isempty(app.PortaSerial)
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

            app.DownloadAtivo = false;
            InterfaceGaiola.atualizarDisponibilidade(app);
        end

        function fecharArquivoSeguro(identificador)
            try
                if identificador > 0
                    fclose(identificador);
                end
            catch
            end
        end

        % ============================================================
        % mesmo algoritmo do firmware
        % ============================================================

        function crc = calcularCRC32(~, dados)
            persistent tabelaCRC

            if isempty(tabelaCRC)
                tabelaCRC = zeros(256, 1, 'uint32');
                polinomio = uint32(hex2dec('EDB88320'));

                for indice = 0:255
                    valor = uint32(indice);

                    for bit = 1:8
                        if bitand(valor, uint32(1)) ~= 0
                            valor = bitxor(bitshift(valor, -1), polinomio);
                        else
                            valor = bitshift(valor, -1);
                        end
                    end

                    tabelaCRC(indice + 1) = valor;
                end
            end

            crc = uint32(hex2dec('FFFFFFFF'));

            for indice = 1:numel(dados)
                tabelaIndice = bitand( ...
                    bitxor(crc, uint32(dados(indice))), ...
                    uint32(255));

                crc = bitxor( ...
                    bitshift(crc, -8), ...
                    tabelaCRC(double(tabelaIndice) + 1));
            end

            crc = bitxor(crc, uint32(hex2dec('FFFFFFFF')));
        end
    end
end
