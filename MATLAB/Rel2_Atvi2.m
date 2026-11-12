function resultado = Rel2_Atvi2(tle, executarSimulacao)

    if nargin < 1 || isempty(tle)
        % TLE de exemplo
        tle = [
            '1 39452U 13067B   26283.14791775  .00010387  00000-0  17703-3 0  9992';
            '2 39452  87.3381  13.5418 0003077  99.5793 260.5816 15.51199968724067'
        ];
    end

    if nargin < 2
        executarSimulacao = true;
    end

    tle = normalizarTLE(tle);

    % Pastas com as funcoes ja utilizadas

    pastaPrincipal = fileparts(mfilename('fullpath'));

    pastasFuncoes = {
        'Funcoes_plotagem'
        'Funcoes_orbita'
        'Funcoes_coordenadas'
        'Funcoes_simulacoes'
        'Funcoes_campo_magnetico'
    };

    for indice = 1:numel(pastasFuncoes)
        caminho = fullfile(pastaPrincipal, pastasFuncoes{indice});
        if isfolder(caminho)
            addpath(caminho);
        end
    end

    funcoesObrigatorias = { ...
        'tle_para_RV_perifocal', ...
        'matriz_rotacao_perifocal_para_inercial', ...
        'retorna_DataHora_TLE', ...
        'tempo_sideralGN', ...
        'calcular_elementos_orbitais'};

    if executarSimulacao
        funcoesObrigatorias = [funcoesObrigatorias, { ...
            'derivadas_orbita_dois_corpos', ...
            'Vetor_estado_para_lat_long', ...
            'Vetor_estado_para_geodesicas', ...
            'Campo_magnetico_IGRF'}];
    end

    ausentes = strings(0, 1);
    for indice = 1:numel(funcoesObrigatorias)
        if exist(funcoesObrigatorias{indice}, 'file') ~= 2
            ausentes(end + 1, 1) = string(funcoesObrigatorias{indice}); 
        end
    end

    if ~isempty(ausentes)
        error( ...
            ['Funções orbitais não encontradas: %s. ' ...
             'Mantenha as pastas Funcoes_* junto deste arquivo.'], ...
            strjoin(ausentes, ', '));
    end

    % W -> nodo ascendente
    % c -> inclinacao
    % w -> argumento de periastro
    [R_peri, V_peri, W, c, w] = tle_para_RV_perifocal(tle);

    % Matriz de rotacao perifocal -> inercial.
    Mat_per2iner = matriz_rotacao_perifocal_para_inercial(0, W, c, w);

    R = Mat_per2iner * R_peri;
    V = Mat_per2iner * V_peri;

    % Data/hora e ano decimal do TLE.
    [data, tempo, ano_decimal] = retorna_DataHora_TLE(tle);

    % Tempo sideral de Greenwich.
    ang_horario = tempo_sideralGN(data, tempo);

    % Elementos orbitais sao calculados aqui tambem para que a tela
    % consiga apresenta-los logo apos "Confirmar TLE", sem executar
    % toda a propagacao.
    elementos_orbitais_iniciais = calcular_elementos_orbitais(R, V);

    u = 398600.4418e0;
    n = sqrt(u / (elementos_orbitais_iniciais(7)^3));
    periodo_s = (2 * pi) / n;

    resultado = struct();
    resultado.tle = tle;
    resultado.R_km = R;
    resultado.V_km_s = V;
    resultado.data = data;
    resultado.tempo = tempo;
    resultado.ano_decimal = ano_decimal;
    resultado.tempo_sideral = ang_horario;
    resultado.elementos_orbitais_iniciais = elementos_orbitais_iniciais;
    resultado.periodo_s = periodo_s;
    resultado.periodo_min = periodo_s / 60;
    resultado.simulacao = struct();

    if executarSimulacao
        resultado.simulacao = simular_e_plotar_orbita_2D( ...
            R, ...
            V, ...
            ang_horario, ...
            ano_decimal, ...
            data, ...
            tempo);
    end
end


function tle = normalizarTLE(tleEntrada)
% Converte cell/string/char para a matriz char de duas linhas esperada
% pelas funcoes orbitais antigas.

    if isstring(tleEntrada)
        linhas = cellstr(tleEntrada(:));
    elseif iscell(tleEntrada)
        linhas = tleEntrada(:);
    elseif ischar(tleEntrada)
        if size(tleEntrada, 1) ~= 2
            error('O TLE deve possuir exatamente duas linhas.');
        end
        linhas = {tleEntrada(1, :); tleEntrada(2, :)};
    else
        error('Formato de TLE nao suportado.');
    end

    if numel(linhas) ~= 2
        error('O TLE deve possuir exatamente duas linhas.');
    end

    linha1 = strtrim(char(linhas{1}));
    linha2 = strtrim(char(linhas{2}));

    if isempty(linha1) || isempty(linha2) || ...
            ~startsWith(string(linha1), "1 ") || ...
            ~startsWith(string(linha2), "2 ")
        error('Formato de TLE invalido. Verifique as linhas 1 e 2.');
    end

    tle = char([string(linha1); string(linha2)]);
end
