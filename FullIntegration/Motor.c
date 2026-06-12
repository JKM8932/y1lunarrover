// standard modules
#include <stdlib.h>

// pico libraries
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"

// project modules
#include "Motor.h"

// pins for motor driver
#define LEFT_ENABLE   13
#define LEFT_DIR      12
#define RIGHT_ENABLE  11
#define RIGHT_DIR     10

#define PWM_WRAP 999

static uint left_slice,  left_channel;
static uint right_slice, right_channel;

// configuring direction pins and PWM output for motor driver
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
    pwm_config_set_wrap(&cfg, PWM_WRAP);
    pwm_config_set_clkdiv(&cfg, 6.25f);  // about 20 kHz PWM

    pwm_init(left_slice,  &cfg, true);
    pwm_init(right_slice, &cfg, true);

    pwm_set_chan_level(left_slice,  left_channel, 0);
    pwm_set_chan_level(right_slice, right_channel, 0);
}


static void set_motor(uint slice, uint channel, uint dir_pin, int speed_percentage, bool forward) {
    // set motor direction
    if (forward) {
        gpio_put(dir_pin, 1);
    } else {
        gpio_put(dir_pin, 0);
    }

    // clamp speed commands to valid range
    if (speed_percentage > 100) {
        speed_percentage = 100;
    }
    if (speed_percentage < 0) {
        speed_percentage = 0;
    }

    // convert speed percentage into PWM duty level
    uint duty = (uint)((speed_percentage / 100.0f) * PWM_WRAP);
    pwm_set_chan_level(slice, channel, duty);
}


// driver function to make the wheels do what is specified by "M {forward} {turn} {speed}" format
void motor_write(int forward, int turn, int speed_percentage) {
    int left_val  = forward - turn;
    int right_val = forward + turn;

    // extra validation for values being written to motors
    // can only have forward(1) / backward(-1) / stop(0)
    if (left_val  >  1) left_val  =  1;
    if (left_val  < -1) left_val  = -1;
    if (right_val >  1) right_val =  1;
    if (right_val < -1) right_val = -1;

    // differential drive - can drive left and right motors independently
    set_motor(left_slice,  left_channel,  LEFT_DIR, abs(left_val) * speed_percentage, left_val >= 0);
    set_motor(right_slice, right_channel, RIGHT_DIR, abs(right_val) * speed_percentage, right_val >= 0);
}
