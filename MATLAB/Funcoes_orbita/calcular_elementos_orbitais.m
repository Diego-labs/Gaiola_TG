function elementos = calcular_elementos_orbitais(R, V)
    % parametro para tratamento de orbitas quase circulares
    eps = 1.e-10;
    % parametro gravitacional para a Terra (km^3/s^2)
    u = 3.986 * (10^5); 
    r = norm(R);
    v = norm(V);

    % determinacao da energia mecanica total
    energia = ((v^2) / 2) - (u/r);
    % definindo a velocidade radial
    v_rad = dot(R, V) / r;

    % obtendo momento angular especifico
    H = cross(R, V);
    h = norm(H);
    
    % determinacao da inclinacao
    incli = acos(H(3) / h);

    % determicacao do nodo ascendente
    % tratamento para caso a orbita sem inclinacao (else)
    % [0 0 1] eh o versor do eixo de rotacao da terra
    N = cross([0 0 1], H);
    n = norm(N);
    if n ~= 0
        % ajuste do quadrante
        nodo_ascen = acos(N(1) / n);
        if N(2) < 0
            nodo_ascen = 2*pi - nodo_ascen;
        end
    else
        nodo_ascen = 0;
    end
   
    % determincao da excentricidade
    E = (1/u)*((v^2 - (u/r))*R - r*v_rad* V);
    e = norm(E);
    
    % calcula o argumento do pericentro
    % angulo entre o vetor do nodo ascendente e o vetor de excentricidade
    % definindo como zero para caso de quase orbita circular ou sem 
    % inclinacao
    if n ~= 0
        if e > eps
            w = acos(dot(N,E)/(n*e));
            if E(3) < 0
                w = 2*pi - w;
            end
        else
            w = 0; % para orbita circular
        end
    else
        w = 0; % para orbita equatorial sem inclinacao
    end

    % se for eliptica determina a partir dos vetores Excentricidade e Posicao
    % sinal de acordo com velocidade radial: aproximando/afastando
    % se for circular com inclinacao: vetor nodo ascendente e Posicao
    % sinal de acordo com terceiro eixo do prod. vetorial
    % se for circular sem inclinacao: vetor arbitrario X e Posicao
    % sinal de acordo com terceiro eixo do prod. vetorial
    if e > eps          % eliptica
        referencia = E;
        divisor = e;
        sinal = v_rad;
    else                % circular
        if n ~= 0       % com inclinacao
            referencia = N;
            divisor = n;
            produt_NR = cross(N, R);
            sinal = produt_NR(3);
        else            % sem inclinacao
            referencia = [1,0,0];
            divisor = 1;
            produt_XR = cross([1,0,0], R);
            sinal = produt_XR(3);
        end
    end
    
    theta = acos(dot(referencia, R)/(divisor*r));
    if sinal < 0
        theta = 2*pi - theta;
    end

    % determinacao semi-eixo maior
    a = - (u) / (2 * energia);
    elementos = [h incli  e nodo_ascen w theta a];
end