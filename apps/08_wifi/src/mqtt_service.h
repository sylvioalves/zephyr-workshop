#ifndef __MQTT_SERVICE_H__
#define __MQTT_SERVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief MQTT service module event type
 *
 */
typedef enum _mqtt_service_event_type_t {
	/** MQTT client has successfully connected to MQTT broker.*/
	MQTT_SERVICE_EVT_CONNECTED,

	/** MQTT client has successfully disconnected from MQTT broker.*/
	MQTT_SERVICE_EVT_DISCONNECTED,

	/** MQTT broker has acknowledged message published by client */
	MQTT_SERVICE_EVT_BROKER_ACK,

	/** MQTT client has received data published by the broker */
	MQTT_SERVICE_EVT_DATA_RECEIVED

} mqtt_service_event_type_t;

/**
 * @brief MQTT service disconnect reason type
 *
 */
typedef enum _mqtt_service_disconnect_reason_t {
	/** MQTT client has disconnected due to user request.*/
	MQTT_SERVICE_DISCONNECT_USER_REQUEST,

	/** MQTT broker has disconnected client */
	MQTT_SERVICE_DISCONNECT_CLOSED_BY_REMOTE,

	/** MQTT client has sent invalid request */
	MQTT_SERVICE_DISCONNECT_INVALID_REQUEST,

	/** MQTT client has disconnected unexpectedly. This can be due to network
	 *  losing connection or connection being dropped by network stack
	 */
	MQTT_SERVICE_DISCONNECT_OTHER

} mqtt_service_disconnect_reason_t;

/**
 * @brief MQTT service acknowledgement data
 *
 */
typedef struct _mqtt_service_broker_ack_t {
	/** Message ID of message being acknowledged */
	uint16_t message_id;
} mqtt_service_broker_ack_t;

/**
 * @brief MQTT service publish data
 *
 */
typedef struct _mqtt_service_publish_data_t {
	/** Message ID of incoming message*/
	uint16_t message_id;

	/** Pointer to topic source */
	const uint8_t *p_topic_name;

	/** Length of topic source */
	uint32_t topic_name_length;

	/** Pointer to received message data */
	uint8_t *p_msg_data;

	/** Length of received message */
	uint32_t msg_length;
} mqtt_service_publish_data_t;

/**
 * @brief MQTT service module event structure
 *
 */
typedef struct _mqtt_service_event_t {
	mqtt_service_event_type_t type;

	union {
		/** Indicates whether broker has stored session state for the client.
		 *  Parameter is set on MQTT_SERVICE_EVT_CONNECTED
		 */
		uint8_t session_present;

		/** Indicates MQTT client disconnect reason.
		 *  Parameter is set on MQTT_SERVICE_EVT_DISCONNECTED
		 */
		mqtt_service_disconnect_reason_t disc_reason;

		/** Parameter is set on MQTT_SERVICE_EVT_BROKER_ACK */
		mqtt_service_broker_ack_t ack;

		/** Contains data received from broker
		 *  Parameter is set on MQTT_SERVICE_EVT_DATA_RECEIVED
		 */
		mqtt_service_publish_data_t publish;
	} data;

} mqtt_service_event_t;

/**
 * @brief User provided callback to handle MQTT service module events
 *
 */
typedef void (*mqtt_service_evt_cb_t)(mqtt_service_event_t *evt);

//---------------------- PUBLIC FUNCTION PROTOTYPES --------------------------

//<mqtt_service_init_start>

/**
 * @brief   Initialize MQTT service module
 * @details Configures MQTT client with predefined RX and TX buffers.
 *          Fetches unique hardware ID to configure client ID string.
 *
 * @param cb User callback to receive events from MQTT service module
 * @return  int32_t - 0 if everything executed correctly, otherwise an
 *                  appropriate negative error code.
 */
int32_t mqtt_service_init(mqtt_service_evt_cb_t cb);

//<mqtt_service_init_end>

//<mqtt_service_connection_start>

/**
 * @brief   Connect to MQTT broker
 *
 * @return  int32_t - 0 if everything executed correctly, otherwise an
 *                  appropriate negative error code.
 */
int32_t mqtt_service_connect(void);

/**
 * @brief   Disonnect from MQTT broker
 *
 * @return  int32_t - 0 if everything executed correctly, otherwise an
 *                  appropriate negative error code.
 */
int32_t mqtt_service_disconnect(void);

//<mqtt_service_connection_end>

//<mqtt_service_data_start>

/**
 * @brief       Publishes payload to specified topic.
 * @details     Prepares an outgoing message to MQTT broker.
 *              Topic, payload data and QoS have to be provided.
 *              Generated message ID is returned on success.
 *
 * @param p_topic       pointer to string containing full topic name
 * @param topic_len     length of topic name
 * @param p_payload     pointer to buffer containing payload data
 * @param payload_len   size of payload buffer
 * @param qos           quality of service level
 * @return int32_t -    positive number indicating message ID, otherwise an
 *                      appropriate negative error code.
 */
int32_t mqtt_service_publish(const uint8_t *p_topic, uint32_t topic_len, uint8_t *p_payload,
			     uint32_t payload_len, uint8_t qos);

/**
 * @brief           Subscribes to topic with specified QoS
 *
 * @param p_topic   Pointer to topic name
 * @param topic_len length of topic name
 * @param qos       quality of service level
 * @return int32_t - 0 if everything executed correctly, otherwise an
 *                  appropriate negative error code.
 */
int32_t mqtt_service_subscribe(const uint8_t *p_topic, uint32_t topic_len, uint8_t qos);

/**
 * @brief           Unsubscribes from previously subscribed
 *
 * @param p_topic   Pointer to topic name
 * @param topic_len length of topic name
 * @return int32_t - 0 if everything executed correctly, otherwise an
 *                  appropriate negative error code.
 */
int32_t mqtt_service_unsubscribe(const uint8_t *p_topic, uint32_t topic_len);

//<mqtt_service_data_end>

#ifdef __cplusplus
}
#endif

#endif // __MQTT_SERVICE_H__
