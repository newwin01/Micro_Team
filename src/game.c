#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include "led.h"

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
    !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
    ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
    DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
                         DT_SPEC_AND_COMMA)
};

static const int ADC_MAX = 1023;
static const int AXIS_DEVIATION = ADC_MAX / 2;
int32_t nowX = 0;
int32_t nowY = 0;

// Game constants
#define PADDLE_HEIGHT 2
#define PADDLE_WIDTH 1
#define BALL_SIZE 1
#define FIELD_WIDTH 8
#define FIELD_HEIGHT 8
#define PADDLE1_START 3
#define PADDLE2_START 3
#define BALL_START_X 4
#define BALL_START_Y 4

// Game state
int paddle1_y = PADDLE1_START;
int paddle2_y = PADDLE2_START;
int ball_x = BALL_START_X;
int ball_y = BALL_START_Y;
int ball_dir_x = 1; // 1 for right, -1 for left
int ball_dir_y = 1; // 1 for down, -1 for up

void draw_game(void)
{
    led_off_all();

    // Draw paddles
    for (int i = 0; i < PADDLE_HEIGHT; i++) {
        led_on(paddle1_y + i, 0);
        led_on(paddle2_y + i, FIELD_WIDTH - 1);
    }

    // Draw ball
    led_on(ball_y, ball_x);
}

void update_ball(void)
{
    ball_x += ball_dir_x;
    ball_y += ball_dir_y;

    // Ball collision with top or bottom
    if (ball_y <= 0 || ball_y >= FIELD_HEIGHT - BALL_SIZE) {
        ball_dir_y = -ball_dir_y;
    }

    // Ball collision with paddles
    if (ball_x == 1 && ball_y >= paddle1_y && ball_y < paddle1_y + PADDLE_HEIGHT) {
        ball_dir_x = -ball_dir_x;
    } else if (ball_x == FIELD_WIDTH - 2 && ball_y >= paddle2_y && ball_y < paddle2_y + PADDLE_HEIGHT) {
        ball_dir_x = -ball_dir_x;
    }

    // Ball out of bounds (scoring)
    if (ball_x <= 0 || ball_x >= FIELD_WIDTH - 1) {
        // Reset ball position
        ball_x = BALL_START_X;
        ball_y = BALL_START_Y;
    }
}

void update_paddle(int32_t joystick_val, int *paddle_y)
{
    if (joystick_val < AXIS_DEVIATION - 50 && *paddle_y > 0) {
        (*paddle_y)--;
    } else if (joystick_val > AXIS_DEVIATION + 50 && *paddle_y < FIELD_HEIGHT - PADDLE_HEIGHT) {
        (*paddle_y)++;
    }
}

int main(void)
{
    int err;
    uint16_t buf;
    struct adc_sequence sequence = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
    };

    /* Configure channels individually prior to sampling. */
    for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
        if (!adc_is_ready_dt(&adc_channels[i])) {
            printk("ADC controller device %s not ready\n", adc_channels[i].dev->name);
            return 0;
        }

        err = adc_channel_setup_dt(&adc_channels[i]);
        if (err < 0) {
            printk("Could not setup channel #%d (%d)\n", i, err);
            return 0;
        }
    }

    led_init();

    while (1) {
        // Read joystick for player 1
        (void)adc_sequence_init_dt(&adc_channels[0], &sequence);
        err = adc_read(adc_channels[0].dev, &sequence);
        if (err < 0) {
            printk("Could not read (%d)\n", err);
            continue;
        }
        nowY = (int32_t)buf;
        update_paddle(nowY, &paddle1_y);

        // Read joystick for player 2
        (void)adc_sequence_init_dt(&adc_channels[1], &sequence);
        err = adc_read(adc_channels[1].dev, &sequence);
        if (err < 0) {
            printk("Could not read (%d)\n", err);
            continue;
        }
        nowY = (int32_t)buf;
        update_paddle(nowY, &paddle2_y);

        // Update ball position
        update_ball();

        // Draw game state
        draw_game();

        k_sleep(K_MSEC(100));
    }
    return 0;
}
