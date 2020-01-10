
#include "as504x.h"
#include "bc_timer.h"

typedef struct _bc_as504x_t
{
    bool use_hardware_spi;
    bc_gpio_channel_t cs;
    bc_gpio_channel_t clk;
    bc_gpio_channel_t miso;
} _bc_as504x_t;

_bc_as504x_t _bc_as504x;

void bc_as504x_init(void)
{
    bc_spi_init(BC_SPI_SPEED_1_MHZ, BC_SPI_MODE_3);
    _bc_as504x.use_hardware_spi = true;
}

void bc_as504x_init_sw_spi(bc_gpio_channel_t cs, bc_gpio_channel_t clk, bc_gpio_channel_t miso)
{
    _bc_as504x.use_hardware_spi = false;

    _bc_as504x.cs = cs;
    _bc_as504x.clk = clk;
    _bc_as504x.miso = miso;

    bc_module_sensor_init();
    bc_module_sensor_set_vdd(true);

    bc_gpio_init(_bc_as504x.cs);
    bc_gpio_init(_bc_as504x.clk);
    bc_gpio_init(_bc_as504x.miso);

    bc_gpio_set_mode(_bc_as504x.cs, BC_GPIO_MODE_OUTPUT);
    bc_gpio_set_mode(_bc_as504x.clk, BC_GPIO_MODE_OUTPUT);
    bc_gpio_set_mode(_bc_as504x.miso, BC_GPIO_MODE_INPUT);

    bc_gpio_set_output(_bc_as504x.cs, 1);
    bc_gpio_set_output(_bc_as504x.clk, 1);

    bc_timer_init();
}

void bc_as504x_init_sw_spi_deinit()
{
    bc_module_sensor_set_vdd(false);

    bc_gpio_set_output(_bc_as504x.cs, 0);
    bc_gpio_set_output(_bc_as504x.clk, 0);
}

uint8_t shift_in(bc_gpio_channel_t clk, bc_gpio_channel_t data)
{
    uint8_t ret = 0;

    for (int i = 0; i < 8; i++)
    {

        bc_gpio_set_output(clk, 0);

        bc_timer_start();
        bc_timer_delay(5);
        bc_timer_stop();

        ret <<= 1;
        ret |= bc_gpio_get_input(data);

        bc_gpio_set_output(clk, 1);

        bc_timer_start();
        bc_timer_delay(5);
        bc_timer_stop();

    }

    return ret;
}

int bc_as504x_get_angle(void)
{
    uint8_t receive[3];

    if (_bc_as504x.use_hardware_spi)
    {
        bc_spi_transfer(NULL, receive, 3);
    }
    else
    {
        bc_gpio_set_output(_bc_as504x.cs, 0);
        receive[0] = shift_in(_bc_as504x.clk, _bc_as504x.miso);
        receive[1] = shift_in(_bc_as504x.clk, _bc_as504x.miso);
        receive[2] = shift_in(_bc_as504x.clk, _bc_as504x.miso);
        bc_gpio_set_output(_bc_as504x.cs, 1);
    }

    int angle = receive[0] << 5 | receive[1] >> 3;
    return angle ;
}
