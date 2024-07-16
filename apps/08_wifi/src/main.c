#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>

#include "mqtt.h"
#include "wifi_service.h"
#include "temperature.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

enum {
	WIFI_DISCONNECTED,
	WIFI_CONNECTING,
	WIFI_CONNECTED,
	MQTT_DISCONNECTED,
	MQTT_CONNECTED
};

const char topic_pub[] = "z/workshop/data";
const char topic_sub[] = "z/workshop/cmd";

static uint8_t m_state = WIFI_DISCONNECTED;

void mqtt_response(const char *topic, uint32_t topic_len, const char *msg, uint32_t msg_len)
{
	LOG_INF("topic: %s", topic);
	LOG_INF("msg: %s", msg);
}

int main(void)
{
	char msg[50];

	/* init wifi and mqtt */
	temp_init();
	wifi_init();
	mqtt_init(mqtt_response);

	for (;;) {

		switch (m_state) {
		case WIFI_DISCONNECTED:
			wifi_connect();
			m_state = WIFI_CONNECTING;
			break;

		case WIFI_CONNECTING:
			if (wifi_connected()) {
				m_state = WIFI_CONNECTED;
			} else {
				m_state = WIFI_DISCONNECTED;
			}
			break;

		case WIFI_CONNECTED:
			if (mqtt_connect_broker() == 0) {
				mqtt_subscribe_to(topic_sub, strlen(topic_sub), 0);
				m_state = MQTT_CONNECTED;
			} else {
				m_state = WIFI_CONNECTING;
			}
			break;

		case MQTT_CONNECTED:
			if (!wifi_connected()) {
				mqtt_disconnect_broker();
				m_state = WIFI_DISCONNECTED;
			}

			if (mqtt_connected()) {
				double temp_val;
				temp_read(&temp_val);
				sprintf(msg, "{\"name\":\"%s\",\"temp\":%0.1f}", CONFIG_MQTT_DEVICE_NAME, temp_val);
				LOG_INF("publishing msg: %s", msg);
				mqtt_publish_to(topic_pub, strlen(topic_pub), msg, strlen(msg), 0);
			} else {
				mqtt_disconnect_broker();
				m_state = WIFI_CONNECTING;
			}
			break;

		default:
			break;
		}

		k_msleep(2000);
	}

	return 0;
}
