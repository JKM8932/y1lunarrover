// standard libraries
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// pico libraries
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

// networking libraries
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

// project modules
#include "Radio.h"
#include "Infrared.h"
#include "Ultrasound.h"
#include "Magnetic.h"
#include "Motor.h"

// Wi-Fi UDP config
#define UDP_PORT 4242

#define WIFI_SSID "PicoTest"
#define WIFI_PASSWORD "picotest123"

#define MOTION_TIMEOUT_US 500000  // 0.5 seconds

static uint32_t last_motion_command_time = 0;
static bool have_received_motion_command = false;

static struct udp_pcb *udp_control_pcb;
static ip_addr_t last_client_addr;
static u16_t last_client_port = 0;
static bool have_client = false;




// send status or sensor result message back to latest Python controller client
static void transmit_to_controller(const char *msg) {
    if (!have_client) return;

    cyw43_arch_lwip_begin();

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, strlen(msg), PBUF_RAM);

    if (p) {
        memcpy(p->payload, msg, strlen(msg));
        udp_sendto(udp_control_pcb, p, &last_client_addr, last_client_port);
        pbuf_free(p);
    }

    cyw43_arch_lwip_end();
}


// stops motors if motion commands stop arriving
static void motion_timeout_poll(void) {
    if (!have_received_motion_command) return;

    uint32_t now = time_us_32();

    // if nothing received for past 0.5s, stop
    if ((int32_t)(now - last_motion_command_time) > MOTION_TIMEOUT_US) {
        motor_write(0, 0, 0);
        have_received_motion_command = false;
        printf("Motion timeout: motors stopped\n");
    }
}




// handling sensor commands
static void handle_sensor_command(const char *msg) {
    if (strcmp(msg, "S ULTRASOUND") == 0) {
        ultrasound_start_detection();
        transmit_to_controller("ULTRASOUND STARTED");
        transmit_to_controller("ULTRASOUND 40kHz/0");
    }

    else if (strcmp(msg, "S IR") == 0) {
        infrared_start_detection();
        transmit_to_controller("IR STARTED");
    }

    else if (strcmp(msg, "S MAGNETIC") == 0) {
        magnetic_start_detection();
        transmit_to_controller("MAGNETIC Up/Down");
    }

    else if (strcmp(msg, "S AGE") == 0) {
        radio_start_detection();
        transmit_to_controller("AGE STARTED");

    }

    else {
        transmit_to_controller("ERROR UNKNOWN SENSOR COMMAND");
    }
}


/*
 called by lwIP when a UDP packet is received from the controller
 copies the packet into a local string, stores the client address for replies,
 then dispatches motion/sensor command (or pong command if new connection)
*/
static void udp_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    if (!p) {
        return;
    }

    char msg[64];

    int len = p->len;
    if (len >= (int)sizeof(msg)) {
        len = sizeof(msg) - 1;
    }

    memcpy(msg, p->payload, len);
    msg[len] = '\0';

    last_client_addr = *addr;
    last_client_port = port;
    have_client = true;

    printf("UDP received: %s\n", msg);

    int forward = 0;
    int turn = 0;
    int speed = 90;

    // parsing the command
    if (sscanf(msg, "M %d %d %d", &forward, &turn, &speed) >= 2) {

        if (speed < 80 && (forward != 0 || turn != 0)) {
            speed = 80;
        }
        if  (forward == 0 && turn == 0) {
            speed = 0;
        }

        motor_write(forward, turn, speed);
        last_motion_command_time = time_us_32();
        have_received_motion_command = true;
        //printf("Motor command: forward=%d turn=%d speed=%d\n", forward, turn, speed);
    }

    else if (strncmp(msg, "S ", 2) == 0) {
        handle_sensor_command(msg);
    }

    else if (strcmp(msg, "PING") == 0) { // to check that connection is established
        transmit_to_controller("PONG");
    }

    else {
        transmit_to_controller("ERROR UNKNOWN COMMAND");
    }

    pbuf_free(p);
}



// main
int main() {
    stdio_init_all();

    sleep_ms(3000);

    printf("\nStarting Pico W UDP controller with radio detection\n");

    // initialising motor and sensor modules before starting networking
    motor_init();
    radio_init();
    infrared_init();
    ultrasound_init();
    magnetic_init();
    magnetic_start_detection();

    // early exit if Wi-Fi connection fails
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    printf("Connecting to Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID,
            WIFI_PASSWORD,
            CYW43_AUTH_WPA2_MIXED_PSK,
            30000
        )) {
        printf("Wi-Fi connection failed\n");
        return 1;
    }

    printf("IP Address: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
    printf("Connected to Wi-Fi\n");

    udp_control_pcb = udp_new();

    if (!udp_control_pcb) {
        printf("Failed to create UDP PCB\n");
        return 1;
    }

    if (udp_bind(udp_control_pcb, IP_ADDR_ANY, UDP_PORT) != ERR_OK) {
        printf("Failed to bind UDP port\n");
        return 1;
    }

    udp_recv(udp_control_pcb, udp_receive_callback, NULL);

    printf("Listening on UDP port %d\n", UDP_PORT);

    uint32_t last_heartbeat = time_us_32();

    // main polling loop
    while (true) {

        // handling radio
        const char *age_result = radio_poll();

        if (age_result != NULL) {
            char response[160];
            snprintf(response, sizeof(response), "AGE %s", age_result);
            transmit_to_controller(response);
        }


        // handling infrared
        int ir_result = infrared_poll();

        if (ir_result == 312) {
            transmit_to_controller("IR 312");
        }
        else if (ir_result == 547) {
            transmit_to_controller("IR 547");
        }

        // handling ultrasound
        int ultrasound_result = ultrasound_poll();

        if (ultrasound_result == 1) {
            transmit_to_controller("ULTRASOUND NONE");
        }
        else if (ultrasound_result == 2) {
            transmit_to_controller("ULTRASOUND WEAK");
        }
        else if (ultrasound_result == 3) {
            transmit_to_controller("ULTRASOUND 40kHz");
        }


        // handling magnetic
        int magnetic_result = magnetic_poll();

        if (magnetic_result == 1) {
            transmit_to_controller("MAGNETIC DOWN");
        }
        else if (magnetic_result == 2) {
            transmit_to_controller("MAGNETIC IDLE");
        }
        else if (magnetic_result == 3) {
            transmit_to_controller("MAGNETIC UP");
        }

        // handling motion timeout
        motion_timeout_poll();

        // "heartbeat" to show that pico is connected to network
        if ((int32_t)(time_us_32() - last_heartbeat) > 1000000) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, !cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN));

            last_heartbeat = time_us_32();
        }

        tight_loop_contents();
    }
}
