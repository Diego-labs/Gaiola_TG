function theta = tempo_sideralGN (data, tempo)
    % data: dia ; mes ; ano
    % tempo: hora ; min ; seg

    % para confirmar
    % https://aa.usno.navy.mil/data/JulianDate
    % https://www.celnav.de/longterm.htm
    % https://www.walter-fendt.de/html5/aen/siderealtime_en.htm
    
    % primeiro eh definida a Data Juliana

    % conta da JD dividida para ficar mais legivel
    JD_part1 = 367 * data(3);
    JD_part2 = (7 * (data(3) + floor((data(2) + 9) / 12))) / 4;
    JD_part3 = (275 * data(2)) / 9;
    JD0 = JD_part1 - floor(JD_part2) + floor(JD_part3) + data(1) + 1721013.5;

    % passa todos os componentes do vetor tempo para hora
    UT = tempo(1) + (tempo(2)/60) + (tempo(3)/3600);
    JD = JD0 + (UT/24);

    % agora calculos para determinacao do tempo sideral de Greenwich
    T0 = (JD - 2415020) / 36525;

    % calculo do angulo dividido em partes para ficar mais legivel
    % valores em graus
    theta1 = 99.6909833 + (36000.7689 * T0);
    theta2 = 0.00038708 * (T0^2);
    theta0 = theta1 + theta2;

    % ajuste de intervalo de angulo para 0 <= theta0 <= 360
    theta0 = mod(theta0, 360);
    theta = theta0 + 360.98564724 * (UT/24);
    theta = mod(theta, 360);

end