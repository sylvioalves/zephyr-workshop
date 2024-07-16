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
	const struct device *const imu = DEVICE_DT_GET(DT_INST(0, invensense_icm42670_temp));

	if (!device_is_ready(imu)) {
		LOG_ERR("ICM42670 sensor device %s is not ready", imu->name);
		return -EIO;
	}

	while (1) {
		struct sensor_value accel[3], gyro[3];

		int rc = sensor_sample_fetch(imu);
		if (rc) {
			LOG_ERR("Error %d: failed to fetch sample", rc);
			return -EIO;
		}

		rc = sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, accel);
		if (rc) {
			LOG_ERR("Error %d: failed to get acceleration", rc);
			return -EIO;
		}

		rc = sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, gyro);
		if (rc) {
			LOG_ERR("Error %d: failed to get gyro", rc);
			return -EIO;
		}

		LOG_INF("Acceleration: x=%.2f y=%.2f z=%.2f", sensor_value_to_double(&accel[0]),
			sensor_value_to_double(&accel[1]), sensor_value_to_double(&accel[2]));

		LOG_INF("Gyro: x=%.2f y=%.2f z=%.2f", sensor_value_to_double(&gyro[0]),
			sensor_value_to_double(&gyro[1]), sensor_value_to_double(&gyro[2]));

		k_msleep(500);
	}
}
