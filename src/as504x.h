#ifndef _TWR_as504x_H
#define _TWR_as504x_H

#include "twr_spi.h"
#include "twr_gpio.h"
#include "twr_module_sensor.h"

void twr_as504x_init(void);
void twr_as504x_init_sw_spi(twr_gpio_channel_t cs, twr_gpio_channel_t clk, twr_gpio_channel_t miso);

void twr_as504x_init_sw_spi_deinit();

int twr_as504x_get_angle(void);

#endif
