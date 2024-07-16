/*
 * Copyright (c) 2018 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <lvgl_input_device.h>

static uint32_t count;

static void lv_btn_click_callback(lv_event_t *e)
{
	ARG_UNUSED(e);

	printf("button count=%d\r\n", count++);
}

int main(void)
{
	char count_str[11] = {0};
	const struct device *display_dev;
	lv_obj_t *hello_world_label;
	lv_obj_t *count_label;

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		printf("Device not ready, aborting test\r\n");
		return 0;
	}

	lv_obj_t *hello_world_button;

	hello_world_button = lv_btn_create(lv_scr_act());
	lv_obj_align(hello_world_button, LV_ALIGN_CENTER, 0, -15);
	lv_obj_add_event_cb(hello_world_button, lv_btn_click_callback, LV_EVENT_CLICKED, NULL);
	hello_world_label = lv_label_create(hello_world_button);

	lv_label_set_text(hello_world_label, "Hello world!");
	lv_obj_align(hello_world_label, LV_ALIGN_CENTER, 0, 0);

	count_label = lv_label_create(lv_scr_act());
	lv_obj_align(count_label, LV_ALIGN_CENTER, 0, 50);

	lv_task_handler();
	display_blanking_off(display_dev);

	while (1) {
		sprintf(count_str, "%d", count);
		lv_label_set_text(count_label, count_str);
		lv_task_handler();
		k_sleep(K_MSEC(10));
	}
}
