#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#define BOOT_CHECK_TIME_USEC 500 * 1000
#define NUM_BUTTONS 14
#define BUTTON_MIN_DOWN_MS 100
#define BUTTON_UNKNOWN_STATE -1
#define BUTTON_DOWN_STATE 0
#define BUTTON_UP_STATE 1

typedef struct button_s {
    int idx;
    int gpio_pin;
    // button led control
    int led_i2c_device;
    int led_i2c_mask;
    // map lighting control
    int map_i2c_device;
    int map_i2c_mask;
} button_t;

static int s_button_states[NUM_BUTTONS];
static unsigned int s_button_down_ts[NUM_BUTTONS];

static button_t s_buttons[] = {
    { 0,  0,  0x38, 1 << 0,  0x39, 1 << 0 },
    { 1,  1,  0x38, 1 << 1,  0x39, 1 << 1 },
    { 2,  2,  0x38, 1 << 2,  0x39, 1 << 2 },
    { 3,  3,  0x38, 1 << 3,  0x39, 1 << 3 },
    { 4,  4,  0x38, 1 << 4,  0x39, 1 << 4 },
    { 5,  5,  0x38, 1 << 5,  0x39, 1 << 5 },
    { 6,  6,  0x38, 1 << 6,  0x39, 1 << 6 },
    { 7,  7,  0x38, 1 << 7,  0x39, 1 << 7 },
    { 8,  8,  0x38, 1 << 8,  0x39, 1 << 8 },
    { 9,  9,  0x38, 1 << 9,  0x39, 1 << 9 },
    { 10, 10, 0x38, 1 << 10, 0x39, 1 << 10 },
    { 11, 11, 0x38, 1 << 11, 0x39, 1 << 11 },
    { 12, 12, 0x38, 1 << 12, 0x39, 1 << 12 },
    { 13, 13, 0x38, 1 << 13, 0x39, 1 << 13 },
};

static void play_sound(int idx) {
    char cmd[128];
    snprintf(cmd, 128, "aplay button_%d.wav", idx);
    system(cmd);
}

static void set_i2c_pin(int device, int mask) {
    int state = wiringPiI2CRead(device);
    int new_state = state | mask;
    wiringPiI2CWrite(device, mask);
}

static void unset_i2c_pin(int device, int mask) {
    int state = wiringPiI2CRead(device);
    int new_state = state & (~mask);
    wiringPiI2CWrite(device, mask);
}

static void button_press_impl(button_t* btn) {
    set_i2c_pin(btn->led_i2c_device, btn->led_i2c_mask);
    set_i2c_pin(btn->map_i2c_device, btn->map_i2c_mask);
    play_sound(btn->idx);
    unset_i2c_pin(btn->led_i2c_device, btn->led_i2c_mask);
    unset_i2c_pin(btn->map_i2c_device, btn->map_i2c_mask);
}

static void reset_button_states() {
    for (int i = 0; i < NUM_BUTTONS; ++i) {
        s_button_states[i] = BUTTON_UNKNOWN_STATE;
        s_button_down_ts[i] = 0;
    }
}

static void setup_buttons() {
    for (int i = 0; i < NUM_BUTTONS; ++i) {
        pinMode(s_buttons[i].gpio_pin, INPUT);
    }
}

static void poll_buttons() {
    for (int i = 0; i < NUM_BUTTONS; ++i) {
        int old_state = s_button_states[i];
        int new_state = digitalRead(s_buttons[i].gpio_pin);
        if (old_state == BUTTON_UNKNOWN_STATE) {
            // keep button as unknown when already down before unknown
            if (new_state == BUTTON_UP_STATE) {
                s_button_states[i] = BUTTON_UP_STATE;
            }
            continue;
        }

        if (old_state != new_state) {
            s_button_states[i] = new_state;
            if (new_state == BUTTON_DOWN_STATE) {
                s_button_down_ts[i] = millis();
            } else {
                s_button_down_ts[i] = 0;
            }
            continue;
        }

        if (new_state == BUTTON_UP_STATE) {
            continue;
        }

        // button was and is in down state. let's check since how long already
        int now = millis();
        if (s_button_down_ts[i] + BUTTON_MIN_DOWN_MS > now) {
            button_press_impl(&s_buttons[i]);
            reset_button_states();
            break;
        }
    }
}

static void boot_check() {
    for (int i = 0; i < NUM_BUTTONS; ++i) {
        unset_i2c_pin(s_buttons[i].led_i2c_device, s_buttons[i].led_i2c_mask);
        unset_i2c_pin(s_buttons[i].map_i2c_device, s_buttons[i].map_i2c_mask);
    }
    for (int i = 0; i < NUM_BUTTONS; ++i) {
        set_i2c_pin(s_buttons[i].led_i2c_device, s_buttons[i].led_i2c_mask);
        usleep(BOOT_CHECK_TIME_USEC);
        unset_i2c_pin(s_buttons[i].led_i2c_device, s_buttons[i].led_i2c_mask);
        usleep(BOOT_CHECK_TIME_USEC);
    }
    for (int i = 0; i < NUM_BUTTONS; ++i) {
        set_i2c_pin(s_buttons[i].map_i2c_device, s_buttons[i].map_i2c_mask);
        usleep(BOOT_CHECK_TIME_USEC);
        unset_i2c_pin(s_buttons[i].map_i2c_device, s_buttons[i].map_i2c_mask);
        usleep(BOOT_CHECK_TIME_USEC);
    }
}

int main() {
    wiringPiSetup();
    setup_buttons();
    reset_button_states();
    boot_check();
    for(;;) {
        poll_buttons();
        usleep(500);
    }
    return 0;
}
