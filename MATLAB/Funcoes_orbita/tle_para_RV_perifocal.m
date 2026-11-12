function [R,V, W, c, w] = tle_para_RV_perifocal(tle)
    % converte TLE para os vetores de posicao e
    % velocidade no sistema de coordenadas perifocal

    % parametro gravitacional para a Terra (km^3/s^2)
    u = 3.986 * (10^5); 

    % divide a string da segunda linha do tle
    part_tle = strsplit(tle(2, :));
    % pega a string da excentricidade
    e_str = part_tle{5};
    parte_tle = str2double(part_tle);
    
    % c = inclinacao
    c = deg2rad(parte_tle(3));
    % ascensao reta do nodo ascendente
    W = deg2rad(parte_tle(4));
    % excentricidade
    e = str2double(['0.' e_str]);

    % argumento de pericentro
    w = deg2rad(parte_tle(6));
    % anomalia media
    M = deg2rad(parte_tle(7));
    % movimento medio
    vel_med = parte_tle(8);
    
    % calcula a anomalia verdadeira usando newton-raphson
    theta = retorna_AnomVerdadeira_NR(e, M);

    % determinacao semi-eixo maior
    n = vel_med * ((2*pi)/(86400));
    a = nthroot(u / (n^2), 3);

    % magnitude e vetor R no perifocal
    r = (a*(1 - e^2)) / (1 + e*cos(theta));
    R = [r*cos(theta); r*sin(theta); 0];

    % momento angular especifico a partir do semi-eixo maior
    h = sqrt(a*u*(1 - e^2));

    % vetor velocidade no perifocal
    V = [-(u/h)*sin(theta); (u/h)*(e + cos(theta)); 0];

end