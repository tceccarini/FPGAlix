// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>

#include "sensor.h"

int FPGAlix_sensor_init(void)
{
	/* TODO: I2C OV7670 register configuration */
	/* TODO: enable FPGA acquisition module */
	return 0;
}

void FPGAlix_sensor_release(void)
{
	/* TODO: disable FPGA acquisition module */
}
