/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if CONFIG_EXT_THINGSET_CAN

#include "ext/ext.h"
#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/can.h>

#ifdef CONFIG_ISOTP
#include <canbus/isotp.h>
#endif

#include "thingset.h"
#include "data_nodes.h"
#include "can_msg_queue.h"

#ifndef CAN_NODE_ID
#define CAN_NODE_ID 20
#endif

extern ThingSet ts;

struct device *can_dev;

#ifdef CONFIG_ISOTP

#define RX_THREAD_STACK_SIZE 512
#define RX_THREAD_PRIORITY 2

const struct isotp_fc_opts fc_opts = {.bs = 8, .stmin = 0};

const struct isotp_msg_id rx_addr = {
    .std_id = 0x80,
    .id_type = CAN_STANDARD_IDENTIFIER,
    .use_ext_addr = 0
};

const struct isotp_msg_id tx_addr = {
    .std_id = 0x180,
    .id_type = CAN_STANDARD_IDENTIFIER,
    .use_ext_addr = 0
};

struct isotp_recv_ctx recv_ctx;

K_THREAD_STACK_DEFINE(rx_thread_stack, RX_THREAD_STACK_SIZE);
struct k_thread rx_thread_data;

void send_complette_cb(int error_nr, void *arg)
{
    ARG_UNUSED(arg);
    printk("TX complete cb [%d]\n", error_nr);
}

void rx_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);
    int ret, rem_len;
    unsigned int received_len;
    struct net_buf *buf;
    static u8_t rx_buffer[100];
    static u8_t tx_buffer[500];

    ret = isotp_bind(&recv_ctx, can_dev, &tx_addr, &rx_addr, &fc_opts, K_FOREVER);
    if (ret != ISOTP_N_OK) {
        printk("Failed to bind to rx ID %d [%d]\n", rx_addr.std_id, ret);
        return;
    }

    while (1) {
        received_len = 0;
        do {
            rem_len = isotp_recv_net(&recv_ctx, &buf, K_FOREVER);
            if (rem_len < 0) {
                printk("Receiving error [%d]\n", rem_len);
                break;
            }
            if (received_len + buf->len <= sizeof(rx_buffer)) {
                memcpy(&rx_buffer[received_len], buf->data, buf->len);
                received_len += buf->len;
            }
            else {
                printk("RX buffer too small\n");
                break;
            }
            net_buf_unref(buf);
        } while (rem_len);

        if (received_len > 0) {
            printk("Got %d bytes in total. Processing ThingSet message.\n", received_len);
            int resp_len = ts.process(rx_buffer, received_len, tx_buffer, sizeof(tx_buffer));

            if (resp_len > 0) {
                static struct isotp_send_ctx send_ctx;
                int ret = isotp_send(&send_ctx, can_dev, tx_buffer, resp_len,
                            &tx_addr, &rx_addr, send_complette_cb, NULL);
                if (ret != ISOTP_N_OK) {
                    printk("Error while sending data to ID %d [%d]\n", tx_addr.std_id, ret);
                }
            }
        }
    }
}

#endif /* CONFIG_ISOTP */

class ThingSetCAN: public ExtInterface
{
public:
    ThingSetCAN(uint8_t can_node_id, const unsigned int c);

    void process_asap();
    void process_1s();

    void enable();

private:
    /**
     * Generate CAN frame for data object and put it into TX queue
     */
    bool pub_object(const DataNode& data_obj);

    /**
     * Retrieves all data objects of configured channel and calls pub_object to enqueue them
     *
     * \returns number of can objects added to queue
     */
    int pub();

    /**
     * Try to send out all data in TX queue
     */
    void process_outbox();

    CanMsgQueue tx_queue;
    uint8_t node_id;
    const uint16_t channel;

    struct device *can_en_dev;
};

ThingSetCAN ts_can(CAN_NODE_ID, PUB_CAN);

//----------------------------------------------------------------------------
// preliminary simple CAN functions to send data to the bus for logging
// Data format based on CBOR specification (except for first byte, which uses
// only 6 bit to specify type and transport protocol)
//
// Protocol details: https://libre.solar/thingset/

ThingSetCAN::ThingSetCAN(uint8_t can_node_id, const unsigned int c):
    node_id(can_node_id),
    channel(c)
{
    can_en_dev = device_get_binding(DT_OUTPUTS_CAN_EN_GPIOS_CONTROLLER);
    gpio_pin_configure(can_en_dev, DT_OUTPUTS_CAN_EN_GPIOS_PIN,
        DT_OUTPUTS_CAN_EN_GPIOS_FLAGS | GPIO_OUTPUT_INACTIVE);

    can_dev = device_get_binding("CAN_1");
}

void ThingSetCAN::enable()
{
    gpio_pin_set(can_en_dev, DT_OUTPUTS_CAN_EN_GPIOS_PIN, 1);

#ifdef CONFIG_ISOTP
    k_tid_t tid = k_thread_create(&rx_thread_data, rx_thread_stack,
                  K_THREAD_STACK_SIZEOF(rx_thread_stack),
                  rx_thread, NULL, NULL, NULL,
                  RX_THREAD_PRIORITY, 0, K_NO_WAIT);
    if (!tid) {
        printk("ERROR spawning rx thread\n");
    }
#endif /* CONFIG_ISOTP */
}

void ThingSetCAN::process_1s()
{
    pub();
    process_asap();
}

int ThingSetCAN::pub()
{
    int retval = 0;
    unsigned int can_id;
    uint8_t can_data[8];

    if (pub_can_enable) {
        int data_len = 0;
        int start_pos = 0;
        while ((data_len = ts.bin_pub_can(start_pos, channel, node_id, can_id, can_data)) != -1) {

            struct zcan_frame frame = {0};
            frame.id_type = CAN_STANDARD_IDENTIFIER;
            frame.rtr     = CAN_DATAFRAME;
            frame.ext_id = can_id;
            memcpy(frame.data, can_data, 8);

            if (data_len >= 0) {
                frame.dlc = data_len;
                tx_queue.enqueue(frame);
            }
            retval++;
        }
    }
    return retval;
}

void ThingSetCAN::process_asap()
{
    process_outbox();
}

void can_pub_isr(uint32_t err_flags, void *arg)
{
	// Do nothing. Publication messages are fire and forget.
}

void ThingSetCAN::process_outbox()
{
    int max_attempts = 15;
    while (!tx_queue.empty() && max_attempts > 0) {
        CanFrame msg;
        tx_queue.first(msg);
        if (can_send(can_dev, &msg, K_MSEC(10), can_pub_isr, NULL) == CAN_TX_OK) {
            tx_queue.dequeue();
        }
        else {
            //printk("Sending CAN message failed");
        }
        max_attempts--;
    }
}

#endif /* CONFIG_EXT_THINGSET_CAN */

