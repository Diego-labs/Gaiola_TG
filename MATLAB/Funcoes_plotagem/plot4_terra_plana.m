function plot4_terra_plana ()

    % carregar imagem da Terra (projeção equiretangular)
    img = imread('2k_earth_daymap.jpg');
    
    % criar o mapa em 2D
    % plota a imagem no intervalo de coordenadas
    % inverte a imagem para que o polo norte fique pra cima
    imagesc([-180 180], [-90 90], flipud(img));
    axis xy;
    axis equal tight;
    
    hold on;
    
    % definindo intervalo das grades de 15 em 15 graus
    grade_latitudes = -90:15:90;
    grade_longitudes = -180:15:180;
    
    % marcador dos eixos de 30 em 30 graus
    marcador_latitudes = -90:30:90;
    marcador_longitudes = -180:30:180;
    
    % desenha linhas de latitude
    for lat = grade_latitudes
        plot([-180, 180], [lat, lat], 'k:', 'LineWidth', 0.5);
    end
    
    % desenha linhas de longitude
    for lon = grade_longitudes
        plot([lon, lon], [-90, 90], 'k:', 'LineWidth', 0.5);
    end
    
    % configurar apenas os ticks desejados (30 em 30)
    set(gca, 'XTick', marcador_longitudes);
    set(gca, 'YTick', marcador_latitudes);
    set(gca, 'XTickLabel', string(marcador_longitudes));
    set(gca, 'YTickLabel', string(marcador_latitudes));


end