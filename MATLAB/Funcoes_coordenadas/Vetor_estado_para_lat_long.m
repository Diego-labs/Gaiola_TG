function [latitude, longitude] = Vetor_estado_para_lat_long (vet_estado, t, tempo_sideral)

    R = vet_estado(:, 1:3);
    X = vet_estado(:, 1);
    Y = vet_estado(:, 2);
    Z = vet_estado(:, 3);
    
    iteracoes = length(R(:, 1));
    latitude = zeros(iteracoes, 1);
    longitude = zeros(iteracoes, 1);
    
    % taxa de rotacao da Terra em graus por segundo
    omega_terra = 360 / 86164.0905;
   
    for i = 1:iteracoes
        r = norm(R(i, :));
        latitude(i, 1) = rad2deg(asin(Z(i)/r));

        tempo_sideral_atual = tempo_sideral + omega_terra * t(i);
        
        % o calculo da longitude leva em conta a rotacao
        lon_deg = rad2deg(atan2(Y(i), X(i))) - tempo_sideral_atual;
        longitude(i) = wrapTo180(lon_deg);
       
    end

end