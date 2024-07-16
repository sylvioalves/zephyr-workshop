#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "temperature.h"

LOG_MODULE_REGISTER(temp, LOG_LEVEL_ERR);

const struct device *const sensor = DEVICE_DT_GET(DT_INST(0, sensirion_shtc3));

int temp_init(void)
{
	if (!device_is_ready(sensor)) {
		LOG_ERR("SHTC3 sensor device %s is not ready", sensor->name);
		return -EIO;
	}

	return 0;
}

int temp_read(double *val)
{
	struct sensor_value temp;
	struct sensor_value hum;

	int rc = sensor_sample_fetch(sensor);
	if (rc) {
		LOG_ERR("Error %d: failed to fetch sample", rc);
		return -EIO;
	}

	rc = sensor_channel_get(sensor, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	if (rc) {
		LOG_ERR("Error %d: failed to get temperature", rc);
		return -EIO;
	}

	rc = sensor_channel_get(sensor, SENSOR_CHAN_HUMIDITY, &hum);
	if (rc) {
		LOG_ERR("Error %d: failed to get humidity", rc);
		return -EIO;
	}

	*val = sensor_value_to_double(&temp);

	return 0;
}
