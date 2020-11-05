/*
  Compile：make
  Run: ./check_bme280

  This Demo is tested on Raspberry PI 3B+
  you can use I2C or SPI interface to test this Demo
  When you use I2C interface, the default Address in this demo is 0X77
  When you use SPI interface, PIN 27 define SPI_CS
*/
#include "bme280.h"
#include <stdio.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>

//Raspberry 3B+ platform's default SPI channel
#define channel 0

//Default write it to the register in one time
#define USESPISINGLEREADWRITE 0

#include <string.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
//Raspberry 3B+ platform's default I2C device file
#define IIC_Dev  "/dev/i2c-1"

#define FAHRENHEIT 1
#define CELSIUS 2
#define	KELVIN 3
#define DEFAULT_TEMP_SCALE FAHRENHEIT

#define MODE_TEMPERATURE 1
#define MODE_PRESSURE 2
#define MODE_MOISTURE 3
#define MODE_HUMIDITY 3

#undef DEBUG

int fd;

void user_delay_ms(uint32_t period)
{
	usleep(period*1000);
}

int8_t user_i2c_read(uint8_t id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
	write(fd, &reg_addr, 1);
	read(fd, data, len);
	return 0;
}

int8_t user_i2c_write(uint8_t id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
  int8_t *buf = alloca(len + 1);
  buf[0] = reg_addr;
  memcpy(buf + 1, data, len);
  write(fd, buf, len + 1);
  return 0;
}

void usage(const char *cmd) {
	fprintf(stdout, "Usage: %s [-h] {-t|-p|-m} [-c <n>] [-w <n>] [-W <n>] [-C <n>]\n", cmd);
	fprintf(stdout, "   -h: print this help\n");
	fprintf(stdout, "   -t: check temperature\n");
	fprintf(stdout, "   -p: check pressure\n");
	fprintf(stdout, "   -m: check moisture\n");
	fprintf(stdout, "   -s <temperature scale>: fahrenheit, celsius, or kelvin\n");
	fprintf(stdout, "   -c <n>: numbers below this value are considered critical\n");
	fprintf(stdout, "   -w <n>: numbers below this value are considered a warning condition\n");
	fprintf(stdout, "   -W <n>: numbers above this value are considered a warning condition\n");
	fprintf(stdout, "   -C <n>: numbers above this value are considered critical\n");
}

int main(int argc, char* argv[])
{
	struct bme280_dev dev = {
		.dev_id = BME280_I2C_ADDR_SEC,
		.intf = BME280_I2C_INTF,
		.read = user_i2c_read,
		.write = user_i2c_write,
		.delay_ms = user_delay_ms,
		.settings = {
			.osr_h = BME280_OVERSAMPLING_1X,
			.osr_p = BME280_OVERSAMPLING_16X,
			.osr_t = BME280_OVERSAMPLING_2X,
			.filter = BME280_FILTER_COEFF_16,
			.standby_time = BME280_STANDBY_TIME_62_5_MS
		}
	};
	uint8_t settings_sel = BME280_STANDBY_SEL | BME280_FILTER_SEL;
	struct bme280_data comp_data;
	signed char opt;
	const char *perf = "temperature";
	int8_t mode = 0;
	float lcrit = -10000.0, lwarn = -10000.0, hcrit = 10000.0, hwarn = 10000.0, outval = -1.0;
	int res = 0;
	int scale = DEFAULT_TEMP_SCALE;

#ifdef DEBUG
	fprintf(stdout, "Starting...\n");
#endif

	while (-1 != (opt = getopt(argc, argv, "c:w:C:W:tpmhs:"))) {
#ifdef DEBUG
	fprintf(stdout, "Handling option %c (%d)\n", opt, opt);
#endif

		switch (opt) {
		case 'c': /* low critical level */
			if (sscanf(optarg, "%f", &lcrit) < 1) {
				usage(argv[0]);
				exit(1);
			}
			break;
		case 'C': /* high critical level */
			if (sscanf(optarg, "%f", &hcrit) < 1) {
				usage(argv[0]);
				exit(1);
			}
			break;

		case 'w': /* low warning level */
			if (sscanf(optarg, "%f", &lwarn) < 1) {
				usage(argv[0]);
				exit(1);
			}
			break;

		case 'W': /* high warning level */
			if (sscanf(optarg, "%f", &hwarn) < 1) {
				usage(argv[0]);
				exit(1);
			}
			break;

		case 't': /* temperature */
			mode = MODE_TEMPERATURE;
			break;
		case 'p': /* pressure */
			mode = MODE_PRESSURE;
			perf = "pressure";
			break;
		case 'm': /* moisture */
			mode = MODE_MOISTURE;
			perf = "humidity";
			break;
		case 's': /* temperature scale */
			switch (optarg[0]) {
			case 'f':
			case 'F':
				scale = FAHRENHEIT;
				perf = "fahrenheit";
				break;
			case 'c':
			case 'C':
				scale = CELSIUS;
				perf = "celsius";
				break;
			case 'k':
			case 'K':
				scale = KELVIN;
				perf = "kelvin";
				break;
			default:
				usage(argv[0]);
				exit(1);
			}
			break;
		case 'h': /* help */
			usage(argv[0]);
			exit(0);
		}
	}

#ifdef DEBUG
	fprintf(stdout, "Past options parsing\n");
#endif

	switch (mode) {
	case 0:
		usage(argv[0]);
		exit(1);
	case MODE_TEMPERATURE:
		settings_sel |= BME280_OSR_TEMP_SEL;
		break;
	case MODE_PRESSURE:
		settings_sel |= BME280_OSR_PRESS_SEL;
		break;
	case MODE_MOISTURE:
		settings_sel |= BME280_OSR_HUM_SEL;
		break;
	default:
		exit(1);
	}

#ifdef DEBUG
	fprintf(stdout, "Done setting the mode\n");
#endif

	if ((fd = open(IIC_Dev, O_RDWR)) < 0) {
		printf("Failed to open the i2c bus %s", argv[1]);
		exit(1);
	}
	if (ioctl(fd, I2C_SLAVE, 0x77) < 0) {
		printf("Failed to acquire bus access and/or talk to slave.\n");
		exit(1);
	}

#ifdef DEBUG
	fprintf(stdout, "Device open\n");
#endif

	bme280_init(&dev);
	bme280_set_sensor_settings(settings_sel, &dev);
	bme280_set_sensor_mode(BME280_NORMAL_MODE, &dev);

	/* Delay while the sensor completes a measurement */
	dev.delay_ms(70);
	bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);

#ifdef DEBUG
	fprintf(stdout, "Got sensor data\n");
#endif

	switch (mode) {
	case MODE_TEMPERATURE: /* temperature */
		switch (scale) {
		case FAHRENHEIT:
			outval = comp_data.temperature * 1.8 + 32.0;
			fprintf(stdout, "%.1f°F", outval);
			break;
		case CELSIUS:
			outval = comp_data.temperature;
			fprintf(stdout, "%.1f°C", outval);
			break;
		case KELVIN:
			outval = comp_data.temperature + 273.15;
			fprintf(stdout, "%.1fK", outval);
		}
		break;
	case MODE_PRESSURE:
		outval = comp_data.pressure / 100.0;
		fprintf(stdout, "%.0fmbs", outval);
		break;
	case MODE_MOISTURE:
		outval = comp_data.humidity;
		fprintf(stdout, "%.0f%%", outval);
	}

	if (outval < lcrit) {
		fprintf(stdout, " (＜%.1f)", lcrit);
		res = 2;
	} else if (outval < lwarn) {
		fprintf(stdout, " (＜%.1f)", lwarn);
		res = 1;
	} else if (outval > hwarn) {
		fprintf(stdout, " (＞%.1f)", hwarn);
		res = 1;
	} else if (outval > hcrit) {
		fprintf(stdout, " (＞%.1f)", hcrit);
		res = 2;
	}
	fprintf(stdout, " | %s=%.2f\n", perf, outval);
	return res;
}
