/*
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(app);

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 7

#define LED0_NODE DT_ALIAS(led)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

void led_task(void *args)
{
	const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

	if (!device_is_ready(led.port)) {
		printk("Error: %s device is not ready\n", led.port->name);
		return;
	}

	int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		return;
	}

	while (true) {
		gpio_pin_toggle_dt(&led);
		k_msleep(100);
	}
}

int main(void)
{
	LOG_INF("Hello World! %s", CONFIG_BOARD_TARGET);

	/* do nothing more */
}

K_THREAD_DEFINE(led_task_id, STACKSIZE, led_task, NULL, NULL, NULL, PRIORITY, 0, 0);
