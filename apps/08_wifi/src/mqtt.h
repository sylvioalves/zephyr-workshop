#ifndef __MQTT_H__
#define __MQTT_H__

#include <zephyr/kernel.h>

typedef void (*user_cb)(const char *topic, uint32_t topic_len, const char *msg, uint32_t msg_len);

bool mqtt_connected(void);
void mqtt_init(user_cb cb);
int32_t mqtt_publish_to(const uint8_t *topic, uint32_t topic_len, uint8_t *payload,
			uint32_t payload_len, uint8_t qos);
int32_t mqtt_subscribe_to(const uint8_t *topic, uint32_t topic_len, uint8_t qos);
int32_t mqtt_unsubscribe_from(const uint8_t *topic, uint32_t topic_len);
int32_t mqtt_connect_broker(void);
void mqtt_disconnect_broker(void);

#endif
