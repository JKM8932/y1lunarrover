// standard libraries
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

// pico libraries
#include "pico/stdlib.h"
#include "hardware/gpio.h"

// project modules
#include "Infrared.h"

// configuration
#define IR_INPUT_PIN 15

#define SAMPLE_WINDOW_MS 100
#define COUNT_HISTORY_SIZE 5

#define DEBOUNCE_COOLDOWN_US 100

#define CLASSIFICATION_THRESHOLD 210
#define NO_SOURCE_THRESHOLD 50

#define CONFIRM_COUNT_547 2
#define CONFIRM_COUNT_312 15

static bool infrared_detection_active = false;

static uint32_t pulse_count = 0;
static uint32_t last_trigger_time = 0;

static bool previous_pin_level = false;

static uint32_t count_history[COUNT_HISTORY_SIZE] = {0};
static int count_history_index = 0;
static bool count_history_full = false;

static absolute_time_t last_window_time;

static int consecutive_312 = 0;
static int consecutive_547 = 0;

/*
 function which counts 1 possible edge per call
 and updates 100ms pulse count
*/
static void infrared_sample_pin(void) {
    if (!infrared_detection_active) {
        previous_pin_level = gpio_get(IR_INPUT_PIN);
        return;
    }

    bool current_pin_level = gpio_get(IR_INPUT_PIN);

    // Software rising-edge detection
    if (current_pin_level && !previous_pin_level) {
        uint32_t now_us = time_us_32();

        if ((uint32_t)(now_us - last_trigger_time) > DEBOUNCE_COOLDOWN_US) {
            pulse_count++;
            last_trigger_time = now_us;
        }
    }

    previous_pin_level = current_pin_level;
}

// pin setup
void infrared_init(void) {
    gpio_init(IR_INPUT_PIN);
    gpio_set_dir(IR_INPUT_PIN, GPIO_IN);

    infrared_detection_active = false;
    previous_pin_level = gpio_get(IR_INPUT_PIN);

    printf("--- Infrared Engine Initialized ---\n");
}

// resetting vars and history array for new detection
void infrared_start_detection(void) {
    pulse_count = 0;
    last_trigger_time = 0;

    for (int i = 0; i < COUNT_HISTORY_SIZE; i++) {
        count_history[i] = 0;
    }

    count_history_index = 0;
    count_history_full = false;

    consecutive_312 = 0;
    consecutive_547 = 0;

    previous_pin_level = gpio_get(IR_INPUT_PIN);
    last_window_time = get_absolute_time();

    infrared_detection_active = true;

    printf("Infrared detection started\n");
}

/*
 driver function for infrared that is called in the main program
*/
int infrared_poll(void) {
    // early exit - dont want to waste resources by running main code if not trying to read IR
    if (!infrared_detection_active) {
        return 0;
    }

    // counting pulses in 100ms window
    infrared_sample_pin();

    absolute_time_t now = get_absolute_time();

    /*
     the main loop calls infrared_poll() much faster than once every 100ms
     each call should still sample pin for edges but rolling count should only be
     updated after a full 100ms window
    */
    if (absolute_time_diff_us(last_window_time, now) < (SAMPLE_WINDOW_MS * 1000)) {
        return 0;
    }

    // storing pulse count from this window
    uint32_t count_snapshot = pulse_count;
    pulse_count = 0;

    count_history[count_history_index] = count_snapshot;

    /* 
     circular buffer stores counts in latest 5 windows
     every 5 count_history_index changes, wrap back around to 0
     exact order doesnt matter, only need values
    */
    count_history_index = (count_history_index + 1) % COUNT_HISTORY_SIZE;

    // have wrapped around
    if (count_history_index == 0) {
        count_history_full = true;
    }

    /*
     storing how many past values there are
     needed for when the loop starts and need to actually get 5 windows to begin with
    */
    int current_history_size;
    if (count_history_full) {
        current_history_size = COUNT_HISTORY_SIZE;
    } else {
        current_history_size = count_history_index;
    }

    // summing total in past 5 windows to get rolling sum
    uint32_t rolling_sum = 0;
    for (int i = 0; i < current_history_size; i++) {
        rolling_sum += count_history[i];
    }

    printf("[100ms] Count: %lu | 500ms Rolling Total: %lu\n", count_snapshot, rolling_sum);

    last_window_time = now;

    if (!count_history_full) {
        printf("VERDICT: CALIBRATING...\n");
        return 0;
    }

    // classification based on rolling sum
    if (rolling_sum < NO_SOURCE_THRESHOLD) { // no source
        consecutive_312 = 0;
        consecutive_547 = 0;
        return 0;
    }

    if (rolling_sum > CLASSIFICATION_THRESHOLD) { // that rolling sum is classified as 547
        consecutive_547++;
        consecutive_312 = 0;
        // if sufficient consecutive windows, final classification is 547
        if (consecutive_547 >= CONFIRM_COUNT_547) {
            printf("FINAL: 547 Hz\n");
            infrared_detection_active = false;
            return 547;
        }
    }
    else { // that rolling sum is classified as 312
        consecutive_312++;
        consecutive_547 = 0;
        // if sufficient consecutive windows, final classification is 312
        if (consecutive_312 >= CONFIRM_COUNT_312) {
            printf("FINAL: 312 Hz\n");
            infrared_detection_active = false;
            return 312;
        }
    }

    return 0;
}
