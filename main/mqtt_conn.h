#ifndef MQTT_CONN_H
#define MQTT_CONN_H

#include "mqtt_client.h"

esp_mqtt_client_handle_t mqtt_init(void (*cb)(void *, esp_event_base_t, int32_t, void *),
								   char *, 
								   uint16_t);
#endif
