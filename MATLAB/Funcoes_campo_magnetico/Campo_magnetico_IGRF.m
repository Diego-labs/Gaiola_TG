function [Bn, Be, Bd, Btotal] = ...
    Campo_magnetico_IGRF(latitude, longitude, altitude, ano_decimal)

%--------------------------------------------------------------
%
% Campo magnético terrestre utilizando o modelo IGRF
% Aerospace Toolbox
%
% latitude  -> graus
% longitude -> graus
% altitude  -> km
%
% Saídas em uT
%
%--------------------------------------------------------------

    % altitude em metros
    altitude = altitude*1000;

    ano_decimal = ano_decimal*ones(size(latitude));
       
    % cálculo do campo (resultado em nT)
    XYZ = igrfmagm(altitude, latitude, longitude, ano_decimal);
    
    % conversão para uT
    XYZ = XYZ/1000;
    
    Bn = XYZ(:,1);
    Be = XYZ(:,2);
    Bd = XYZ(:,3);
    
    Btotal = sqrt(Bn.^2 + Be.^2 + Bd.^2);

end