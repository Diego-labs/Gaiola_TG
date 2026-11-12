function Vetor_cartesiano = geografica_para_cartesiana (coord_geograf)
    % geodesico para geocentrico/cartesiano
    % calculo da excentricidade do elipsoide de revolucao (Terra)
    % a - raio equatorial (Km)
    % b - raio polar (Km)
    a = 6378.137;
    b = 6356.7523142;
    e = sqrt(a^2 - b^2) / a;

    lat = deg2rad(coord_geograf(1));
    long = deg2rad(coord_geograf(2));
    % em Km
    altit = coord_geograf(3)/1000;

    % raio de curvatura
    N = a / sqrt(1 - e^2 * sin(lat)^2);
    
    x = (N + altit) * cos(lat) * cos(long);
    y = (N + altit) * cos(lat) * sin(long);
    z = ((N * (1 - e^2)) + altit) * sin(lat);

    Vetor_cartesiano = [x y z];
end