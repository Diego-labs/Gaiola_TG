function [latitude, longitude] = Vetor_estado_para_lat_long_ponto (vet_estado)
    X = vet_estado(1);
    Y = vet_estado(2);
    Z = vet_estado(3);
      
    r = norm(vet_estado);
    latitude = rad2deg(asin(Z/r));

    lon_deg = rad2deg(atan2(Y, X));
    longitude = wrapTo180(lon_deg);

end