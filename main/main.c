#include "esp_camera.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include <string.h>


#include "lwip/err.h"
#include "lwip/sys.h"

#include "drawfont.h"
#include "espWiFi.h"
#include "espHTTP.h"
#include "espCLIENT.h"
#include "espMQTT.h"
#include "espFILE.h"

#define BOARD_ESP32CAM_AITHINKER

// ESP32Cam (AiThinker) PIN Map
#ifdef BOARD_ESP32CAM_AITHINKER

#define BLINK_GPIO 33
#define FLASHLIGHT_GPIO 4

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 // software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#endif

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_GRAYSCALE, // YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size =
        FRAMESIZE_VGA, // QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, // 0-63 lower number means higher quality
    .fb_count =
        1 // if more than one, i2s runs in continuous mode. Use only with JPEG
};

static esp_err_t init_camera() {
    // initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

void init_gpio() {
    gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    // gpio_pad_select_gpio(FLASHLIGHT_GPIO);
    // gpio_set_direction(FLASHLIGHT_GPIO, GPIO_MODE_OUTPUT);
    // gpio_set_level(FLASHLIGHT_GPIO, 0);  // off
}

void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static void initialize_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}

static void sync_time(void) {
    initialize_sntp();
    // wait for time to be set
    // time_t now = 0;
    // struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET &&
           ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry,
                 retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    // time(&now);
    // setenv("TZ", "UTC-9", 1);
    // tzset();
    // localtime_r(&now, &timeinfo);
}

void draw_info_string(uint8_t *fb_buf, const char *str) {
    // VGA
    int cam_w = 640;
    int cam_h = 480;

    set_defaultfont();
    draw_string(fb_buf, cam_w, cam_h, 8 - 1, 8, str, FONTCOLOR_BLACK);
    draw_string(fb_buf, cam_w, cam_h, 8 + 1, 8, str, FONTCOLOR_BLACK);
    draw_string(fb_buf, cam_w, cam_h, 8, 8 - 1, str, FONTCOLOR_BLACK);
    draw_string(fb_buf, cam_w, cam_h, 8, 8 + 1, str, FONTCOLOR_BLACK);
    draw_string(fb_buf, cam_w, cam_h, 8, 8, str, FONTCOLOR_WHITE);
}

void app_main(void) {
    bool is_wifi_connected = false;

    init_gpio();
    gpio_set_level(BLINK_GPIO, 0); // on

    // ++take_count;
    // ESP_LOGI(TAG, "Boot count: %d", take_count);

    time_t now;
    struct tm timeinfo;
    time(&now);
    setenv("TZ", "UTC-9", 1); // TODO: means UTC+9
    tzset();
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting "
                      "time over NTP.");
        init_wifi();
        is_wifi_connected = true;
        sync_time();
        // update 'now' variable with current time
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    // only take snapshot in 11 and 12 o'clock
    // it makes -at last- one picture taken at a day.
    if (take_count > 0 && timeinfo.tm_hour != 12) {
        ESP_LOGI(TAG, "Skip take picture");
        goto deepsleep;
    }

    char strftime_buf[32];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);

    // gpio_set_level(FLASHLIGHT_GPIO, 1);  // on
    ESP_LOGI(TAG, "Taking picture...");
    init_camera();
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    camera_fb_t *fb;
    // waste first 5 frame for Auto Expose
    for (int i = 0; i < 5; i++) {
        fb = esp_camera_fb_get();
        esp_camera_fb_return(fb);
    }
    fb = esp_camera_fb_get();
    // gpio_set_level(FLASHLIGHT_GPIO, 0);  // off

    char strinfo_buf[64];
    sprintf(strinfo_buf, "%s cnt: %03d", strftime_buf, ++take_count);
    draw_info_string(fb->buf, strinfo_buf);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    bool converted = frame2jpg(fb, 80, &buf, &buf_len);
    esp_camera_fb_return(fb);
    esp_camera_deinit();
    if (!converted)
        goto deepsleep;

    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, (const char *)(buf),
                            buf_len, 0, 0);
    if (take_count > 1) {
        sync_time(); // interal RTC sucks sync time again.
    }

deepsleep:
    // TODO: need free buf?
    gpio_set_level(BLINK_GPIO, 1); // off

    //esp_mqtt_client_destroy(mqtt_client);
    //if (is_wifi_connected)
        //esp_wifi_stop();

    const int deep_sleep_sec = 1 * 60 * 60;
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", deep_sleep_sec);
    esp_deep_sleep(1000000LL * deep_sleep_sec);
}
