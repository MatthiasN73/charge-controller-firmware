/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ESP32_H__
#define __ESP32_H__

#include "mbed.h"

enum ESP32_status_t {
    ESP32_STATUS_AP_CONNECTED = 2,
    ESP32_STATUS_TCP_ACTIVE,
    ESP32_STATUS_TCP_DIS,
    ESP32_STATUS_AP_DISCONNECTED
};

enum ESP32_wifi_mode_t {
    ESP32_WIFI_MODE_NULL = 0,
    ESP32_WIFI_MODE_STATION,
    ESP32_WIFI_MODE_SOFTAP,
    ESP32_WIFI_MODE_BOTH
};

enum ESP32_ip_mode_t {
    ESP32_IP_MODE_NORMAL = 0,
    ESP32_IP_MODE_PASSTHROUGH
};

class ESP32
{
public:
    ESP32(UARTSerial &s);
    ~ESP32();

    int reset();
    void print_firmware();

    int set_wifi_mode(ESP32_wifi_mode_t mode);
    int get_conn_status();
    void list_APs(char *buf, int size);
    int join_AP(const char *id, const char *pwd);
    int quit_AP();

    int get_IP(char* ip, int size);
    int ping(const char* ip);

    int set_single();
    int set_multiple();

    int set_ip_mode(ESP32_ip_mode_t mode);

    int start_TCP_conn(const char *ip, const char *port, bool ssl=false);
    int close_TCP_conn();
    int send_URL(char *url, char *host);
    int send_TCP_data(uint8_t *data, int length);

    int start_TCP_server(int port);
    int close_TCP_server();

private:
    UARTSerial &_serial;
    ATCmdParser _at;
};

#endif /* __ESP32_H__ */
