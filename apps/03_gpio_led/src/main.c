/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define SLEEP_TIME_MS 1000 /* 1000 msec = 1 sec */

/* The devicetree node identifier for the "led" alias. */
#define LED_NODE    DT_ALIAS(led)
#define BUTTON_NODE DT_ALIAS(button)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

static struct gpio_callback button_cb;

void button_cb_func(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button pressed at %d seconds\r\n", k_uptime_seconds());
	gpio_pin_toggle_dt(&led);
}

int main(void)
{
	int ret;

	/* LED configuration */
	if (!gpio_is_ready_dt(&led)) {
		return -EIO;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return -EIO;
	}

	/* Button configuration */
	if (!gpio_is_ready_dt(&button)) {
		return -EIO;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret < 0) {
		return -EIO;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		// LOG_ERR("Error %d: failed to configure interrupt on %s pin %d",
		// ret, button.port->name, button.pin);
		return -EIO;
	}

	gpio_init_callback(&button_cb, button_cb_func, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb);

	// while (1) {
	// 	ret = gpio_pin_toggle_dt(&led);
	// 	if (ret < 0) {
	// 		return -EIO;
	// 	}

	// 	k_msleep(SLEEP_TIME_MS);
	// }
}
