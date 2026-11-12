function plot5_orbita_2d(lat, long)

    % encontra os pontos onde a longitude salta abruptamente
    % salto > 150 graus indica que o meridiano foi cruzado
    % diff calcula a diferena entre elementos consecutivos do vetor long
    % abs() > 150 encontra saltos maiores que 150
    % find retorna os indices apos o salto
    % valor de 150 graus foi escolhido arbitrariamente
    idx_quebra = find(abs(diff(long)) > 150) + 1;
    
    % insere NaN nos pontos de quebra para interromper a linha
    % NaN em um vetor de plotagem interrompe a linha
    if ~isempty(idx_quebra)
        % para cada quebra, insere NaN no ponto correspondente
        % tanto para lat e long, assim ambos com Nan e linha interrompida
        long(idx_quebra) = NaN;
        lat(idx_quebra) = NaN;
    end


    plot(long(1), lat(1), 'o', 'MarkerSize', 10, 'Color', 'g');
    hold on;  % mantém o gráfico para o próximo plot
    
    plot(long, lat, 'LineWidth', 2, 'Color', 'r');
    
    axis equal;
    grid on;
    xlabel('Longitude (°)', 'FontSize', 16, 'FontWeight', 'bold');
    ylabel('Latitude (°)', 'FontSize', 16, 'FontWeight', 'bold');
    title('Ground track', 'FontSize', 14, 'FontWeight', 'bold');
    set(gca, 'FontSize', 14, 'FontWeight', 'bold');

end