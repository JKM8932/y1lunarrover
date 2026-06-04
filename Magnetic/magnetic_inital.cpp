#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

// Configuration
#define ADC_PIN 26          // GP26 (ADC0)
#define ADC_NUM 0           
#define FILTER_SIZE 7      // Size of the moving average filter buffer
#define THRESHOLD 25        // Trigger sensitivity window

static uint16_t readings[FILTER_SIZE];
static int read_index = 0;
static uint32_t total = 0;
static uint16_t baseline = 0;

// Helper function to maintain the moving average filter
uint16_t get_filtered_reading() {
    total -= readings[read_index];
    readings[read_index] = adc_read();
    total += readings[read_index];
    read_index = (read_index + 1) % FILTER_SIZE;
    
    return total / FILTER_SIZE;
}

// Single main entry point that the Pico SDK looks for
int main() {
    // Initialize standard I/O (USB serial communication)
    stdio_init_all();

    // 5-second countdown to let you open your Serial Monitor
    for (int i = 10; i > 0; i--) {
        printf("[MAG] Starting calibration in %d seconds...\n", i);
        sleep_ms(1000);
    }

    // Initialize the Pico ADC hardware
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_NUM);

    // Warm up the moving average buffer with live data
    for (int i = 0; i < FILTER_SIZE; i++) {
        uint16_t val = adc_read();
        readings[i] = val;
        total += val;
        sleep_ms(5);
    }

    // Dynamic Calibration: Sample the voltage divider 100 times to set "zero"
    printf("[MAG] Calibrating baseline... Keep magnets away from sensor!\n");
    uint32_t sum = 0;
    for(int i = 0; i < 1000; i++) {
        sum += get_filtered_reading();
        sleep_ms(10);
    }
    
    // Set our custom center reference point
    baseline = sum / 1000;
    printf("[MAG] Calibration successful! Base center ADC level set to: %d\n", baseline);

    // Primary execution loop
    while (true) {
        uint16_t current_avg = get_filtered_reading();

        // Evaluate live data against the dynamic baseline + safety threshold
        if (current_avg > (baseline + THRESHOLD)) {
            printf("[MAG] Direction: UP   (ADC: %d | Base: %d)\n", current_avg, baseline);
        } else if (current_avg < (baseline - THRESHOLD)) {
            printf("[MAG] Direction: DOWN (ADC: %d | Base: %d)\n", current_avg, baseline);
        } else {
            printf("[MAG] Direction: IDLE (ADC: %d | Base: %d)\n", current_avg, baseline);
        }

        // Loop pause interval
        sleep_ms(1000);
    }

    return 0;
}
