function theta = retorna_AnomVerdadeira_NR (e, M)
    % encontrar a anomalia excentrica (u) a partir da anomalia media (M)
    % em seguida, converte E para a anomalia verdadeira (theta)

    % tolerancia para a convergencia do metodo
    cond_parada = 10^-8;
    % numero maximo de iteracoes para evitar loops infinitos
    max_iteracoes = 30;

    % calcula a estimativa inicial para a anomalia excentrica (u)
    if M < pi
        ui = M + (e/2);
    else
        ui = M - (e/2);
    end

    % laco para o metodo de newton-raphson
    for i = 1:max_iteracoes
        f = M - ui + e*sin(ui);
        df = -1 + e*cos(ui);
        % proxima aproximacao da anomalia excentrica
        % pode apresentar instabilidade para orbita muito excentrica ~ 1
        un = ui - (f/df);
        if abs(un - ui) <= cond_parada
            % converte a anomalia excentrica (un) para a anomalia verdadeira
            % uso de atan2 para obter o quadrante correto
            theta = atan2(sqrt(1-e^2)*sin(un), cos(un)-e);
            % para que o resultado esteja no intervalo [0, 2*pi]
            if theta < 0
                theta = theta + 2*pi; 
            end
            return;
        end
        ui = un;
    end
    % retorna NaN se o metodo nao convergir apos o numero maximo de iteracoes
    theta = NaN;
end