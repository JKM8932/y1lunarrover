#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "Ultrasound.h"

// --- Configuration ---
// ADC connected to GP26
#define ADC_INPUT_PIN 26
#define ADC_INPUT_CHANNEL 0

// Calibration settings
#define CALIBRATION_TIME_S 5
#define SAMPLES 50

// Detection thresholds above the calibrated baseline
#define WEAK_MARGIN 100
#define DETECT_MARGIN 250

#define POLL_DELAY_MS 300

static bool ultrasound_calibrated = false;
static bool ultrasound_detection_active = false;

static uint16_t baseline = 0;
static uint16_t weak_threshold = 0;
static uint16_t detect_threshold = 0;

static void read_adc_average(uint16_t *avg_out, uint16_t *spread_out) {
    uint32_t sum = 0;
    uint16_t min_value = 4095;
    uint16_t max_value = 0;

    for (int i = 0; i < SAMPLES; i++) {
        uint16_t reading = adc_read();

        sum += reading;

        if (reading < min_value) {
            min_value = reading;
        }

        if (reading > max_value) {
            max_value = reading;
        }

        sleep_ms(2);
    }

    *avg_out = sum / SAMPLES;
    *spread_out = max_value - min_value;
}

// automatic calibration
// assumes no rock/ultrasonic signal is present
static void ultrasound_calibrate(void) {
    printf("Calibrating... keep rock OFF/away\n");

    uint32_t calibration_sum = 0;
    uint32_t calibration_count = 0;

    absolute_time_t start = get_absolute_time();

    while (absolute_time_diff_us(start, get_absolute_time()) <
           CALIBRATION_TIME_S * 1000000) {
        uint16_t avg = 0;
        uint16_t spread = 0;

        adc_select_input(ADC_INPUT_CHANNEL);
        read_adc_average(&avg, &spread);

        calibration_sum += avg;
        calibration_count++;

        printf("Calibrating: %u\n", avg);
    }

    baseline = calibration_sum / calibration_count;

    weak_threshold = baseline + WEAK_MARGIN;
    detect_threshold = baseline + DETECT_MARGIN;

    ultrasound_calibrated = true;

    printf("Calibration complete\n");
    printf("Baseline: %u\n", baseline);
    printf("Weak threshold: %u\n", weak_threshold);
    printf("Detect threshold: %u\n", detect_threshold);
    printf("----------------------\n");
}

void ultrasound_init(void) {
    adc_init();
    adc_gpio_init(ADC_INPUT_PIN);
    adc_select_input(ADC_INPUT_CHANNEL);

    ultrasound_calibrated = false;
    ultrasound_detection_active = false;

    printf("--- Ultrasound Engine Initialized ---\n");

    ultrasound_calibrate();   // calibrate once at startup
}

void ultrasound_start_detection(void) {
    if (!ultrasound_calibrated) {
        printf("Ultrasound not calibrated yet\n");
        return;
    }

    ultrasound_detection_active = true;

    printf("Ultrasound detection started\n");
}

// Detect and classify ultrasonic signal strength when requested
// Return values: 0 = inactive, 1 = no signal, 2 = weak signal, 3 = detected
int ultrasound_poll(void) {
    if (!ultrasound_detection_active) {
        return 0;
    }

    uint16_t avg = 0;
    uint16_t spread = 0;

    adc_select_input(ADC_INPUT_CHANNEL);
    read_adc_average(&avg, &spread);

    float voltage = avg * 3.3f / 4095.0f;

    printf("ADC avg: %u\n", avg);
    printf("Voltage: %.3f V\n", voltage);
    printf("Spread: %u\n", spread);

    // stops after one detection
    ultrasound_detection_active = false;

    if (avg >= detect_threshold) {
        printf("40kHz SIGNAL DETECTED\n");
        printf("----------------------\n");
        return 3;
    }

    if (avg >= weak_threshold) {
        printf("Weak / uncertain signal\n");
        printf("----------------------\n");
        return 2;
    }

    printf("No 40kHz signal\n");
    printf("----------------------\n");

    return 1;
}
