/*
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app);

int main(void)
{
	const struct device *const sensor = DEVICE_DT_GET(DT_INST(0, sensirion_shtc3));

	if (!device_is_ready(sensor)) {
		LOG_ERR("SHTC3 sensor device %s is not ready", sensor->name);
		return -EIO;
	}

	while (1) {
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

		LOG_INF("Temperature: %d.%06d C", temp.val1, temp.val2);
		LOG_INF("Humidity: %d.%06d %%", hum.val1, hum.val2);

		k_msleep(500);
	}
}
