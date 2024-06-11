/*
 * Copyright (c) 2020 Libre Solar Technologies GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>


#include <zephyr/types.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

#include <string.h>
#include <errno.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <soc.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include "../services/my_service.h"

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
//endif

static uint8_t mfg_data[] = { 0xff, 0xff, 0x00 };

static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, 3),
};

static const struct bt_data sd[] =
{
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, MY_SERVICE_UUID),
};

struct bt_conn *my_connection;

struct k_sem ble_init_ok;

static void connected(struct bt_conn *conn, uint8_t err)
{
	struct bt_conn_info info;
	char addr[BT_ADDR_LE_STR_LEN];

	my_connection = conn;

	if (err)
	{
		printk("Connection failed (err %u)\n", err);
		return;
	}
	else if(bt_conn_get_info(conn, &info))
	{
		printk("Could not parse connection info\n");
	}
	else
	{
		bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

		printk("Connection established!		\n\
		Connected to: %s					\n\
		Role: %u							\n\
		Connection interval: %u				\n\
		Slave latency: %u					\n\
		Connection supervisory timeout: %u	\n"
		, addr, info.role, info.le.interval, info.le.latency, info.le.timeout);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason %u)\n", reason);
}

static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
	//If acceptable params, return true, otherwise return false.
	return true;
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout)
{
	struct bt_conn_info info;
	char addr[BT_ADDR_LE_STR_LEN];

	if(bt_conn_get_info(conn, &info))
	{
		printk("Could not parse connection info\n");
	}
	else
	{
		bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

		printk("Connection parameters updated!	\n\
		Connected to: %s						\n\
		New Connection Interval: %u				\n\
		New Slave Latency: %u					\n\
		New Connection Supervisory Timeout: %u	\n"
		, addr, info.le.interval, info.le.latency, info.le.timeout);
	}
}

static struct bt_conn_cb conn_callbacks =
{
	.connected			= connected,
	.disconnected   		= disconnected,
	.le_param_req			= le_param_req,
	.le_param_updated		= le_param_updated
};

static void bt_ready(int err)
{
	if (err)
	{
		printk("BLE init failed with error code %d\n", err);
		return;
	}

	//Configure connection callbacks
	bt_conn_cb_register(&conn_callbacks);

	//Initalize services
	err = my_service_init();
	if (err)
	{
		printk("Failed to init LBS (err:%d)\n", err);
		return;
	}

	//Start advertising
	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");

	k_sem_give(&ble_init_ok);
}


static void error(void)
{
	while (true) {
		printk("Error!\n");
		/* Spin for ever */
		k_sleep(K_MSEC(1000)); //1000ms
	}
}

int main(void)
{
	int err;
	uint32_t count = 0;
	uint16_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
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
	int current_position = 0;

	uint32_t number = 0;

	printk("Starting Nordic BLE peripheral tutorial\n");

	k_sem_init(&ble_init_ok, 0, 1);
	err = bt_enable(bt_ready);

	if (err)
	{
		printk("BLE initialization failed\n");
		error(); //Catch error
	}

	/* 	Bluetooth stack should be ready in less than 100 msec. 								\
																							\
		We use this semaphore to wait for bt_enable to call bt_ready before we proceed 		\
		to the main loop. By using the semaphore to block execution we allow the RTOS to 	\
		execute other tasks while we wait. */
	err = k_sem_take(&ble_init_ok, K_MSEC(500));

	if (!err)
	{
		printk("Bluetooth initialized\n");
	} else
	{
		printk("BLE initialization did not complete in time\n");
		error(); //Catch error
	}

	err = my_service_init();

	while (1) {
		printk("ADC reading[%u]: ", count++);

		(void)adc_sequence_init_dt(&adc_channels[0], &sequence);
		err = adc_read(adc_channels[0].dev, &sequence);
		if (err < 0) {
			printk("Could not read (%d)\n", err);
			continue;
		}

		nowX = (int32_t)buf;

		(void)adc_sequence_init_dt(&adc_channels[1], &sequence);
		err = adc_read(adc_channels[1].dev, &sequence);
		if (err < 0) {
			printk("Could not read (%d)\n", err);
			continue;
		}

		nowY = (int32_t)buf;

        printk("Joy X: %" PRIu32 ", ", nowX);
		printk("Joy Y: %" PRIu32 ", ", nowY);

		if (nowX >= 65500 || nowY >= 65500){
			printk("Out of Range\n");
			k_sleep(K_MSEC(100));
			continue;
		}

		bool checkFlag = isChange();
		if(!checkFlag){
            printk("No Change\n");
			k_sleep(K_MSEC(100));
			continue;
		} else {
			led_off_all();
		}

		if (nowX == ADC_MAX && nowY == ADC_MAX){
			// led_on_center();
			printk("Center");
		} else if (nowX < AXIS_DEVIATION && nowY == ADC_MAX){
			// led_on_left();
			printk("Left");
			if ( current_position != 0)
				current_position = current_position - 1;
		} else if (nowX > AXIS_DEVIATION && nowY == ADC_MAX){
			// led_on_right();
			printk("Right");
			if ( current_position < 127)
				current_position = current_position + 1;
		} else if (nowY > AXIS_DEVIATION && nowX == ADC_MAX){
			// led_on_up();
			if ( current_position > 16)
				current_position = current_position - 16;
			printk("Up");
		} else if (nowY < AXIS_DEVIATION && nowX == ADC_MAX){
			// led_on_down();
			if ( current_position < 127)
				current_position = current_position + 16;
			printk("Down");
		}

		led_on(led, current_position);
        printk("\n");
		my_service_send(my_connection, (uint8_t *)&number, sizeof(number));
		number++;

		k_sleep(K_MSEC(100));
	}

	return 0;
}
