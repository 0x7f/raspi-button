#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#define NUM_BUTTONS 14
#define BUTTON_MIN_DOWN_MS 100

typedef struct button_s {
    int idx;
    // button gpio input
    int gpio_pin;
    int down_edge_type;
    int up_edge_type;
    unsigned int down_ts;
    void (*isr_down_fn)(void);
    void (*isr_up_fn)(void);
    // button led control
    int led_i2c_device;
    int led_i2c_mask;
    // map lighting control
    int map_i2c_device;
    int map_i2c_mask;
} button_t;

static volatile int s_button_active = 0;

static volatile button_t s_buttons[] = {
    {
        .gpio_pin = 0,
        .led_i2c_device = 0x38,
        .led_i2c_mask = 1 << 0,
        .map_i2c_device = 0x39,
        .map_i2c_mask = 1 << 0,
    },
    {
        .gpio_pin = 1,
        .led_i2c_device = 0x38,
        .led_i2c_mask = 1 << 1,
        .map_i2c_device = 0x39,
        .map_i2c_mask = 1 << 1,
    },
    {
        .gpio_pin = 2,
        .led_i2c_device = 0x38,
        .led_i2c_mask = 1 << 2,
        .map_i2c_device = 0x39,
        .map_i2c_mask = 1 << 2,
    },
    {
        .gpio_pin = 3,
        .led_i2c_device = 0x38,
        .led_i2c_mask = 1 << 3,
        .map_i2c_device = 0x39,
        .map_i2c_mask = 1 << 3,
    },
    {
        .gpio_pin = 4,
        .led_i2c_device = 0x38,
        .led_i2c_mask = 1 << 4,
        .map_i2c_device = 0x39,
        .map_i2c_mask = 1 << 4,
    },
    {
        .gpio_pin = 5,
        .led_i2c_device = 0x38,
        .led_i2c_mask = 1 << 5,
        .map_i2c_device = 0x39,
        .map_i2c_mask = 1 << 5,
    },
    {
        .gpio_pin = 6,
        .led_i2c_device = 0x38,
        .led_i2c_mask = 1 << 6,
        .map_i2c_device = 0x39,
        .map_i2c_mask = 1 << 6,
    },
    {
        .gpio_pin = 7,
        .led_i2c_device = 0x38,
        .led_i2c_mask = 1 << 7,
        .map_i2c_device = 0x39,
        .map_i2c_mask = 1 << 7,
    },
    {
        .gpio_pin = 8,
        .led_i2c_device = 0x38,
        .led_i2c_mask = 1 << 8,
        .map_i2c_device = 0x39,
        .map_i2c_mask = 1 << 8,
    },
    {
        .gpio_pin = 9,
        .led_i2c_device = 0x38,
        .led_i2c_mask = 1 << 9,
        .map_i2c_device = 0x39,
        .map_i2c_mask = 1 << 9,
    },
    {
        .gpio_pin = 10,
        .led_i2c_device = 0x38,
        .led_i2c_mask = 1 << 10,
        .map_i2c_device = 0x39,
        .map_i2c_mask = 1 << 10,
    },
    {
        .gpio_pin = 11,
        .led_i2c_device = 0x38,
        .led_i2c_mask = 1 << 11,
        .map_i2c_device = 0x39,
        .map_i2c_mask = 1 << 11,
    },
    {
        .gpio_pin = 12,
        .led_i2c_device = 0x38,
        .led_i2c_mask = 1 << 12,
        .map_i2c_device = 0x39,
        .map_i2c_mask = 1 << 12,
    },
    {
        .gpio_pin = 13,
        .led_i2c_device = 0x38,
        .led_i2c_mask = 1 << 13,
        .map_i2c_device = 0x39,
        .map_i2c_mask = 1 << 13,
    },
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

static void button_press_impl(volatile button_t* btn) {
    set_i2c_pin(btn->led_i2c_device, btn->led_i2c_mask);
    set_i2c_pin(btn->map_i2c_device, btn->map_i2c_mask);
    play_sound(btn->idx);
    unset_i2c_pin(btn->led_i2c_device, btn->led_i2c_mask);
    unset_i2c_pin(btn->map_i2c_device, btn->map_i2c_mask);
}

static void reset_button_states() {
    for (int i = 0; i < NUM_BUTTONS; ++i) {
        s_buttons[i].down_ts = 0;
    }
}

static void isr_button_down(volatile button_t* btn) {
    if (s_button_active) {
        return;
    }
    btn->down_ts = millis();
}

static void isr_button_up(volatile button_t* btn) {
    unsigned int now = millis();
    if (btn->down_ts + BUTTON_MIN_DOWN_MS > now || s_button_active) {
        btn->down_ts = 0;
        return;
    }

    // the button is really active and obviously the first to
    // get above BUTTON_MIN_DOWN_MS, so let's reset all buttons again

    s_button_active = 1;
    reset_button_states();
    button_press_impl(btn);
    s_button_active = 0;
}

#define DEFINE_BUTTON_FUNCTIONS(X) \
    static void isr_button_down_##X() { isr_button_down(&s_buttons[X]); } \
    static void isr_button_up_##X() { isr_button_up(&s_buttons[X]); }

DEFINE_BUTTON_FUNCTIONS(0)
DEFINE_BUTTON_FUNCTIONS(1)
DEFINE_BUTTON_FUNCTIONS(2)
DEFINE_BUTTON_FUNCTIONS(3)
DEFINE_BUTTON_FUNCTIONS(4)
DEFINE_BUTTON_FUNCTIONS(5)
DEFINE_BUTTON_FUNCTIONS(6)
DEFINE_BUTTON_FUNCTIONS(7)
DEFINE_BUTTON_FUNCTIONS(8)
DEFINE_BUTTON_FUNCTIONS(9)
DEFINE_BUTTON_FUNCTIONS(10)
DEFINE_BUTTON_FUNCTIONS(11)
DEFINE_BUTTON_FUNCTIONS(12)
DEFINE_BUTTON_FUNCTIONS(13)

static void init_buttons() {
    for (int i = 0; i < NUM_BUTTONS; ++i) {
        s_buttons[i].idx = i;
        s_buttons[i].down_ts = 0;
        s_buttons[i].down_edge_type = INT_EDGE_FALLING;
        s_buttons[i].up_edge_type = INT_EDGE_RISING;
    }
}

static void setup_buttons() {
    for (int i = 0; i < NUM_BUTTONS; ++i) {
        pinMode(s_buttons[i].gpio_pin, INPUT);
    }

#define ASSIGN_BUTTON_FUNCTIONS(X) \

    ASSIGN_BUTTON_FUNCTIONS(0)
    ASSIGN_BUTTON_FUNCTIONS(1)
    ASSIGN_BUTTON_FUNCTIONS(2)
    ASSIGN_BUTTON_FUNCTIONS(3)
    ASSIGN_BUTTON_FUNCTIONS(4)
    ASSIGN_BUTTON_FUNCTIONS(5)
    ASSIGN_BUTTON_FUNCTIONS(6)
    ASSIGN_BUTTON_FUNCTIONS(7)
    ASSIGN_BUTTON_FUNCTIONS(8)
    ASSIGN_BUTTON_FUNCTIONS(9)
    ASSIGN_BUTTON_FUNCTIONS(10)
    ASSIGN_BUTTON_FUNCTIONS(11)
    ASSIGN_BUTTON_FUNCTIONS(12)
    ASSIGN_BUTTON_FUNCTIONS(13)
}

static void register_isr() {
    int res;
    for (int i = 0; i < NUM_BUTTONS; ++i) {
        res = wiringPiISR(s_buttons[i].gpio_pin, s_buttons[i].down_edge_type, s_buttons[i].isr_down_fn);
        if (res != 0) {
            fprintf(stderr, "Error while registering isr down handler for button %d: %d\n", i, res);
            exit(1);
            return;
        }

        res = wiringPiISR(s_buttons[i].gpio_pin, s_buttons[i].up_edge_type, s_buttons[i].isr_up_fn);
        if (res != 0) {
            fprintf(stderr, "Error while registering isr up handler for button %d: %d\n", i, res);
            exit(1);
            return;
        }
    }
}

int main() {
    wiringPiSetup();
    init_buttons();
    setup_buttons();
    register_isr();
    for(;;) {
        sleep(1);
    }
    return 0;
}
