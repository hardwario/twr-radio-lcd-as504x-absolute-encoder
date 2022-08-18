#include <application.h>
#include "as504x.h"

/*

Sensor Module Connection

CS      TWR_GPIO_P4  A
CLK     TWR_GPIO_P5  B
MISO    TWR_GPIO_P7  C

Supported sensors:
AS5045
AS5040

*/

#define SLEEP_TIMER_MS (2 * 60 * 60 * 1000)

// LED instance
twr_led_t led;

// Button instance
twr_button_t button;

int angle_current;
int angle_last;
int revolutions;
float combined;

twr_tick_t radio_timestamp;
float radio_value;

bool sleeping = false;
twr_scheduler_task_id_t lcd_task_id;
twr_scheduler_task_id_t uart_task_id;
twr_scheduler_task_id_t as504x_task_id;
twr_scheduler_task_id_t radio_task_id;

twr_scheduler_task_id_t sleep_task_id;


// Pointer to GFX instance
twr_gfx_t *pgfx;

void as504x_task(void *param);
void radio_task(void *param);
void sleep_task(void *param);

void start_lcd(void);
void start_uart(void);

void sleep_enable(bool sleep)
{
    sleeping = sleep;

    if(sleeping)
    {
        twr_as504x_init_sw_spi_deinit();

        // LCD is stopped in the task
        //twr_scheduler_plan_absolute(lcd_task_id, TWR_TICK_INFINITY);

        twr_scheduler_plan_absolute(as504x_task_id, TWR_TICK_INFINITY);
        twr_scheduler_plan_absolute(radio_task_id, TWR_TICK_INFINITY);
    }
    else
    {
        twr_as504x_init_sw_spi(TWR_GPIO_P4, TWR_GPIO_P5, TWR_GPIO_P7);

        twr_scheduler_plan_now(lcd_task_id);
        if(uart_task_id)
        {
            twr_scheduler_plan_now(uart_task_id);
        }
        twr_scheduler_plan_now(as504x_task_id);
        twr_scheduler_plan_now(radio_task_id);

        twr_scheduler_plan_relative(sleep_task_id, SLEEP_TIMER_MS);
    }
}

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    if (event == TWR_BUTTON_EVENT_PRESS)
    {
        sleeping = !sleeping;
        sleep_enable(sleeping);
    }

    // Logging in action
    //twr_log_info("Button event handler - event: %i", event);
}

void application_init(void)
{
    // Initialize logging
    //twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_pulse(&led, 2000);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);
    twr_radio_pairing_request("magnetic-encoder", FW_VERSION);

    //twr_as504x_init();
    //twr_as504x_init_sw_spi(TWR_GPIO_P15, TWR_GPIO_P14, TWR_GPIO_P12);

    // Sensor module
    twr_as504x_init_sw_spi(TWR_GPIO_P4, TWR_GPIO_P5, TWR_GPIO_P7);

    angle_current = angle_last = twr_as504x_get_angle();

    as504x_task_id = twr_scheduler_register(as504x_task, NULL, 0);
    radio_task_id = twr_scheduler_register(radio_task, NULL, 0);

    sleep_task_id = twr_scheduler_register(sleep_task, NULL, SLEEP_TIMER_MS);


    start_lcd();
    //start_uart();
}


void lcd_task(void *param)
{
    if (!twr_gfx_display_is_ready(pgfx))
    {
        twr_scheduler_plan_current_from_now(50);
        return;
    }

    // if we are sleeping, clear lcd and stop task
    if(sleeping)
    {
        twr_gfx_clear(pgfx);
        twr_gfx_update(pgfx);
        return;
    }

    twr_system_pll_enable();


    twr_gfx_clear(pgfx);

    twr_gfx_set_font(pgfx, &twr_font_ubuntu_24);
    twr_gfx_printf(pgfx, 20, 2, 1, "%4.3f", combined);


    int center_x = 64;
    int center_y = 74;
    int radius = 42;

    #define PI 3.14159

    double angle = 360.0f - (((float)angle_current / 4096.0f) * 360.0f);
    double radianVal = angle * (PI/180);

    twr_gfx_draw_line(pgfx, center_x, center_y, center_x + sin(radianVal) * radius, center_y + cos(radianVal) * radius, 1 );

    twr_gfx_draw_circle(pgfx, center_x, center_y, radius, 1);

    twr_gfx_update(pgfx);

    twr_system_pll_disable();

    twr_scheduler_plan_current_from_now(50);
}

void start_lcd(void)
{

    lcd_task_id = twr_scheduler_register(lcd_task, NULL, 0);

    twr_module_lcd_init();
    pgfx = twr_module_lcd_get_gfx();
}

void uart_task(void *param)
{
    (void) param;

    if(sleeping)
    {
        return;
    }

    //twr_log_debug("angle: %4.d, rot: %4.d, comb:%4.3f", angle_current, revolutions, combined);
    char line[32];
    snprintf(line, sizeof(line), "%4.3f\n", combined);
    twr_uart_write(TWR_UART_UART2, line, strlen(line));
    twr_scheduler_plan_current_from_now(50);
}

void start_uart(void)
{
    uart_task_id = twr_scheduler_register(uart_task, NULL, 0);
}

void radio_task(void *param)
{
    (void) param;

    float hysteresis = 0.5f;

    if( (combined >= (radio_value + hysteresis) ||
        combined <= (radio_value - hysteresis)) &&
        twr_tick_get() >= radio_timestamp + 1000)
    {
        radio_timestamp = twr_tick_get();
        radio_value = combined;
        twr_radio_pub_float("encoder/-/rotation", &radio_value);
    }

    twr_scheduler_plan_current_from_now(100);
}

void as504x_task(void *param)
{
    (void) param;

    angle_current = twr_as504x_get_angle();

    if(angle_last < 1024 && angle_current > 1024 * 3)
    {
        revolutions--;
    }
    else if(angle_last > 1024 * 3 && angle_current < 1024)
    {
        revolutions++;
    }

    angle_last = angle_current;

    combined = (float)revolutions + ((float)angle_current / 4096);

    twr_scheduler_plan_current_from_now(10);
}


// Sleep after some time
void sleep_task(void *param)
{
    sleep_enable(true);
}
