function SisInercial = rotacao_para_inercial (angulo, vetor)

    angulo = deg2rad(angulo);
    matriz_rotacao = [
                        cos(angulo), sin(angulo), 0;
                        -sin(angulo), cos(angulo), 0;
                        0, 0, 1
                        ];

    SisInercial = matriz_rotacao * vetor;

end