#include "mqtt_conn.h"

esp_mqtt_client_handle_t mqtt_init(void (*cb)(void *, esp_event_base_t, int32_t, void *),
								   char *broker_url,
								   uint16_t port)
{
	esp_mqtt_client_config_t mqtt_cfg = {
        .uri = broker_url,
		.port = port
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, *cb, NULL);
    esp_mqtt_client_start(client);
	return client;
}
