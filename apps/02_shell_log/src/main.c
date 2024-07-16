/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app);

int main(void)
{
	LOG_INF("Hello World! %s", CONFIG_BOARD_TARGET);

	return 0;
}
