Instrucciones de compilación:
(No varia con respecto a la tarea 3)
Se incluye make file para compilar con el comando make.
bwcs se ejecuta ./bwcs localhost 2001 2000 agregando el flag -d para debug

Resultados:
Para probar el correcto funcionamiento de la implementación realizada se
envía la imagen a.jpg del cliente al servidor, pasando por los
proxys, cambiando el porcentaje de pérdida a 0%, 1%, 10% y 20% y el delay en 0ms,
100ms y 300ms, utilizando tanto los flags proporcionados para bwss como el
comando tc qdisc add/change dev lo root netem loss L% delay Dms (con 0.0 <= L <= 100.0 y 0 <= D). Se presentan los resultados obtenidos con este último comando para algunas de las combinaciones.

L=0.0% D=0ms
----bwc:
write 718544 bytes in 0.000836134 seconds at 6556.43 Mbps
read 721000 bytes in 2.01155 seconds at 2.73461 Mbps

throughput total: 1442000 bytes in 2.54723 seconds at 4.31904 Mbps

----bws:
read 718544 bytes in 29.7539 seconds at 0.184246 Mbps, 514 packs
write 718544 bytes in 0.000730991 seconds at 7499.48 Mbps, 513 packs
===============================================================================
L=1.0% D=200ms
----bwc:
write 718544 bytes in 0.00251317 seconds at 2181.33 Mbps
read 721000 bytes in 2.00201 seconds at 2.74764 Mbps

throughput total: 1442000 bytes in 5.60289 seconds at 1.96355 Mbps

----bws:
read 718544 bytes in 8.72503 seconds at 0.628314 Mbps, 514 packs
write 718544 bytes in 0.00069499 seconds at 7887.96 Mbps, 513 packs
===============================================================================
L=20.0% D=100ms
----bwc:
write 718544 bytes in 0.000690937 seconds at 7934.23 Mbps
read 721000 bytes in 9.71622 seconds at 0.566145 Mbps

throughput total: 1442000 bytes in 16.3301 seconds at 0.673698 Mbps

----bws:
read 718544 bytes in 13.5273 seconds at 0.405258 Mbps, 514 packs
write 718544 bytes in 0.000746965 seconds at 7339.1 Mbps, 513 packs
===============================================================================
L=0.0% D=300ms
----bwc:
write 718544 bytes in 0.000746012 seconds at 7348.48 Mbps
read 718544 bytes in 1.80157 seconds at 3.04294 Mbps

throughput total: 1437088 bytes in 7.81054 seconds at 1.40376 Mbps

----bws:
read 718544 bytes in 11.4483 seconds at 0.478855 Mbps, 514 packs
write 718544 bytes in 0.000720978 seconds at 7603.64 Mbps, 513 packs
===============================================================================
L=20.0% D=0ms
write 718544 bytes in 0.000825882 seconds at 6637.82 Mbps
read 721000 bytes in 6.84804 seconds at 0.803265 Mbps

throughput total: 1442000 bytes in 10.092 seconds at 1.09013 Mbps

----bws:
read 718544 bytes in 11.6837 seconds at 0.469207 Mbps, 514 packs
write 718544 bytes in 0.000751972 seconds at 7290.24 Mbps, 513 packs
===============================================================================

