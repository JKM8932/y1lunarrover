// standard libraries
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// pico libraries
#include "pico/stdlib.h"
#include "hardware/adc.h"

// project module
#include "Magnetic.h"

// configuration
#define MAG_ADC_PIN 27
#define MAG_ADC_NUM 1

#define FILTER_SIZE 10
#define THRESHOLD 5
#define CONFIRM_COUNT 7
#define IDLE_CONFIRM_COUNT 10

#define CALIBRATION_SAMPLES 600
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

// uses an updating running sum and average to reduce the influence of noise at ADC port
static uint16_t get_filtered_reading(void) {
    total -= readings[read_index];
    readings[read_index] = adc_read();
    total += readings[read_index];
    read_index = (read_index + 1) % FILTER_SIZE;
    return total / FILTER_SIZE;
}

// calibration, runs on startup when there is no magnet nearby, sets baseline ADC value
static void magnetic_calibrate(void) {
    printf("[MAG] Calibrating baseline... Keep magnets away from sensor!\n");
    adc_select_input(MAG_ADC_NUM);
    uint32_t sum = 0;

    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        sum += get_filtered_reading();
        sleep_ms(CALIBRATION_DELAY_MS);
    }

    baseline = sum / CALIBRATION_SAMPLES;
    magnetic_calibrated = true;

    printf("[MAG] Calibration successful! Base ADC level: %u\n", baseline);
}

// pin setup
void magnetic_init(void) {
    adc_init();
    adc_gpio_init(MAG_ADC_PIN);
    adc_select_input(MAG_ADC_NUM);

    read_index = 0;
    total = 0;

    for (int i = 0; i < FILTER_SIZE; i++) {
        uint16_t val = adc_read();
        readings[i] = val;
        total += val;
        sleep_ms(5);
    }

    magnetic_detection_active = false;
    magnetic_calibrated = false;
    up_count = 0;
    down_count = 0;

    printf("[MAG] Magnetic engine initialized\n");

    magnetic_calibrate();
}

// does not start until calibration is started and finished
void magnetic_start_detection(void) {
    if (!magnetic_calibrated) {
        printf("[MAG] Not calibrated yet\n");
        return;
    }

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

    if (delta > THRESHOLD) { // if sensor reading is above baseline by threshold amount or more, consider instance as Up
        up_count++;
        down_count = 0;
        idle_count = 0;
    }
    else if (delta < -THRESHOLD) { // if sensor reading is below baseline by threshold amount or more, consider instance as Down
        down_count++;
        up_count = 0;
        idle_count = 0;
    }
    else { // if unable to cross thresholds (too far or no magnet), consider instance as IDLE (no magnet near)
        idle_count++;
        up_count = 0;
        down_count = 0;
    }

    if (up_count >= CONFIRM_COUNT) { // confirm Up once Up instance has been recorded enough times
        up_count = 0;
        down_count = 0;
        idle_count = 0;

        magnetic_detection_active = false;

        printf("[MAG] Direction: UP\n");
        return 3;
    }

    if (down_count >= CONFIRM_COUNT) { // confirm Down once Down instance has been recorded enough times
        up_count = 0;
        down_count = 0;
        idle_count = 0;

        magnetic_detection_active = false;

        printf("[MAG] Direction: DOWN\n");
        return 1;
    }

    if (idle_count >= IDLE_CONFIRM_COUNT) { // confirm IDLE once IDLE instance has been recorded enough times
        up_count = 0;
        down_count = 0;
        idle_count = 0;

        magnetic_detection_active = false;

        printf("[MAG] Direction: IDLE\n");
        return 2;
    }

    // Still collecting evidence. No final result yet.
    return 0;
}

int main(void) {

    stdio_init_all();

    // Give USB serial time to connect
    sleep_ms(2000);

    printf("\n=== Magnetic Sensor Test ===\n");
    printf("Keep magnet away during calibration.\n");

    magnetic_init();

    while (true) {
        printf("\nPress/restart command: starting magnetic detection...\n");
        magnetic_start_detection();

        int result = 0;

        while (result == 0) {
            result = magnetic_poll();
            sleep_ms(50);
        }

        if (result == 3) {
            printf("FINAL RESULT: UP\n");
        }
        else if (result == 1) {
            printf("FINAL RESULT: DOWN\n");
        }
        else if (result == 2) {
            printf("FINAL RESULT: IDLE / NO MAGNET\n");
        }
        else {
            printf("FINAL RESULT: UNKNOWN CODE %d\n", result);
        }

        printf("Waiting 2 seconds before next test...\n");
        sleep_ms(2000);
    }

    return 0;
}
