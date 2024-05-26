#ifndef WIFI_H
#define WIFI_H


#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "esp_http_server.h"

#include "config.h"
#include "sd_card.h"
#include "i2s_mic.h"

#include "esp_log.h"



void start_wifi(void);
void start_http_server(void);


#endif