#ifndef _ESP_HTTP_H_
#define _ESP_HTTP_H_

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "string.h"
#include <sys/param.h>
#include "esp_wifi.h"
#include <stdbool.h>

#include "lwip/err.h"
#include "lwip/sys.h"
#include "cJSON.h"
#include "espWiFi.h"
#include "espFILE.h"
#include "espCLIENT.h"

esp_err_t get_url_server(httpd_req_t *req);
esp_err_t post_handler(httpd_req_t *req);
httpd_handle_t server_start(void);
void server_stop(httpd_handle_t server_handle);

int flag_call_mqtt;

#endif 
