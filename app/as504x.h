#ifndef _BC_as504x_H
#define _BC_as504x_H

#include "bc_spi.h"
#include "bc_gpio.h"
#include "bc_module_sensor.h"

void bc_as504x_init(void);
void bc_as504x_init_sw_spi(bc_gpio_channel_t cs, bc_gpio_channel_t clk, bc_gpio_channel_t miso);

int bc_as504x_get_angle(void);

#endif
