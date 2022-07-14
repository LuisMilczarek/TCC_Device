#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

#include "wifi_start.h"
#include "mqtt_conn.h"
#include "MAX6675.h"

#define BROKER_URL CONFIG_BROKER_URL
#define BROKER_PORT CONFIG_BROKER_PORT
#define DELTA 5

static const char *TAG = "Main";

/* TOPICOS:
* Setar/Monitorar Setpoint (SP)
* Setar/Monitorar Tempo Alvo
* 
* Monitorar Temperatura
* Setar/Monitorar Estado
* 
*/

static const char *OUT_TOPIC = "/ifrs_rg/auto/sub/fornos/1/temp";
static const char *IN_TOPIC = "/ifrs_rg/auto/sub/fornos/1/setpoint";
static const char *STATE_TOPIC = "/ifrs_rg/auto/sub/fornos/1/state";
static const char *TIME_TOPIC = "/ifrs_rg/auto/sub/fornos/1/time";

esp_timer_handle_t bakingTimer;

uint8_t relayPin = 2;

uint8_t connected = 0;
uint16_t SP = 25;

void mqttHandler(void *, esp_event_base_t, int32_t, void *);
void dataCallback(char *, int, char *, int, esp_mqtt_client_handle_t);
void endTimeCallback(void *);
static void log_error_if_nonzero(const char *, int);

enum states {IDLE, WARM_UP,READY,BAKING, FINISHED} state;


void app_main(void)
{
    esp_log_level_set(TAG,ESP_LOG_DEBUG);
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    const esp_timer_create_args_t bakingTimerArgs = {
            .callback = &endTimeCallback, //seta o calback do timer
    };
    ESP_ERROR_CHECK(esp_timer_create(&bakingTimerArgs, &bakingTimer));
    
    gpio_reset_pin(relayPin);
    gpio_set_direction(relayPin, GPIO_MODE_OUTPUT);
    
    state = IDLE;
    
    wifi_init_sta();
    esp_mqtt_client_handle_t client = mqtt_init(&mqttHandler, BROKER_URL, BROKER_PORT);
    
    MAX6675 tempSensor = {};
    tempSensor.spi = SPI2_HOST;
    tempSensor.DMA_channel = 2;
    tempSensor.CS = 5;
    
    initMAX6675(&tempSensor);
    float temp;
    char msg[10];
    
    while(1)
    {
        switch (state)
        {
            case IDLE:
                ESP_LOGI(TAG,"Im in idle!!!");
                break;
            case WARM_UP:
                if(getTemp(&tempSensor, &temp) == ESP_OK && temp >= SP)
                {
                    state = READY;
                    esp_mqtt_client_publish(client, STATE_TOPIC,"READY", 0, 1, 0);
                }
                sprintf(msg,"%d",(int)temp);
                esp_mqtt_client_publish(client, OUT_TOPIC, msg, 0, 1, 0);

                ESP_LOGI(TAG,"Actual temp: %f", temp);
                break;
            case READY:
            case BAKING:
                if(getTemp(&tempSensor, &temp) == ESP_OK)
                {
                    ESP_LOGI(TAG,"Temp value: %.2fC",temp);
                    if(connected)
                    {
                         sprintf(msg,"%d",(int)temp);
                         esp_mqtt_client_publish(client, OUT_TOPIC, msg, 0, 1, 0);
                    }
                    if(temp < SP - DELTA)
                    {
                        gpio_set_level(relayPin, 1);
                    }
                    else if(temp > SP + DELTA)
                    {
                        gpio_set_level(relayPin, 0);
                    }
                }
                
                break;

            case FINISHED:
                esp_mqtt_client_publish(client, STATE_TOPIC, "FINISHED",0,1,0);
                gpio_set_level(relayPin,0);
                state = IDLE;
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void mqttHandler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        connected = 1;
        msg_id = esp_mqtt_client_subscribe(client, IN_TOPIC, 0);
        msg_id = esp_mqtt_client_subscribe(client, STATE_TOPIC, 0);
        msg_id = esp_mqtt_client_subscribe(client, TIME_TOPIC, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
            
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        connected = 0;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
            
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
            
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
            
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        dataCallback(event->topic, event->topic_len,event->data,event->data_len, client);
        break;
            
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
            
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

void dataCallback(char *topic, int topic_len, char *data, int data_len, esp_mqtt_client_handle_t client)
{
    
    if(strncmp(topic, TIME_TOPIC, topic_len) == 0 && state != BAKING)
    {
           state = BAKING;
           ESP_LOGI(TAG, "New TIME: %d",atoi(data));
           esp_timer_start_once(bakingTimer, atoi(data) * 60000000);
           esp_mqtt_client_publish(client, STATE_TOPIC, "BAKING", 0, 1, 0);

    }
    else if(strncmp(topic, IN_TOPIC, topic_len) == 0)
    {
        SP = atoi(data);
        ESP_LOGI(TAG, "New SP: %d",SP);
        state = WARM_UP;
        gpio_set_level(relayPin,1);
        esp_mqtt_client_publish(client, STATE_TOPIC, "WARM_UP", 0, 1, 0);
    }
}

void endTimeCallback(void *ptr)
{
    state = FINISHED;
}
