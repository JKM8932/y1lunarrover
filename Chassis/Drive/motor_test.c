#include <stdio.h>
#include <cstdint>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"

// --- UPDATED: Pins shifted 1 value lower ---
#define LEFT_ENABLE  14  // Left speed (PWM)  -> Slice 7, Chan A
#define LEFT_DIR     15  // Left dir (GPIO)   -> (Sharing Slice 7, but used as pure GPIO)
#define RIGHT_ENABLE 16  // Right speed (PWM) -> Slice 0, Chan A
#define RIGHT_DIR    17  // Right dir (GPIO)  -> Slice 0, Chan B

// Track hardware registers
uint left_slice, left_channel;
uint right_slice, right_channel;

void init_hardware() {
    // 1. Configure DIRECTION Pins as pure standard GPIO outputs
    gpio_init(LEFT_DIR);
    gpio_set_dir(LEFT_DIR, GPIO_OUT);
    gpio_put(LEFT_DIR, 0);

    gpio_init(RIGHT_DIR);
    gpio_set_dir(RIGHT_DIR, GPIO_OUT);
    gpio_put(RIGHT_DIR, 0);

    // 2. Map ENABLE Pins specifically to the internal PWM hardware
    gpio_set_function(LEFT_ENABLE, GPIO_FUNC_PWM);
    gpio_set_function(RIGHT_ENABLE, GPIO_FUNC_PWM);

    // 3. Find slices and channels
    left_slice = pwm_gpio_to_slice_num(LEFT_ENABLE);
    left_channel = pwm_gpio_to_channel(LEFT_ENABLE);

    right_slice = pwm_gpio_to_slice_num(RIGHT_ENABLE);
    right_channel = pwm_gpio_to_channel(RIGHT_ENABLE);

    // 4. Setup configurations for Left Slice
    pwm_config config_left = pwm_get_default_config();
    pwm_config_set_wrap(&config_left, 25000);
    pwm_config_set_clkdiv(&config_left, 5.0f); // 125MHz / (25000 * 5) = 1kHz
    pwm_init(left_slice, &config_left, true);

    // 5. Setup configurations for Right Slice
    pwm_config config_right = pwm_get_default_config();
    pwm_config_set_wrap(&config_right, 25000);
    pwm_config_set_clkdiv(&config_right, 5.0f);
    pwm_init(right_slice, &config_right, true);

    // 6. Mandatory brief pause to let the hardware clock cycles synchronize
    sleep_us(50);
}

void set_motor_speed(uint slice, uint channel, uint dir_pin, int speed_percent, bool forward) {
    // Set direction line state
    gpio_put(dir_pin, forward ? 1 : 0);

    // Clamp bounds safely
    if (speed_percent > 100) speed_percent = 100;
    if (speed_percent < 0) speed_percent = 0;

    // Direct math map to the 25000 wrap limit
    uint duty_level = (uint)((speed_percent / 100.0f) * 25000);
    
    // Write directly to hardware channel registers
    pwm_set_chan_level(slice, channel, duty_level);
}

void stop_all_motors() {
    pwm_set_chan_level(left_slice, left_channel, 0);
    pwm_set_chan_level(right_slice, right_channel, 0);
}

int main() {
    stdio_init_all();
    
    // Run our robust config function
    init_hardware();
    stop_all_motors();
    
    sleep_ms(2000); // Give the VS Code USB Serial monitor time to bind
    printf("--- Pico W Motor Test Starting (Pins 14-17) ---\n");

    while (true) {
        // 1. Both forward at 50%
        printf("Both FORWARD at 50%...\n");
        set_motor_speed(left_slice, left_channel, LEFT_DIR, 50, true);
        set_motor_speed(right_slice, right_channel, RIGHT_DIR, 50, true);
        sleep_ms(2000);

        // 2. Both reverse at 50%
        printf("Both REVERSE at 50%...\n");
        set_motor_speed(left_slice, left_channel, LEFT_DIR, 50, false);
        set_motor_speed(right_slice, right_channel, RIGHT_DIR, 50, false);
        sleep_ms(2000);

        // 3. Spin Left
        printf("SPIN LEFT...\n");
        set_motor_speed(left_slice, left_channel, LEFT_DIR, 50, false);
        set_motor_speed(right_slice, right_channel, RIGHT_DIR, 50, true);
        sleep_ms(2000);

        // 4. Spin Right
        printf("SPIN RIGHT...\n");
        set_motor_speed(left_slice, left_channel, LEFT_DIR, 50, true);
        set_motor_speed(right_slice, right_channel, RIGHT_DIR, 50, false);
        sleep_ms(2000);

        // 5. Ramp up/down loop
        printf("RAMP UP...\n");
        for (int speed = 0; speed <= 100; speed += 20) {
            set_motor_speed(left_slice, left_channel, LEFT_DIR, speed, true);
            set_motor_speed(right_slice, right_channel, RIGHT_DIR, speed, true);
            printf("  Speed: %d%%\n", speed);
            sleep_ms(400);
        }

        printf("Stopping test rotation loop... Resting 5 seconds.\n");
        stop_all_motors();
        sleep_ms(5000);
    }

    return 0;
}
