function derivadas = derivadas_orbita_dois_corpos(t, X)
    % parâmetro gravitacional padrão (km^3/s^2)
    u = 3.986 * (10^5); 

    % Extrai as componentes do vetor de estado X
    r_vec = X(1:3); % [x, y, z]
    v_vec = X(4:6); % [vx, vy, vz]
    r = norm(r_vec);
    
    % calcula a aceleração
    accel = (-u / r^3) * r_vec;
    
    % retorna as derivadas
    derivadas = [v_vec; accel];
end