#include <zephyr/kernel.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/util.h>

#include "mqtt_service.h"

LOG_MODULE_REGISTER(mqtt_service, CONFIG_MQTT_SERVICE_LOG_LEVEL);

#define MQTT_SERVICE_KEEP_ALIVE_TIMEOUT 60000

#define MQTT_SERVICE_DNS_TIMEOUT (2 * MSEC_PER_SEC)

#define MQTT_SERVICE_IP_ADDR_STRING_LEN NET_IPV4_ADDR_LEN

#define MQTT_SERVICE_UID_LENGTH (12u)

static int32_t mqtt_service_client_init(struct mqtt_client *client);

static int32_t mqtt_service_client_connect(void);

static void mqtt_service_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt);

/* MQTT client struct */
static struct mqtt_client client_ctx;

/* MQTT broker details. */
static struct sockaddr mqtt_broker;

/* MQTT sync connection flags */
static atomic_t disconnect_requested;
static atomic_t connection_poll_active;
static atomic_t broker_disconnected = ATOMIC_INIT(1);

/* RX and TX buffers */
static uint8_t rx_buffer[CONFIG_MQTT_SERVICE_RX_BUFFER_SIZE];
static uint8_t tx_buffer[CONFIG_MQTT_SERVICE_TX_BUFFER_SIZE];

static uint8_t payload[CONFIG_MQTT_SERVICE_PAYLOAD_BUFFER_SIZE];

/* MQTT topic and subscription list */
static struct mqtt_topic subs_topic;
static struct mqtt_subscription_list subs_list;

/* MQTT message ID counter (valid values start from 1) */
static uint16_t message_id = 1u;

/** Semaphore for starting MQTT service connection */
static K_SEM_DEFINE(connection_poll_sem, 0, 1);

/** User callback for reporting events to application layer */
static mqtt_service_evt_cb_t user_cb;

int32_t mqtt_service_init(mqtt_service_evt_cb_t cb)
{
	if (NULL == cb) {
		return -EINVAL;
	}

	user_cb = cb;

	return mqtt_service_client_init(&client_ctx);
}

int32_t mqtt_service_connect(void)
{
	if (atomic_get(&connection_poll_active)) {
		LOG_DBG("Connection poll in progress");
		return -EINPROGRESS;
	}

	atomic_set(&disconnect_requested, 0);
	k_sem_give(&connection_poll_sem);

	return 0;
}

int32_t mqtt_service_disconnect(void)
{
	int32_t err = 0;

	atomic_set(&disconnect_requested, 1);

	err = mqtt_disconnect(&client_ctx);

	return err;
}

int32_t mqtt_service_publish(const uint8_t *p_topic, uint32_t topic_len, uint8_t *p_payload,
			     uint32_t payload_len, uint8_t qos)
{
	if (atomic_get(&broker_disconnected) == 1) {
		LOG_WRN("Not connected. Unable to publish!");
		return -ENETDOWN;
	}

	int32_t ret = 0;
	struct mqtt_publish_param publish_params;

	publish_params.message.topic.qos = qos;
	publish_params.message.topic.topic.utf8 = p_topic;
	publish_params.message.topic.topic.size = topic_len;
	publish_params.message.payload.data = p_payload;
	publish_params.message.payload.len = payload_len;
	publish_params.dup_flag = 0U;
	publish_params.retain_flag = 0U;
	publish_params.message_id = message_id++; // sys_rand32_get(); // Message ID?
	// Value of 0 for message ID is forbidden
	if (0 == message_id) {
		message_id = 1u;
	}

	ret = mqtt_publish(&client_ctx, &publish_params);

	if (ret) {
		// LOG_ERR("Failed to publish to %.*s, err: %d", topic_len, p_topic, ret);
	} else {
		ret = publish_params.message_id;
	}

	return ret;
}

int32_t mqtt_service_subscribe(const uint8_t *p_topic, uint32_t topic_len, uint8_t qos)
{
	int32_t err;

	subs_topic.topic.utf8 = p_topic;
	subs_topic.topic.size = topic_len;
	subs_topic.qos = qos;
	subs_list.list = &subs_topic;
	subs_list.list_count = 1U;
	subs_list.message_id = message_id++;
	// Value of 0 for message ID is forbidden
	if (0 == message_id) {
		message_id = 1u;
	}

	err = mqtt_subscribe(&client_ctx, &subs_list);
	if (err) {
		// LOG_ERR("Failed subscribing to topic %s", p_topic);
	}

	return err;
}

int32_t mqtt_service_unsubscribe(const uint8_t *p_topic, uint32_t topic_len)
{
	int32_t err;

	subs_topic.topic.utf8 = p_topic;
	subs_topic.topic.size = topic_len;
	subs_list.list = &subs_topic;
	subs_list.list_count = 1U;
	subs_list.message_id = message_id++;
	// Value of 0 for message ID is forbidden
	if (0 == message_id) {
		message_id = 1u;
	}

	err = mqtt_unsubscribe(&client_ctx, &subs_list);
	if (err) {
		// LOG_ERR("Failed unsubscribing from topic %s", p_topic);
	}

	return err;
}

//---------------------------- PRIVATE FUNCTIONS ------------------------------

static int broker_init(void)
{
	int err;
	struct zsock_addrinfo *result;
	struct zsock_addrinfo *addr;
	struct zsock_addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM};

	LOG_INF("Resolving: %s", CONFIG_MQTT_SERVICE_SERVER_DOMAIN_NAME);

	err = zsock_getaddrinfo(CONFIG_MQTT_SERVICE_SERVER_DOMAIN_NAME, NULL, &hints, &result);
	if (err) {
		// LOG_ERR("getaddrinfo, error %d", err);
		return -ECHILD;
	}

	addr = result;

	/* Look for address of the broker. */
	while (addr != NULL) {
		/* IPv4 Address. */
		if (addr->ai_addrlen == sizeof(struct sockaddr_in)) {
			struct sockaddr_in *broker4 = ((struct sockaddr_in *)&mqtt_broker);
			char ipv4_addr[NET_IPV4_ADDR_LEN];

			broker4->sin_addr.s_addr =
				((struct sockaddr_in *)addr->ai_addr)->sin_addr.s_addr;
			broker4->sin_family = AF_INET;
			broker4->sin_port = htons(CONFIG_MQTT_SERVICE_SERVER_PORT);

			inet_ntop(AF_INET, &broker4->sin_addr.s_addr, ipv4_addr, sizeof(ipv4_addr));
			LOG_INF("IPv4 Address found %s", ipv4_addr);

			break;
		} else {
			LOG_ERR("ai_addrlen = %u should be %u or %u",
				(unsigned int)addr->ai_addrlen,
				(unsigned int)sizeof(struct sockaddr_in),
				(unsigned int)sizeof(struct sockaddr_in6));
		}

		addr = addr->ai_next;
	}

	/* Free the address. */
	zsock_freeaddrinfo(result);
	return err;
}

#if defined(CONFIG_MQTT_LIB_TLS)

#if defined(CONFIG_TLS_CREDENTIAL_FILENAMES)
static const unsigned char ca_certificate[] = "ca.der";
static const unsigned char server_certificate[] = "device.der";
static const unsigned char private_key[] = "device.key";
#else
static const unsigned char ca_certificate[] = {
#include "ca.der.inc"
};
static const unsigned char server_certificate[] = {
#include "device.der.inc"
};
static const unsigned char private_key[] = {
#include "device_key.der.inc"
};
#endif

#define APP_CA_CERT_TAG 1

static sec_tag_t m_sec_tags[] = {
	APP_CA_CERT_TAG,
};

static int tls_init(struct mqtt_client *client)
{
	int err = -EINVAL;

	err = tls_credential_add(APP_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE, ca_certificate,
				 sizeof(ca_certificate));
	if (err != 0) {
		// LOG_ERR("Failed to register ca certificate: %d", err);
		return err;
	}

	err = tls_credential_add(APP_CA_CERT_TAG, TLS_CREDENTIAL_SERVER_CERTIFICATE,
				 server_certificate, sizeof(server_certificate));
	if (err < 0) {
		// LOG_ERR("Failed to register server certificate: %d", err);
	}

	err = tls_credential_add(APP_CA_CERT_TAG, TLS_CREDENTIAL_PRIVATE_KEY, private_key,
				 sizeof(private_key));
	if (err < 0) {
		// LOG_ERR("Failed to register private key: %d", err);
	}

	client->transport.type = MQTT_TRANSPORT_SECURE;

	struct mqtt_sec_config *tls_config = &client->transport.tls.config;

	tls_config->peer_verify = TLS_PEER_VERIFY_NONE;
	tls_config->cipher_list = NULL;
	tls_config->sec_tag_list = m_sec_tags;
	tls_config->sec_tag_count = ARRAY_SIZE(m_sec_tags);
	tls_config->hostname = CONFIG_MQTT_SERVICE_SERVER_DOMAIN_NAME;
	tls_config->cert_nocopy = TLS_CERT_NOCOPY_OPTIONAL;

	return err;
}

#endif // CONFIG_MQTT_LIB_TLS

static int32_t mqtt_service_client_init(struct mqtt_client *client)
{
	// UID length on STM32 MCUs is 12 bytes
	uint8_t client_id[MQTT_SERVICE_UID_LENGTH];
	// 12 UID bytes (represented in hex) * 2 (one byte = 2 hex characters) +
	// string null termination
	static uint8_t client_id_string[(2u * MQTT_SERVICE_UID_LENGTH) + 1u];
	int32_t err = 0;

	// Prepare MQTT client structure for initializing its values
	mqtt_client_init(client);

	/* MQTT client configuration */
	client->broker = &mqtt_broker;
	client->evt_cb = mqtt_service_evt_handler;

	size_t hwinfo_size_read;
	// Get device serial number in binary (byte) representation
	hwinfo_size_read = hwinfo_get_device_id(client_id, sizeof(client_id));

	// LOG_HEXDUMP_INF(client_id, hwinfo_size_read, "Device ID");

	// Convert device serial number from binary (byte) to hex string
	// representation
	(void)bin2hex((const uint8_t *)client_id, sizeof(client_id), client_id_string,
		      sizeof(client_id_string));

	// LOG_INF("Device ID String: %s", client_id_string);

	client->client_id.utf8 = (const uint8_t *)client_id_string;
	client->client_id.size = strlen(client_id_string);
	client->password = NULL;
	client->user_name = NULL;
	client->protocol_version = MQTT_VERSION_3_1_1;

	/* MQTT buffers configuration */
	client->rx_buf = rx_buffer;
	client->rx_buf_size = sizeof(rx_buffer);
	client->tx_buf = tx_buffer;
	client->tx_buf_size = sizeof(tx_buffer);

	/* MQTT transport configuration */
#if defined(CONFIG_MQTT_LIB_TLS)
	tls_init(client);
#else
	client->transport.type = MQTT_TRANSPORT_NON_SECURE;
#endif

	return err;
}

static void mqtt_service_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt)
{
	mqtt_service_event_t event;

	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		event.type = MQTT_SERVICE_EVT_CONNECTED;

		if (evt->result != 0) {
			// LOG_ERR("MQTT connect failed %d", evt->result);
			break;
		}

		LOG_DBG("MQTT client connected!");

		user_cb(&event);

		break;

	case MQTT_EVT_DISCONNECT:
		event.type = MQTT_SERVICE_EVT_DISCONNECTED;

		atomic_set(&broker_disconnected, 1);

		LOG_DBG("MQTT client disconnected %d", evt->result);

		event.data.disc_reason = MQTT_SERVICE_DISCONNECT_OTHER;

		if (atomic_get(&disconnect_requested)) {
			LOG_DBG("User triggered disconnect");
			event.data.disc_reason = MQTT_SERVICE_DISCONNECT_USER_REQUEST;
		}

		user_cb(&event);

		break;

	case MQTT_EVT_PUBACK:
		event.type = MQTT_SERVICE_EVT_BROKER_ACK;

		if (evt->result != 0) {
			// LOG_ERR("MQTT PUBACK error %d", evt->result);
			break;
		}

		event.data.ack.message_id = evt->param.puback.message_id;

		LOG_DBG("PUBACK packet id: %u", evt->param.puback.message_id);

		user_cb(&event);

		break;

	case MQTT_EVT_SUBACK:
		LOG_DBG("SUBACK packet");
		break;

	case MQTT_EVT_UNSUBACK:
		LOG_DBG("UNSUBACK packet");
		break;

	case MQTT_EVT_PUBLISH: {
		event.type = MQTT_SERVICE_EVT_DATA_RECEIVED;

		int32_t len;
		int32_t bytes_read;

		len = evt->param.publish.message.payload.len;

		LOG_DBG("MQTT publish received %d, %d bytes", evt->result, len);
		LOG_DBG(" id: %d, qos: %d", evt->param.publish.message_id,
			evt->param.publish.message.topic.qos);

		while (len > 0) {
			bytes_read = mqtt_read_publish_payload(
				&client_ctx, payload,
				len > sizeof(payload) ? sizeof(payload) : len);
			if (bytes_read < 0 && bytes_read != -EAGAIN) {
				// LOG_ERR("Failed to read payload");
				break;
			}

			payload[bytes_read] = '\0';
			LOG_HEXDUMP_DBG(payload, bytes_read, "MQTT message payload:");
			len -= bytes_read;

			event.data.publish.message_id = evt->param.publish.message_id;
			event.data.publish.p_topic_name =
				evt->param.publish.message.topic.topic.utf8;
			event.data.publish.topic_name_length =
				evt->param.publish.message.topic.topic.size;
			event.data.publish.p_msg_data = payload;
			event.data.publish.msg_length = evt->param.publish.message.payload.len;

			user_cb(&event);
		}

		if (evt->param.publish.message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
			struct mqtt_puback_param puback = {.message_id =
								   evt->param.publish.message_id};

			mqtt_publish_qos1_ack(&client_ctx, &puback);
		} else if (evt->param.publish.message.topic.qos == MQTT_QOS_2_EXACTLY_ONCE) {
			struct mqtt_pubrec_param pubrec = {.message_id =
								   evt->param.publish.message_id};

			mqtt_publish_qos2_receive(&client_ctx, &pubrec);
		}

		break;
	}

	case MQTT_EVT_PUBREC: {
		struct mqtt_pubrel_param pubrel = {.message_id = evt->param.pubrec.message_id};

		mqtt_publish_qos2_release(&client_ctx, &pubrel);

		break;
	}

	case MQTT_EVT_PUBREL: {
		struct mqtt_pubcomp_param pubcomp = {.message_id = evt->param.pubrel.message_id};

		mqtt_publish_qos2_complete(&client_ctx, &pubcomp);
	}

	default:
		break;
	}
}

static int32_t mqtt_service_client_connect(void)
{
	int err;

	broker_init();

	err = mqtt_connect(&client_ctx);
	if (err) {
		// LOG_ERR("mqtt_connect, error: %d", err);
		return err;
	}

	return 0;
}

static void mqtt_service_poll(void)
{
	int err;
	struct zsock_pollfd fds[1];
	mqtt_service_event_t event = {.type = MQTT_SERVICE_EVT_DISCONNECTED};

start:
	k_sem_take(&connection_poll_sem, K_FOREVER);
	atomic_set(&connection_poll_active, 1);

	err = mqtt_service_client_connect();
	if (err) {
		goto reset;
	}

	LOG_DBG("MQTT broker connection request sent");

	fds[0].fd = client_ctx.transport.tcp.sock;
	fds[0].events = ZSOCK_POLLIN;

	atomic_set(&broker_disconnected, 0);

	while (true) {
		err = zsock_poll(fds, ARRAY_SIZE(fds), mqtt_keepalive_time_left(&client_ctx));

		/* If poll returns 0 the timeout has expired. */
		if (err == 0) {
			err = mqtt_ping(&client_ctx);
			if (err) {
				// LOG_ERR("Cloud MQTT keepalive ping failed: %d", err);
				break;
			}
			continue;
		}

		if ((fds[0].revents & ZSOCK_POLLIN) == ZSOCK_POLLIN) {
			err = mqtt_input(&client_ctx);
			if (err) {
				// LOG_ERR("Cloud MQTT input error: %d", err);
				if (err == -ENOTCONN) {
					break;
				}
			}

			if (atomic_get(&broker_disconnected) == 1) {
				LOG_DBG("The cloud socket is already closed.");
				break;
			}

			continue;
		}

		if (err < 0) {
			// LOG_ERR("poll() returned an error: %d", err);
			event.data.disc_reason = MQTT_SERVICE_DISCONNECT_OTHER;
			break;
		}

		if ((fds[0].revents & ZSOCK_POLLNVAL) == ZSOCK_POLLNVAL) {
			LOG_DBG("Socket error: POLLNVAL");
			LOG_DBG("The cloud socket was unexpectedly closed.");
			event.data.disc_reason = MQTT_SERVICE_DISCONNECT_INVALID_REQUEST;
			break;
		}

		if ((fds[0].revents & ZSOCK_POLLHUP) == ZSOCK_POLLHUP) {
			LOG_DBG("Socket error: POLLHUP");
			LOG_DBG("Connection was closed by the cloud.");
			event.data.disc_reason = MQTT_SERVICE_DISCONNECT_CLOSED_BY_REMOTE;
			break;
		}

		if ((fds[0].revents & ZSOCK_POLLERR) == ZSOCK_POLLERR) {
			LOG_DBG("Socket error: POLLERR");
			LOG_DBG("Cloud connection was unexpectedly closed.");
			event.data.disc_reason = MQTT_SERVICE_DISCONNECT_OTHER;
			break;
		}
	}

	/* Upon a socket error, disconnect the client and notify the
	 * application. If the client has already been disconnected this has
	 * occurred via a MQTT DISCONNECT event and the application has
	 * already been notified.
	 */
	if (atomic_get(&broker_disconnected) == 0) {
		user_cb(&event);
		err = mqtt_disconnect(&client_ctx);
		if (err) {
			// LOG_ERR("mqtt_disconnect fail, err: %d", err);
		}
	}

reset:
	atomic_set(&connection_poll_active, 0);
	k_sem_take(&connection_poll_sem, K_NO_WAIT);
	goto start;
}

K_THREAD_DEFINE(mqtt_poll_thread, CONFIG_MQTT_SERVICE_THREAD_STACK_SIZE, mqtt_service_poll, NULL,
		NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);
