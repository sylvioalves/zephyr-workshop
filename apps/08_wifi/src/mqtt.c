#include <zephyr/kernel.h>
#include <errno.h>

#include "mqtt_service.h"
#include "mqtt.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mqtt, LOG_LEVEL_DBG);

static bool _mqtt_connected = false;

static user_cb callback = NULL;

static void mqtt_service_user_cb(mqtt_service_event_t *evt)
{
	switch (evt->type) {
	case MQTT_SERVICE_EVT_CONNECTED:
		LOG_INF("MQTT Client connected!");
		_mqtt_connected = true;
		break;

	case MQTT_SERVICE_EVT_DISCONNECTED:
		LOG_WRN("MQTT Client disconnected! Reason: %d", evt->data.disc_reason);
		_mqtt_connected = false;
		break;

	case MQTT_SERVICE_EVT_BROKER_ACK:
		LOG_INF("MQTT Broker has acknowledged message, MID = %d", evt->data.ack.message_id);
		break;

	case MQTT_SERVICE_EVT_DATA_RECEIVED:
		// LOG_INF("Received %d bytes from topic: %.*s", evt->data.publish.msg_length,
		// evt->data.publish.topic_name_length, evt->data.publish.p_topic_name);
		// LOG_HEXDUMP_INF(evt->data.publish.p_msg_data, evt->data.publish.msg_length,
		// "Message");
		if (callback != NULL) {
			callback(evt->data.publish.p_topic_name,
				 evt->data.publish.topic_name_length, evt->data.publish.p_msg_data,
				 evt->data.publish.msg_length);
		}
		break;

	default:
		LOG_WRN("Uknown event from MQTT service module");
		break;
	}
}

void mqtt_init(user_cb cb)
{
	int err = 0;

	if (cb != NULL) {
		callback = cb;
	}

	err = mqtt_service_init(mqtt_service_user_cb);
	if (err) {
		LOG_ERR("Failed to initialize MQTT service");
	}
}

bool mqtt_connected(void)
{
	return _mqtt_connected;
}

int32_t mqtt_connect_broker(void)
{
	return mqtt_service_connect();
}

void mqtt_disconnect_broker(void)
{
	mqtt_service_disconnect();
}

int32_t mqtt_publish_to(const uint8_t *topic, uint32_t topic_len, uint8_t *payload,
			uint32_t payload_len, uint8_t qos)
{
	return mqtt_service_publish(topic, topic_len, payload, payload_len, qos);
}

int32_t mqtt_subscribe_to(const uint8_t *topic, uint32_t topic_len, uint8_t qos)
{
	return mqtt_service_subscribe(topic, topic_len, qos);
}

int32_t mqtt_unsubscribe_from(const uint8_t *topic, uint32_t topic_len)
{
	return mqtt_service_unsubscribe(topic, topic_len);
}
