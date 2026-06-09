#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "motor.h"

#define LEFT_ENABLE   13
#define LEFT_DIR      12
#define RIGHT_ENABLE  11
#define RIGHT_DIR     10

static uint left_slice,  left_channel;
static uint right_slice, right_channel;

void motor_init(void) {
    gpio_init(LEFT_DIR);
    gpio_set_dir(LEFT_DIR, GPIO_OUT);
    gpio_put(LEFT_DIR, 0);

    gpio_init(RIGHT_DIR);
    gpio_set_dir(RIGHT_DIR, GPIO_OUT);
    gpio_put(RIGHT_DIR, 0);

    gpio_set_function(LEFT_ENABLE,  GPIO_FUNC_PWM);
    gpio_set_function(RIGHT_ENABLE, GPIO_FUNC_PWM);

    left_slice    = pwm_gpio_to_slice_num(LEFT_ENABLE);
    left_channel  = pwm_gpio_to_channel(LEFT_ENABLE);
    right_slice   = pwm_gpio_to_slice_num(RIGHT_ENABLE);
    right_channel = pwm_gpio_to_channel(RIGHT_ENABLE);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, 25000);
    pwm_config_set_clkdiv(&cfg, 5.0f);

    pwm_init(left_slice,  &cfg, true);
    pwm_init(right_slice, &cfg, true);

    pwm_set_chan_level(left_slice,  PWM_CHAN_A, 0);
    pwm_set_chan_level(left_slice,  PWM_CHAN_B, 0);
    pwm_set_chan_level(right_slice, PWM_CHAN_A, 0);
    pwm_set_chan_level(right_slice, PWM_CHAN_B, 0);
}

static void set_motor(uint slice, uint channel, uint dir_pin,
                      int speed_pct, bool forward) {
    gpio_put(dir_pin, forward ? 1 : 0);
    if (speed_pct > 90) speed_pct = 90;
    if (speed_pct <  0) speed_pct = 0;
    uint duty = (uint)((speed_pct / 100.0f) * 25000);
    pwm_set_chan_level(slice, channel, duty);
}

void motor_write(int forward, int turn, int speed_pct) {
    int left_val  = forward - turn;
    int right_val = forward + turn;

    if (left_val  >  1) left_val  =  1;
    if (left_val  < -1) left_val  = -1;
    if (right_val >  1) right_val =  1;
    if (right_val < -1) right_val = -1;

    set_motor(left_slice,  left_channel,  LEFT_DIR,
              abs(left_val)  * speed_pct, left_val  >= 0);
    set_motor(right_slice, right_channel, RIGHT_DIR,
              abs(right_val) * speed_pct, right_val >= 0);
}
