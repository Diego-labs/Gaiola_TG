classdef UtilGaiola
    % ============================================================
    % UTILGAIOLA
    % ============================================================
    %
    % Funções auxiliares de parsing, eixos e máscaras.
    %
    % Esta classe contém apenas métodos estáticos usados por
    % ControleGaiolaHelmholtz.m.
    % ============================================================

    methods (Static)
        function valor = obterCampoTexto(~, linha, chave)
            linha = char(linha);
            chave = regexptranslate('escape', char(chave));

            expressao = ['(?:^|;)' chave '=([^;]+)'];
            resultado = regexp(linha, expressao, 'tokens', 'once');

            if isempty(resultado)
                valor = "";
            else
                valor = strtrim(string(resultado{1}));
            end
        end


        function valor = obterCampoNumero(app, linha, chave)
            texto = UtilGaiola.obterCampoTexto(app, linha, chave);

            if strlength(texto) == 0
                valor = NaN;
            else
                valor = str2double(texto);
            end
        end


        % ============================================================
        % FUNCOES AUXILIARES - EIXOS
        % ============================================================

        function indice = indiceEixo(~, eixo)
            switch upper(char(eixo))
                case 'X'
                    indice = 1;
                case 'Y'
                    indice = 2;
                case 'Z'
                    indice = 3;
                otherwise
                    error('Eixo invalido.');
            end
        end


        function eixo = caractereEixo(~, indice)
            eixos = 'XYZ';
            eixo = eixos(indice);
        end


        function mascara = converterEixosParaMascara(~, texto)
            texto = upper(char(texto));
            mascara = 0;

            if contains(texto, 'X')
                mascara = bitor(mascara, 1);
            end

            if contains(texto, 'Y')
                mascara = bitor(mascara, 2);
            end

            if contains(texto, 'Z')
                mascara = bitor(mascara, 4);
            end
        end


    end
end
