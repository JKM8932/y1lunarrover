#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

// --- Pin assignments (matching your motor test file) ---
#define LEFT_ENABLE   13
#define LEFT_DIR      12
#define RIGHT_ENABLE  11
#define RIGHT_DIR     10

#define UDP_PORT      4242
#define WIFI_SSID     "EEERover"
#define WIFI_PASSWORD "exhibition"

// --- Motor state ---
static uint left_slice,   left_channel;
static uint right_slice,  right_channel;

static struct udp_pcb *udp_control_pcb;
static ip_addr_t last_client_addr;
static u16_t last_client_port = 0;
static bool have_client = false;

// --- Hardware init (same logic as your motor test) ---
static void init_motors() {
    gpio_init(LEFT_DIR);
    gpio_set_dir(LEFT_DIR, GPIO_OUT);
    gpio_put(LEFT_DIR, 0);

    gpio_init(RIGHT_DIR);
    gpio_set_dir(RIGHT_DIR, GPIO_OUT);
    gpio_put(RIGHT_DIR, 0);

    gpio_set_function(LEFT_ENABLE,  GPIO_FUNC_PWM);
    gpio_set_function(RIGHT_ENABLE, GPIO_FUNC_PWM);

    left_slice   = pwm_gpio_to_slice_num(LEFT_ENABLE);
    left_channel = pwm_gpio_to_channel(LEFT_ENABLE);
    right_slice  = pwm_gpio_to_slice_num(RIGHT_ENABLE);
    right_channel = pwm_gpio_to_channel(RIGHT_ENABLE);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, 25000);
    pwm_config_set_clkdiv(&cfg, 5.0f);   // 125MHz / (25000 * 5) = 1 kHz

    pwm_init(left_slice,  &cfg, true);
    pwm_init(right_slice, &cfg, true);

    // Zero all channels, including the B channels on the dir-pin slices
    pwm_set_chan_level(left_slice,  PWM_CHAN_A, 0);
    pwm_set_chan_level(left_slice,  PWM_CHAN_B, 0);
    pwm_set_chan_level(right_slice, PWM_CHAN_A, 0);
    pwm_set_chan_level(right_slice, PWM_CHAN_B, 0);
}

// --- Set one motor: speed 0-100, direction as bool ---
static void set_motor(uint slice, uint channel, uint dir_pin,
                      int speed_pct, bool forward) {
    gpio_put(dir_pin, forward ? 1 : 0);
    if (speed_pct > 80) speed_pct = 80;
    if (speed_pct <   0) speed_pct = 0;
    uint duty = (uint)((speed_pct / 100.0f) * 25000);
    pwm_set_chan_level(slice, channel, duty);
}

// --- Differential drive mixer ---
// forward: -1 (back), 0 (stop), +1 (forward)
// turn:    -1 (left),  0 (straight), +1 (right)
static void motor_write(int forward, int turn) {
    // Mix: left = forward - turn,  right = forward + turn
    // This gives smooth curves when both keys are held
    int left_val  = forward - turn;
    int right_val = forward + turn;

    // Clamp to [-1, 1] range
    if (left_val  >  1) left_val  =  1;
    if (left_val  < -1) left_val  = -1;
    if (right_val >  1) right_val =  1;
    if (right_val < -1) right_val = -1;

    set_motor(left_slice,  left_channel,  LEFT_DIR,
              abs(left_val)  * 100, left_val  >= 0);
    set_motor(right_slice, right_channel, RIGHT_DIR,
              abs(right_val) * 100, right_val >= 0);
}

// --- UDP helpers ---
static void transmit_to_receiver(const char *msg) {
    if (!have_client) return;
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, strlen(msg), PBUF_RAM);
    if (!p) return;
    memcpy(p->payload, msg, strlen(msg));
    udp_sendto(udp_control_pcb, p, &last_client_addr, last_client_port);
    pbuf_free(p);
}

static void udp_receive_callback(void *arg, struct udp_pcb *pcb,
                                  struct pbuf *p, const ip_addr_t *addr,
                                  u16_t port) {
    if (!p) return;

    char msg[64] = {0};
    int len = p->len < (int)sizeof(msg) - 1 ? p->len : (int)sizeof(msg) - 1;
    memcpy(msg, p->payload, len);
    pbuf_free(p);

    last_client_addr = *addr;
    last_client_port = port;
    have_client = true;

    int fwd = 0, turn = 0;
    if (sscanf(msg, "M %d %d", &fwd, &turn) == 2) {
        motor_write(fwd, turn);
        printf("CMD: fwd=%d turn=%d\n", fwd, turn);
    }
}

int main() {
    stdio_init_all();

    init_motors();

    sleep_ms(3000);
    printf("Starting Pico W UDP motor controller...\n");

    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Connecting to Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                            CYW43_AUTH_WPA2_MIXED_PSK, 60000)) {
        printf("Wi-Fi connection failed\n");
        return 1;
    }

    printf("IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));

    udp_control_pcb = udp_new();
    udp_bind(udp_control_pcb, IP_ADDR_ANY, UDP_PORT);
    udp_recv(udp_control_pcb, udp_receive_callback, NULL);
    printf("Listening on UDP port %d\n", UDP_PORT);

    while (true) {
        cyw43_arch_poll();
        sleep_ms(5);
    }
}
