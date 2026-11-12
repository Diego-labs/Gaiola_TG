function receber_TLE(tle)
    addpath('Funcoes_plotagem/');
    addpath('Funcoes_orbita/');
    addpath('Funcoes_coordenadas/');
    addpath('Funcoes_simulacoes/');
    addpath('Funcoes_campo_magnetico/');

    % W -> nodo ascendente
    % c -> inclinacao
    % w -> argumento de periastro

    [R_peri,V_peri, W, c, w] = tle_para_RV_perifocal(tle);
    % obtem a matriz de rotacao para converter do perifocal para inercial
    Mat_per2iner = matriz_rotacao_perifocal_para_inercial(0, W, c, w);

    u = 3.986 * (10^5); 
    R = Mat_per2iner * R_peri;
    V = Mat_per2iner * V_peri;

    % obter data e hora do TLE
    % dia mes ano
    % hora minuto segundo
    [data, tempo, ano_decimal] = retorna_DataHora_TLE(tle);

    % obter tempo sideral de Greenwick
    ang_horario = tempo_sideralGN(data, tempo);
    %fprintf('tempo sideral %.6f\n', ang_horario);

    % inicia a simulacaoo e visualizacaoo da trajetoria orbital
    % a função lida com todo o processo,
    % desde a integracaoo numerica ate a plotagem.
    simular_e_plotar_orbita_2D(R, V, ang_horario, ano_decimal, data, tempo);
end