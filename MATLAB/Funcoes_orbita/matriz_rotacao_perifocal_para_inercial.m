function mat_final = matriz_rotacao_perifocal_para_inercial(sentido, W, c, w)

    % sentido ~= 1 : perifocal -> inercial
    % sentido = 1 : inercial -> perifocal 
    % W -> nodo ascendente
    % c -> inclinacao
    % w -> argumento de periastro
   
    mat_final = [
    cos(W)*cos(w) - cos(c)*sin(W)*sin(w), -cos(W)*sin(w) - cos(c)*cos(w)*sin(W),  sin(c)*sin(W);
    cos(w)*sin(W) + cos(c)*cos(W)*sin(w),  cos(c)*cos(W)*cos(w) - sin(W)*sin(w), -sin(c)*cos(W);
    sin(c)*sin(w),                         sin(c)*cos(w),                         cos(c)
                ];
    
    if sentido == 1
        mat_final = mat_final.';
    end
end