function [data, tempo, ano_decimal] = retorna_DataHora_TLE (tle)
    % fix -> trunca em direcao ao zero
    % rem -> retorna o resto da divisao

    % divide a primeira linha do TLE em partes usando o espaço como delimitador
    part_tle = strsplit(tle(1, :));

    % pega a string da epoca, que esta na quarta celula do cell array
    % chaves {} para acessar o conteúdo de uma célula
    epoca_str = part_tle{4};

    % pega os dois primeiros caracteres (ano) 
    ano = str2double(epoca_str(1:2)) + 2000;
    
    epoca_num = str2double(epoca_str);
    
    % a parte inteira eh o dia do ano
    dia_do_ano = fix(epoca_num);        % fix(25277.48592711) -> 25277
    dia_do_ano = rem(dia_do_ano, 1000); % rem(25277, 1000) -> 277
    
    % 25277.48592711 - 25277 -> 0.48592711
    fracao_do_dia = epoca_num - fix(epoca_num);
    
    % cria a data do inicio do ano (1 de janeiro)
    data_inicio_ano = datetime(ano, 1, 1);
    
    % adiciona o número de dias (dia 1 ja foi contado)
    data_completa = data_inicio_ano + days(dia_do_ano - 1);
    
    % adiciona a fracao do dia
    data_completa = data_completa + days(fracao_do_dia);
    
    % valores separados
    data_vetor = datevec(data_completa);
    mes = data_vetor(2);
    dia = data_vetor(3);
    hora = data_vetor(4);
    minuto = data_vetor(5);
    segundo = data_vetor(6);
    
    data = [dia mes ano];
    tempo = [hora minuto segundo];

    ano_decimal = decyear( ...
        data(3), ...
        data(2), ...
        data(1), ...
        tempo(1), ...
        tempo(2), ...
        tempo(3));

end