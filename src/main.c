/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Nordic UART Bridge Service (NUS) sample
 */

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <soc.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>

#include <bluetooth/services/nus.h>

#include <dk_buttons_and_leds.h>

#include <zephyr/settings/settings.h>

#include <stdio.h>

#include <zephyr/logging/log.h>

//led
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#define LOG_MODULE_NAME peripheral_uart
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define STACKSIZE CONFIG_BT_NUS_THREAD_STACK_SIZE
#define PRIORITY 7

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define RUN_STATUS_LED DK_LED1
#define CON_STATUS_LED DK_LED2

#define BUTTON1 DK_BTN1_MSK
#define BUTTON2 DK_BTN2_MSK
#define BUTTON3 DK_BTN3_MSK
#define BUTTON4 DK_BTN4_MSK

#define UART_BUF_SIZE CONFIG_BT_NUS_UART_BUFFER_SIZE
#define UART_WAIT_FOR_BUF_DELAY K_MSEC(50)
#define UART_WAIT_FOR_RX CONFIG_BT_NUS_UART_RX_WAIT_TIME

static K_SEM_DEFINE(ble_init_ok, 0, 1);

static struct bt_conn *current_conn;
static struct bt_conn *auth_conn;

static const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(nordic_nus_uart));
static struct k_work_delayable uart_work;

struct uart_data_t {
    void *fifo_reserved;
    uint8_t data[UART_BUF_SIZE];
    uint16_t len;
};

static K_FIFO_DEFINE(fifo_uart_tx_data);
static K_FIFO_DEFINE(fifo_uart_rx_data);

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

//led part
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

// add for joystick
int32_t preX = 0 , perY = 0;
static const int ADC_MAX = 1023;
// static const int ADC_MIN = 0;
static const int AXIS_DEVIATION = ADC_MAX / 2;
int32_t nowX = 0;
int32_t nowY = 0;

bool isChange(void)
{
    if((nowX < (preX - 50)) || nowX > (preX+50)){
        preX = nowX;
        return true;
    }

    if((nowY < (perY - 50)) || nowY > (perY+50)){
        perY = nowY;
        return true;
    }
    return false;
}

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
    ARG_UNUSED(dev);

    static size_t aborted_len;
    struct uart_data_t *buf;
    static uint8_t *aborted_buf;
    static bool disable_req;

    switch (evt->type) {
    case UART_TX_DONE:
        LOG_DBG("UART_TX_DONE");
        if ((evt->data.tx.len == 0) ||
            (!evt->data.tx.buf)) {
            return;
        }

        if (aborted_buf) {
            buf = CONTAINER_OF(aborted_buf, struct uart_data_t,
                       data);
            aborted_buf = NULL;
            aborted_len = 0;
        } else {
            buf = CONTAINER_OF(evt->data.tx.buf, struct uart_data_t,
                       data);
        }

        k_free(buf);

        buf = k_fifo_get(&fifo_uart_tx_data, K_NO_WAIT);
        if (!buf) {
            return;
        }

        if (uart_tx(uart, buf->data, buf->len, SYS_FOREVER_MS)) {
            LOG_WRN("Failed to send data over UART");
        }

        break;

    case UART_RX_RDY:
        LOG_DBG("UART_RX_RDY");
        buf = CONTAINER_OF(evt->data.rx.buf, struct uart_data_t, data);
        buf->len += evt->data.rx.len;

        if (disable_req) {
            return;
        }

        if ((evt->data.rx.buf[buf->len - 1] == '\n') ||
            (evt->data.rx.buf[buf->len - 1] == '\r')) {
            disable_req = true;
            uart_rx_disable(uart);
        }

        break;

    case UART_RX_DISABLED:
        LOG_DBG("UART_RX_DISABLED");
        disable_req = false;

        buf = k_malloc(sizeof(*buf));
        if (buf) {
            buf->len = 0;
        } else {
            LOG_WRN("Not able to allocate UART receive buffer");
            k_work_reschedule(&uart_work, UART_WAIT_FOR_BUF_DELAY);
            return;
        }

        uart_rx_enable(uart, buf->data, sizeof(buf->data),
                   UART_WAIT_FOR_RX);

        break;

    case UART_RX_BUF_REQUEST:
        LOG_DBG("UART_RX_BUF_REQUEST");
        buf = k_malloc(sizeof(*buf));
        if (buf) {
            buf->len = 0;
            uart_rx_buf_rsp(uart, buf->data, sizeof(buf->data));
        } else {
            LOG_WRN("Not able to allocate UART receive buffer");
        }

        break;

    case UART_RX_BUF_RELEASED:
        LOG_DBG("UART_RX_BUF_RELEASED");
        buf = CONTAINER_OF(evt->data.rx_buf.buf, struct uart_data_t,
                   data);

        if (buf->len > 0) {
            k_fifo_put(&fifo_uart_rx_data, buf);
        } else {
            k_free(buf);
        }

        break;

    case UART_TX_ABORTED:
        LOG_DBG("UART_TX_ABORTED");
        if (!aborted_buf) {
            aborted_buf = (uint8_t *)evt->data.tx.buf;
        }

        aborted_len += evt->data.tx.len;
        buf = CONTAINER_OF(aborted_buf, struct uart_data_t,
                   data);

        uart_tx(uart, &buf->data[aborted_len],
            buf->len - aborted_len, SYS_FOREVER_MS);

        break;

    default:
        break;
    }
}

static void uart_work_handler(struct k_work *item)
{
    struct uart_data_t *buf;

    buf = k_malloc(sizeof(*buf));
    if (buf) {
        buf->len = 0;
    } else {
        LOG_WRN("Not able to allocate UART receive buffer");
        k_work_reschedule(&uart_work, UART_WAIT_FOR_BUF_DELAY);
        return;
    }

    uart_rx_enable(uart, buf->data, sizeof(buf->data), UART_WAIT_FOR_RX);
}

static int uart_init(void)
{
    int err;
    int pos;
    struct uart_data_t *rx;
    struct uart_data_t *tx;

    if (!device_is_ready(uart)) {
        return -ENODEV;
    }

    if (IS_ENABLED(CONFIG_USB_DEVICE_STACK)) {
        err = usb_enable(NULL);
        if (err && (err != -EALREADY)) {
            LOG_ERR("Failed to enable USB");
            return err;
        }
    }

    rx = k_malloc(sizeof(*rx));
    if (rx) {
        rx->len = 0;
    } else {
        return -ENOMEM;
    }

    k_work_init_delayable(&uart_work, uart_work_handler);


    err = uart_callback_set(uart, uart_cb, NULL);
    if (err) {
        k_free(rx);
        LOG_ERR("Cannot initialize UART callback");
        return err;
    }

    if (IS_ENABLED(CONFIG_UART_LINE_CTRL)) {
        LOG_INF("Wait for DTR");
        while (true) {
            uint32_t dtr = 0;

            uart_line_ctrl_get(uart, UART_LINE_CTRL_DTR, &dtr);
            if (dtr) {
                break;
            }
            /* Give CPU resources to low priority threads. */
            k_sleep(K_MSEC(100));
        }
        LOG_INF("DTR set");
        err = uart_line_ctrl_set(uart, UART_LINE_CTRL_DCD, 1);
        if (err) {
            LOG_WRN("Failed to set DCD, ret code %d", err);
        }
        err = uart_line_ctrl_set(uart, UART_LINE_CTRL_DSR, 1);
        if (err) {
            LOG_WRN("Failed to set DSR, ret code %d", err);
        }
    }

    tx = k_malloc(sizeof(*tx));

    if (tx) {
        pos = snprintf(tx->data, sizeof(tx->data),
                   "Starting Nordic UART service example\r\n");

        if ((pos < 0) || (pos >= sizeof(tx->data))) {
            k_free(rx);
            k_free(tx);
            LOG_ERR("snprintf returned %d", pos);
            return -ENOMEM;
        }

        tx->len = pos;
    } else {
        k_free(rx);
        return -ENOMEM;
    }

    err = uart_tx(uart, tx->data, tx->len, SYS_FOREVER_MS);
    if (err) {
        k_free(rx);
        k_free(tx);
        LOG_ERR("Cannot display welcome message (err: %d)", err);
        return err;
    }

    err = uart_rx_enable(uart, rx->data, sizeof(rx->data), 50);
    if (err) {
        LOG_ERR("Cannot enable uart reception (err: %d)", err);
        /* Free the rx buffer only because the tx buffer will be handled in the callback */
        k_free(rx);
    }

    return err;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("Connected %s", addr);

    current_conn = bt_conn_ref(conn);

    dk_set_led_on(CON_STATUS_LED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Disconnected: %s (reason %u)", addr, reason);

    if (auth_conn) {
        bt_conn_unref(auth_conn);
        auth_conn = NULL;
    }

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
        dk_set_led_off(CON_STATUS_LED);
    }
}


BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected    = connected,
    .disconnected = disconnected
};


static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data,
              uint16_t len)
{
    int err;
    char addr[BT_ADDR_LE_STR_LEN] = {0};

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));

    LOG_INF("Received data from: %s", addr);

    for (uint16_t pos = 0; pos != len;) {
        struct uart_data_t *tx = k_malloc(sizeof(*tx));

        if (!tx) {
            LOG_WRN("Not able to allocate UART send data buffer");
            return;
        }

        /* Keep the last byte of TX buffer for potential LF char. */
        size_t tx_data_size = sizeof(tx->data) - 1;

        if ((len - pos) > tx_data_size) {
            tx->len = tx_data_size;
        } else {
            tx->len = (len - pos);
        }

        memcpy(tx->data, &data[pos], tx->len);

        pos += tx->len;

        /* Append the LF character when the CR character triggered
         * transmission from the peer.
         */
        if ((pos == len) && (data[len - 1] == '\r')) {
            tx->data[tx->len] = '\n';
            tx->len++;
        }

        err = uart_tx(uart, tx->data, tx->len, SYS_FOREVER_MS);
        if (err) {
            k_fifo_put(&fifo_uart_tx_data, tx);
        }
    }
}

static struct bt_nus_cb nus_cb = {
    .received = bt_receive_cb,
};

void error(void)
{
    dk_set_leds_state(DK_ALL_LEDS_MSK, DK_NO_LEDS_MSK);

    while (true) {
        /* Spin for ever */
        k_sleep(K_MSEC(1000));
    }
}

void button_changed(uint32_t button_state, uint32_t has_changed)
{
    uint32_t buttons = button_state & has_changed;

    if (buttons & BUTTON1) {
        char buf[7] = "Button1\n";
        bt_nus_send(NULL, buf, sizeof(buf));
    }

    if (buttons & BUTTON2) {
        char buf[7] = "Button2\n";
        bt_nus_send(NULL, buf, sizeof(buf));
    }

    if (buttons & BUTTON3) {
        char buf[7] = "Button3\n";
        bt_nus_send(NULL, buf, sizeof(buf));
    }

    if (buttons & BUTTON4) {
        char buf[7] = "Button4\n";
        bt_nus_send(NULL, buf, sizeof(buf));
    }

}

static void configure_gpio(void)
{
    int err;

    err = dk_buttons_init(button_changed);
    if (err) {
        LOG_ERR("Cannot init buttons (err: %d)", err);
    }

    err = dk_leds_init();
    if (err) {
        LOG_ERR("Cannot init LEDs (err: %d)", err);
    }
}

int main(void)
{
    int blink_status = 0;
    int err = 0;

    configure_gpio();

    err = uart_init();
    if (err) {
        error();
    }

    err = bt_enable(NULL);
    if (err) {
        error();
    }

    LOG_INF("Bluetooth initialized");

    k_sem_give(&ble_init_ok);

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    err = bt_nus_init(&nus_cb);
    if (err) {
        LOG_ERR("Failed to initialize UART service (err: %d)", err);
        return 0;
    }

    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd,
                  ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
        return 0;
    }

    //led 
    uint32_t count = 0;
    uint16_t bufled;
    struct adc_sequence sequence = {
     .buffer = &bufled,
     /* buffer size in bytes, not number of samples */
     .buffer_size = sizeof(bufled),
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
	led_on_right();

    char right[5] = "Right"; //TODO: Need to change
	char left[4] = "Left";
	char down[4] = "Down";
	char up[2] = "Up";

    for (;;) {

        printk("ADC reading[%u]: ", count++);

        (void)adc_sequence_init_dt(&adc_channels[0], &sequence);
        err = adc_read(adc_channels[0].dev, &sequence);
        if (err < 0) {
         printk("Could not read (%d)\n", err);
         continue;
        }

        nowX = (int32_t)bufled;

        (void)adc_sequence_init_dt(&adc_channels[1], &sequence);
        err = adc_read(adc_channels[1].dev, &sequence);
        if (err < 0) {
         printk("Could not read (%d)\n", err);
         continue;
        }

        nowY = (int32_t)bufled;

        if (nowX >= 65500 || nowY >= 65500){
         k_sleep(K_MSEC(50));
         continue;
        }

        bool checkFlag = isChange();
        if(!checkFlag){
         	k_sleep(K_MSEC(50));
         continue;
        } else {
         led_off_all();
        }

        if (nowX == ADC_MAX && nowY == ADC_MAX){
         led_on_center();
         printk("Center");
        } else if (nowX < AXIS_DEVIATION && nowY == ADC_MAX){
         led_on_left();
         bt_nus_send(NULL, left, sizeof(left));
        } else if (nowX > AXIS_DEVIATION && nowY == ADC_MAX){
         led_on_right();
         bt_nus_send(NULL, right, sizeof(right));
        } else if (nowY > AXIS_DEVIATION && nowX == ADC_MAX){
         led_on_up();
         bt_nus_send(NULL, up, sizeof(up));
        } else if (nowY < AXIS_DEVIATION && nowX == ADC_MAX){
         led_on_down();
         bt_nus_send(NULL, down, sizeof(down));
        }

        printk("\n");

        // k_sleep(K_MSEC(100));

        dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
        k_sleep(K_MSEC(50));
        // bt_nus_send(NULL, buf, 20);
    }
}

void ble_write_thread(void)
{
    /* Don't go any further until BLE is initialized */
    k_sem_take(&ble_init_ok, K_FOREVER);

    for (;;) {
        /* Wait indefinitely for data to be sent over bluetooth */
        struct uart_data_t *buf = k_fifo_get(&fifo_uart_rx_data,
                             K_MSEC(50));

        if (bt_nus_send(NULL, buf->data, buf->len)) {
            LOG_WRN("Failed to send data over BLE connection");
        }

        k_free(buf);
    }
}

K_THREAD_DEFINE(ble_write_thread_id, STACKSIZE, ble_write_thread, NULL, NULL,
        NULL, PRIORITY, 0, 0);
