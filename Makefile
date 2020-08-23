check_bme280:check_bme280.c bme280.o
	gcc -Wall -O2 -o check_bme280 check_bme280.c bme280.o -lwiringPi -std=gnu99
bme280.o: bme280.c bme280.h bme280_defs.h
	gcc -Wall -c bme280.c -std=gnu99
clean:
	rm bme280.o check_bme280
