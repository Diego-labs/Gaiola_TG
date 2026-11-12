function [latitude, longitude, altitude] = ...
    Vetor_estado_para_geodesicas(vet_estado, t, tempo_sideral)

%----------------------------------------------------------
% Converte o vetor de estado propagado para:
%
% Latitude geodésica (graus)
% Longitude (graus)
% Altitude (km)
%
% Utiliza o elipsoide WGS84.
%----------------------------------------------------------

X = vet_estado(:,1);
Y = vet_estado(:,2);
Z = vet_estado(:,3);

N = length(X);

latitude  = zeros(N,1);
longitude = zeros(N,1);
altitude  = zeros(N,1);

% Rotação da Terra
omega_terra = 360/86164.0905;

% WGS84
a = 6378.137;                 % km
f = 1/298.257223563;
e2 = f*(2-f);

for k = 1:N

    %------------------------------------------
    % Rotação ECI -> ECEF

    theta = deg2rad(tempo_sideral + omega_terra*t(k));

    R3 = [ cos(theta)  sin(theta) 0;
        -sin(theta)  cos(theta) 0;
        0          0      1];

    r_ecef = R3*[X(k);Y(k);Z(k)];

    xe = r_ecef(1);
    ye = r_ecef(2);
    ze = r_ecef(3);

    %------------------------------------------
    % Longitude

    lon = atan2(ye,xe);

    %------------------------------------------
    % Latitude geodésica (método iterativo)

    p = sqrt(xe^2 + ye^2);

    lat = atan2(ze,p*(1-e2));

    erro = 1;

    while erro > 1e-12

        Nphi = a/sqrt(1-e2*sin(lat)^2);

        h = p/cos(lat)-Nphi;

        lat_new = atan2(ze,...
            p*(1-e2*Nphi/(Nphi+h)));

        erro = abs(lat_new-lat);

        lat = lat_new;

    end

    Nphi = a/sqrt(1-e2*sin(lat)^2);

    h = p/cos(lat)-Nphi;

    latitude(k)  = rad2deg(lat);
    longitude(k) = wrapTo180(rad2deg(lon));
    altitude(k)  = h;

end

end