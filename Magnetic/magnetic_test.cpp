#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

// Configuration
#define ADC_PIN 26          // GP26 is ADC0
#define ADC_NUM 0           // ADC0 channel
#define FILTER_SIZE 20      // Number of samples for the moving average filter
#define THRESHOLD 50        // Hysteresis threshold to prevent flickering near center

// Global variables for the moving average filter
uint16_t readings[FILTER_SIZE];
int read_index = 0;
uint32_t total = 0;

// Function to smooth out rover vibrations
uint16_t get_smoothed_adc() {
    // Subtract the oldest reading
    total -= readings[read_index];
    // Read the newest value from the sensor
    readings[read_index] = adc_read();
    // Add the newest value to the total
    total += readings[read_index];
    // Advance to the next position in the array
    read_index = (read_index + 1) % FILTER_SIZE;
    
    // Return the average
    return total / FILTER_SIZE;
}

int main() {
    stdio_init_all();
    
    // Initialize ADC hardware
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_NUM);
    
    // Initialize the filter array with baseline readings
    for (int i = 0; i < FILTER_SIZE; i++) {
        readings[i] = adc_read();
        total += readings[i];
        sleep_ms(5);
    }
    
    // Let the system settle and capture the quiescent (idle) center value
    printf("Calibrating baseline... Keep magnet away.\n");
    sleep_ms(1000);
    
    uint16_t baseline = 0;
    for(int i = 0; i < 50; i++) {
        baseline += get_smoothed_adc();
        sleep_ms(10);
    }
    baseline /= 50;
    printf("Baseline calibrated at ADC value: %d\n", baseline);
    
    while (true) {
        uint16_t current_val = get_smoothed_adc();
        
        // Determine direction based on deviation from the resting baseline
        if (current_val > (baseline + THRESHOLD)) {
            printf("Direction: UP   (ADC: %d)\n", current_val);
        } 
        else if (current_val < (baseline - THRESHOLD)) {
            printf("Direction: DOWN (ADC: %d)\n", current_val);
        } 
        else {
            printf("Direction: IDLE (ADC: %d)\n", current_val);
        }
        
        // Loop delay (adjust based on how fast your rover needs to react)
        sleep_ms(50); 
    }
}
