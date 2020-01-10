#include <application.h>
#include "as504x.h"

/*

Sensor Module Connection

CS      BC_GPIO_P4  A
CLK     BC_GPIO_P5  B
MISO    BC_GPIO_P7  C

Supported sensors:
AS5045
AS5040

*/

#define SLEEP_TIMER_MS (2 * 60 * 60 * 1000)

// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

int angle_current;
int angle_last;
int revolutions;
float combined;

bc_tick_t radio_timestamp;
float radio_value;

bool sleeping = false;
bc_scheduler_task_id_t lcd_task_id;
bc_scheduler_task_id_t uart_task_id;
bc_scheduler_task_id_t as504x_task_id;
bc_scheduler_task_id_t radio_task_id;

bc_scheduler_task_id_t sleep_task_id;


// Pointer to GFX instance
bc_gfx_t *pgfx;

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
        bc_as504x_init_sw_spi_deinit();

        // LCD is stopped in the task
        //bc_scheduler_plan_absolute(lcd_task_id, BC_TICK_INFINITY);

        bc_scheduler_plan_absolute(as504x_task_id, BC_TICK_INFINITY);
        bc_scheduler_plan_absolute(radio_task_id, BC_TICK_INFINITY);
    }
    else
    {
        bc_as504x_init_sw_spi(BC_GPIO_P4, BC_GPIO_P5, BC_GPIO_P7);

        bc_scheduler_plan_now(lcd_task_id);
        if(uart_task_id)
        {
            bc_scheduler_plan_now(uart_task_id);
        }
        bc_scheduler_plan_now(as504x_task_id);
        bc_scheduler_plan_now(radio_task_id);

        bc_scheduler_plan_relative(sleep_task_id, SLEEP_TIMER_MS);
    }
}

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    if (event == BC_BUTTON_EVENT_PRESS)
    {
        sleeping = !sleeping;
        sleep_enable(sleeping);
    }

    // Logging in action
    //bc_log_info("Button event handler - event: %i", event);
}

void application_init(void)
{
    // Initialize logging
    //bc_log_init(BC_LOG_LEVEL_DUMP, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_pulse(&led, 2000);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);
    bc_radio_pairing_request("magnetic-encoder", VERSION);

    //bc_as504x_init();
    //bc_as504x_init_sw_spi(BC_GPIO_P15, BC_GPIO_P14, BC_GPIO_P12);

    // Sensor module
    bc_as504x_init_sw_spi(BC_GPIO_P4, BC_GPIO_P5, BC_GPIO_P7);

    angle_current = angle_last = bc_as504x_get_angle();

    as504x_task_id = bc_scheduler_register(as504x_task, NULL, 0);
    radio_task_id = bc_scheduler_register(radio_task, NULL, 0);

    sleep_task_id = bc_scheduler_register(sleep_task, NULL, SLEEP_TIMER_MS);


    start_lcd();
    //start_uart();
}


void lcd_task(void *param)
{
    if (!bc_gfx_display_is_ready(pgfx))
    {
        bc_scheduler_plan_current_from_now(50);
        return;
    }

    // if we are sleeping, clear lcd and stop task
    if(sleeping)
    {
        bc_gfx_clear(pgfx);
        bc_gfx_update(pgfx);
        return;
    }

    bc_system_pll_enable();


    bc_gfx_clear(pgfx);

    bc_gfx_set_font(pgfx, &bc_font_ubuntu_24);
    bc_gfx_printf(pgfx, 20, 2, 1, "%4.3f", combined);


    int center_x = 64;
    int center_y = 74;
    int radius = 42;

    #define PI 3.14159

    double angle = 360.0f - (((float)angle_current / 4096.0f) * 360.0f);
    double radianVal = angle * (PI/180);

    bc_gfx_draw_line(pgfx, center_x, center_y, center_x + sin(radianVal) * radius, center_y + cos(radianVal) * radius, 1 );

    bc_gfx_draw_circle(pgfx, center_x, center_y, radius, 1);

    bc_gfx_update(pgfx);

    bc_system_pll_disable();

    bc_scheduler_plan_current_from_now(50);
}

void start_lcd(void)
{

    lcd_task_id = bc_scheduler_register(lcd_task, NULL, 0);

    bc_module_lcd_init();
    pgfx = bc_module_lcd_get_gfx();
}

void uart_task(void *param)
{
    (void) param;

    if(sleeping)
    {
        return;
    }

    //bc_log_debug("angle: %4.d, rot: %4.d, comb:%4.3f", angle_current, revolutions, combined);
    char line[32];
    snprintf(line, sizeof(line), "%4.3f\n", combined);
    bc_uart_write(BC_UART_UART2, line, strlen(line));
    bc_scheduler_plan_current_from_now(50);
}

void start_uart(void)
{
    uart_task_id = bc_scheduler_register(uart_task, NULL, 0);
}

void radio_task(void *param)
{
    (void) param;

    float hysteresis = 0.5f;

    if( (combined >= (radio_value + hysteresis) ||
        combined <= (radio_value - hysteresis)) &&
        bc_tick_get() >= radio_timestamp + 1000)
    {
        radio_timestamp = bc_tick_get();
        radio_value = combined;
        bc_radio_pub_float("encoder/-/rotation", &radio_value);
    }

    bc_scheduler_plan_current_from_now(100);
}

void as504x_task(void *param)
{
    (void) param;

    angle_current = bc_as504x_get_angle();

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

    bc_scheduler_plan_current_from_now(10);
}


// Sleep after some time
void sleep_task(void *param)
{
    sleep_enable(true);
}
