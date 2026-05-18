/* SPDX-License-Identifier: GPL-2.0 */
#ifndef FPGALIX_SENSOR_H
#define FPGALIX_SENSOR_H

/* Configure the OV7670 via I2C and enable the FPGA acquisition module.
 * Returns 0 on success, negative errno on failure. */
int  FPGAlix_sensor_init(void);

/* Disable the FPGA acquisition module. */
void FPGAlix_sensor_release(void);

#endif /* FPGALIX_SENSOR_H */
