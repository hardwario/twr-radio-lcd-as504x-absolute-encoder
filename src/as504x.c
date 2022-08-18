
#include "as504x.h"
#include "twr_timer.h"

typedef struct _twr_as504x_t
{
    bool use_hardware_spi;
    twr_gpio_channel_t cs;
    twr_gpio_channel_t clk;
    twr_gpio_channel_t miso;
} _twr_as504x_t;

_twr_as504x_t _twr_as504x;

void twr_as504x_init(void)
{
    twr_spi_init(TWR_SPI_SPEED_1_MHZ, TWR_SPI_MODE_3);
    _twr_as504x.use_hardware_spi = true;
}

void twr_as504x_init_sw_spi(twr_gpio_channel_t cs, twr_gpio_channel_t clk, twr_gpio_channel_t miso)
{
    _twr_as504x.use_hardware_spi = false;

    _twr_as504x.cs = cs;
    _twr_as504x.clk = clk;
    _twr_as504x.miso = miso;

    twr_module_sensor_init();
    twr_module_sensor_set_vdd(true);

    twr_gpio_init(_twr_as504x.cs);
    twr_gpio_init(_twr_as504x.clk);
    twr_gpio_init(_twr_as504x.miso);

    twr_gpio_set_mode(_twr_as504x.cs, TWR_GPIO_MODE_OUTPUT);
    twr_gpio_set_mode(_twr_as504x.clk, TWR_GPIO_MODE_OUTPUT);
    twr_gpio_set_mode(_twr_as504x.miso, TWR_GPIO_MODE_INPUT);

    twr_gpio_set_output(_twr_as504x.cs, 1);
    twr_gpio_set_output(_twr_as504x.clk, 1);

    twr_timer_init();
}

void twr_as504x_init_sw_spi_deinit()
{
    twr_module_sensor_set_vdd(false);

    twr_gpio_set_output(_twr_as504x.cs, 0);
    twr_gpio_set_output(_twr_as504x.clk, 0);
}

uint8_t shift_in(twr_gpio_channel_t clk, twr_gpio_channel_t data)
{
    uint8_t ret = 0;

    for (int i = 0; i < 8; i++)
    {

        twr_gpio_set_output(clk, 0);

        twr_timer_start();
        twr_timer_delay(5);
        twr_timer_stop();

        ret <<= 1;
        ret |= twr_gpio_get_input(data);

        twr_gpio_set_output(clk, 1);

        twr_timer_start();
        twr_timer_delay(5);
        twr_timer_stop();

    }

    return ret;
}

int twr_as504x_get_angle(void)
{
    uint8_t receive[3];

    if (_twr_as504x.use_hardware_spi)
    {
        twr_spi_transfer(NULL, receive, 3);
    }
    else
    {
        twr_gpio_set_output(_twr_as504x.cs, 0);
        receive[0] = shift_in(_twr_as504x.clk, _twr_as504x.miso);
        receive[1] = shift_in(_twr_as504x.clk, _twr_as504x.miso);
        receive[2] = shift_in(_twr_as504x.clk, _twr_as504x.miso);
        twr_gpio_set_output(_twr_as504x.cs, 1);
    }

    int angle = receive[0] << 5 | receive[1] >> 3;
    return angle ;
}
