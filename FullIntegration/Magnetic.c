#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"

// setting up the sensor
#define MAG_ADC_PIN 27
#define MAG_ADC_NUM 1

#define FILTER_SIZE 10
#define THRESHOLD 40
#define CONFIRM_COUNT 5
#define IDLE_CONFIRM_COUNT 50

#define CALIBRATION_SAMPLES 400
#define CALIBRATION_DELAY_MS 5

static uint16_t readings[FILTER_SIZE];
static int read_index = 0;
static uint32_t total = 0;
static uint16_t baseline = 0;

static bool magnetic_calibrated = false;
static bool magnetic_detection_active = false;

static int up_count = 0;
static int down_count = 0;
static int idle_count = 0;

// moving average filter
// takes sum of last n readings and averages them
static uint16_t get_filtered_reading(void) {
    total -= readings[read_index];
    readings[read_index] = adc_read();
    total += readings[read_index];
    read_index = (read_index + 1) % FILTER_SIZE;
    return total / FILTER_SIZE;
}

// clears and refills the moving average buffer using current ADC readings
static void reset_filter_buffer(void) {
    adc_select_input(MAG_ADC_NUM);

    read_index = 0;
    total = 0;

    for (int i = 0; i < FILTER_SIZE; i++) {
        uint16_t val = adc_read();
        readings[i] = val;
        total += val;
        sleep_ms(5);
    }
}

// calibrates sensor to establish baseline
// as the baseline voltage might change based on the environment
static void magnetic_calibrate(void) {
    printf("[MAG] Calibrating baseline... Keep magnets away from sensor!\n");

    adc_select_input(MAG_ADC_NUM);

    reset_filter_buffer();

    uint32_t sum = 0;

    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        sum += get_filtered_reading();
        sleep_ms(CALIBRATION_DELAY_MS);
    }

    baseline = sum / CALIBRATION_SAMPLES;
    magnetic_calibrated = true;

    printf("[MAG] Calibration successful! Base ADC level: %u\n", baseline);
}

// initialisation - resets all counters and the buffer
void magnetic_init(void) {
    adc_init();
    adc_gpio_init(MAG_ADC_PIN);
    adc_select_input(MAG_ADC_NUM);

    reset_filter_buffer();

    magnetic_detection_active = false;
    magnetic_calibrated = false;

    up_count = 0;
    down_count = 0;
    idle_count = 0;

    printf("[MAG] Magnetic engine initialized\n");
}

// runs when the button is pressed
void magnetic_start_detection(void) {
    // Recalibrate every time before detection starts
    magnetic_calibrate();

    up_count = 0;
    down_count = 0;
    idle_count = 0;

    magnetic_detection_active = true;

    printf("[MAG] Magnetic detection started\n");
}

int magnetic_poll(void) {
    if (!magnetic_detection_active) {
        return 0;
    }

    adc_select_input(MAG_ADC_NUM);

    uint16_t current_avg = get_filtered_reading();
    int delta = (int)current_avg - (int)baseline;

    printf("[MAG] ADC: %u | Base: %u | Delta: %d\n",
           current_avg, baseline, delta);

    if (delta < THRESHOLD) { // records an UP if delta < threshold (inverting amplifier)
        up_count++;
        down_count = 0;
        idle_count = 0;
    }
    else if (delta > -THRESHOLD) { // records DOWN if delta > threshold
        down_count++;
        up_count = 0;
        idle_count = 0;
    }
    else {
        idle_count++;
        up_count = 0;
        down_count = 0;
    }

    if (up_count >= CONFIRM_COUNT) {
        up_count = 0;
        down_count = 0;
        idle_count = 0;

        magnetic_detection_active = false;

        printf("[MAG] Direction: UP\n");
        return 3;
    }

    if (down_count >= CONFIRM_COUNT) {
        up_count = 0;
        down_count = 0;
        idle_count = 0;

        magnetic_detection_active = false;

        printf("[MAG] Direction: DOWN\n");
        return 1;
    }

    if (idle_count >= IDLE_CONFIRM_COUNT) {
        up_count = 0;
        down_count = 0;
        idle_count = 0;

        magnetic_detection_active = false;

        printf("[MAG] Direction: IDLE\n");
        return 2;
    }

    return 0;
}
