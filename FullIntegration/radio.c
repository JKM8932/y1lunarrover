// standard libraries
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// pico libraries
#include "pico/stdlib.h"
#include "hardware/uart.h"

// project modules
#include "Radio.h"

//configuration for radio UART
#define RADIO_UART_ID uart1
#define RADIO_BAUD_RATE 600
#define RADIO_UART_RX_PIN 9


// variables for checking repeating message
#define message_MAX_LEN 128
#define REQUIRED_MATCH_COUNT 5

static char radio_message[message_MAX_LEN];
static char last_radio_message[message_MAX_LEN];
static char detected_message[message_MAX_LEN];

static int radio_message_len = 0;
static bool have_last_radio_message = false;
static int radio_match_count = 0;
static bool radio_detection_active = false;
static bool have_detected_message = false;

// to clear setup before detection
static void reset_radio_detection(void) {
    radio_message_len = 0;
    radio_match_count = 0;
    have_last_radio_message = false;
    have_detected_message = false;

    // setting each string to "" by putting null terminator at first char
    radio_message[0] = '\0';
    last_radio_message[0] = '\0';
    detected_message[0] = '\0';
}

// setting up pins for UART and calling reset function
void radio_init(void) {
    uart_init(RADIO_UART_ID, RADIO_BAUD_RATE);

    gpio_set_function(RADIO_UART_RX_PIN, GPIO_FUNC_UART);

    uart_set_format(RADIO_UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(RADIO_UART_ID, false, false);
    uart_set_fifo_enabled(RADIO_UART_ID, true);

    reset_radio_detection();

    // confirmation of successful initialisation
    printf("Radio UART started on RX GP%d at %d baud\n", RADIO_UART_RX_PIN, RADIO_BAUD_RATE);
}

void radio_start_detection(void) {
    reset_radio_detection();
    radio_detection_active = true;

    printf("Radio detection started\n");
}

// function to decode each byte that is received
static void process_radio_byte(uint8_t b) {
    printf("Radio RX byte: 0x%02X", b);

    // ASCII values < 32 or > 126 arent printable chars
    // so may cause issues if trying to print
    if (b >= 32 && b <= 126) {
        printf(" '%c'", b);
    }

    printf("\n");

    if (b == '#') { // start of new message and end of prev message
        if (radio_message_len > 0) {
            radio_message[radio_message_len] = '\0';

            printf("Radio message: %s\n", radio_message);
            
            // if matching previous message then increment counter for matching messages
            if (have_last_radio_message && strcmp(radio_message, last_radio_message) == 0) {
                radio_match_count++;
                printf("Radio match %d/%d\n",
                       radio_match_count,
                       REQUIRED_MATCH_COUNT);
                
                // if there are enough matches then output the result
                // protects against "one-off" bit corruption causing wrong message output
                // may take longer but will be more likely to give correct value
                if (radio_match_count >= REQUIRED_MATCH_COUNT) {
                    printf("5 repeated radio messages detected: %s\n", radio_message);

                    strcpy(detected_message, radio_message); // store stable message for return from radio_poll()
                    have_detected_message = true;
                    radio_detection_active = false;
                    return;
                }
            } else { // no match
                radio_match_count = 0;

                if (have_last_radio_message) {
                    printf("Different radio message, match count reset\n");
                }
            }

            strcpy(last_radio_message, radio_message); // store current message for next comparison
            have_last_radio_message = true;
        }

        radio_message_len = 0;
        return;
    }

    if (radio_message_len < message_MAX_LEN - 1) {
        radio_message[radio_message_len++] = (char)b;
    } else {
        printf("Radio message buffer full, resetting\n");
        radio_message_len = 0;
    }
}

// driver function for radio called repeatedly within main program
const char *radio_poll(void) {
    // early exit - dont want to waste resources by running main code if not trying to read radio
    if (!radio_detection_active) {
        return NULL;
    }

    while (uart_is_readable(RADIO_UART_ID)) {
        uint8_t b = uart_getc(RADIO_UART_ID);
        process_radio_byte(b);

        if (have_detected_message) {
            have_detected_message = false;
            return detected_message;
        }
    }

    return NULL;
}
