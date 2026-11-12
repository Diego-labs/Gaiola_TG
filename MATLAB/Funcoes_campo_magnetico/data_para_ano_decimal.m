function ano_decimal = data_para_ano_decimal(data)

    dia = data(1);
    mes = data(2);
    ano = data(3);
    
    data0 = datetime(ano,1,1);
    data1 = datetime(ano,mes,dia);
    
    dias_ano = days(data1-data0);
    
    if mod(ano,4)==0
        total_dias = 366;
    else
        total_dias = 365;
    end
    
    ano_decimal = ano + dias_ano/total_dias;

end