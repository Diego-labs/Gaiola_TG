function resultado = simular_e_plotar_orbita_2D( ...
    R, V, temp_sideral, ano_decimal, data, tempo1)

    % Vetor de estado
    % R e V podem chegar como vetores coluna
    % O ode45 precisa do estado inicial como vetor  

    X = [R(:).' V(:).'];

    % Parametro gravitacional para a Terra (km^3/s^2)
    u = 398600.4418e0;

    % Obtem h, i, e, Nodo_Ascen, w, Anom_Verdadeira e a.
    elementos_orbitais_iniciais = calcular_elementos_orbitais(R, V);

    % Frequencia angular da orbita / frequencia de Kepler.
    n = sqrt(u / (elementos_orbitais_iniciais(7)^3));

    % Periodo orbital.
    T = (2 * pi) / n;
    tempo = T / 60;

    t_simulacao = 0:1:T;

    % ------------------------------------------------------------
    % Informacoes no Command Window

    disp("Elementos orbitais iniciais:");
    fprintf('Inclinacao: %.2f graus\n', ...
        rad2deg(elementos_orbitais_iniciais(2)));
    fprintf('Excentricidade: %f\n', ...
        elementos_orbitais_iniciais(3));
    fprintf('ARNA: %.2f graus\n', ...
        rad2deg(elementos_orbitais_iniciais(4)));
    fprintf('Argumento do periapsis: %.2f graus\n', ...
        rad2deg(elementos_orbitais_iniciais(5)));
    fprintf('Anomalia verdadeira: %.2f graus\n', ...
        rad2deg(elementos_orbitais_iniciais(6)));
    fprintf('Semi-eixo maior: %.2f km\n\n', ...
        elementos_orbitais_iniciais(7));
    fprintf('Tempo para integrar uma orbita: %.2f minutos\n', tempo);

    % ------------------------------------------------------------
    % Integracao numerica

    options = odeset('RelTol', 1e-6, 'AbsTol', 1e-10);

    [t, solucao] = ode45( ...
        @derivadas_orbita_dois_corpos, ...
        t_simulacao, ...
        X, ...
        options);

    % ------------------------------------------------------------
    % Coordenadas

    [lat, long] = Vetor_estado_para_lat_long( ...
        solucao, t, temp_sideral);

    [lat_geo, long_geo, alt_geo] = Vetor_estado_para_geodesicas( ...
        solucao, t, temp_sideral);

    % ------------------------------------------------------------
    % Campo magnetico IGRF

    [Bn, Be, Bd, B] = Campo_magnetico_IGRF( ...
        lat_geo, long_geo, alt_geo, ano_decimal);

    % Garante vetores coluna para tabelas e para o envio ao Teensy
    t = t(:);
    lat_geo = lat_geo(:);
    long_geo = long_geo(:);
    alt_geo = alt_geo(:);
    Bn = Bn(:);
    Be = Be(:);
    Bd = Bd(:);
    B = B(:);

    % ------------------------------------------------------------
    % Tabelas

    T_estado = table( ...
        t, ...
        solucao(:,1), solucao(:,2), solucao(:,3), ...
        solucao(:,4), solucao(:,5), solucao(:,6), ...
        lat_geo, long_geo, alt_geo, ...
        'VariableNames', ...
        {'Tempo_s', ...
         'X_km','Y_km','Z_km', ...
         'Vx_km_s','Vy_km_s','Vz_km_s', ...
         'Latitude_deg','Longitude_deg','Altitude_km'});

    T_campo = table( ...
        t, Bn, Be, Bd, B, ...
        'VariableNames', ...
        {'Tempo_s','Bn_uT','Be_uT','Bd_uT','Btotal_uT'});

    % ------------------------------------------------------------
    % Saida

    resultado = struct();
    resultado.t = t;
    resultado.solucao = solucao;
    resultado.latitude_geocentrica_deg = lat(:);
    resultado.longitude_geocentrica_deg = long(:);
    resultado.latitude_geodesica_deg = lat_geo;
    resultado.longitude_geodesica_deg = long_geo;
    resultado.altitude_km = alt_geo;
    resultado.Bn_uT = Bn;
    resultado.Be_uT = Be;
    resultado.Bd_uT = Bd;
    resultado.Btotal_uT = B;
    resultado.elementos_orbitais_iniciais = elementos_orbitais_iniciais;
    resultado.periodo_s = T;
    resultado.periodo_min = tempo;
    resultado.passo_minimo_s = min(diff(t));
    resultado.tabela_estado = T_estado;
    resultado.tabela_campo = T_campo;
    resultado.data_tle = data;
    resultado.tempo_tle = tempo1;
    resultado.ano_decimal = ano_decimal;
end
