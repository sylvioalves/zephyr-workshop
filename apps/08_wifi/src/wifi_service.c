#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/dhcpv4.h>

#include "wifi_service.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_service, CONFIG_LOG_DEFAULT_LEVEL);

static struct wifi_data wifi;

struct wifi_data {
	struct net_mgmt_event_callback wifi_mgmt_cb;
	struct k_sem connect_sem;
	bool status;
};

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
				    struct net_if *iface)
{
	struct wifi_data *wifi_cb = CONTAINER_OF(cb, struct wifi_data, wifi_mgmt_cb);
	const struct wifi_status *status = cb->info;

	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		if (status->status == 0) {
			wifi_cb->status = true;
		} else {
			wifi_cb->status = false;
		}
		k_sem_give(&wifi_cb->connect_sem);
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		wifi_cb->status = false;
		k_sem_give(&wifi_cb->connect_sem);
		break;
	default:
		break;
	}
}

static uint8_t wifi_ssid[] = CONFIG_WIFI_SSID;
static size_t wifi_ssid_len = sizeof(CONFIG_WIFI_SSID) - 1;
static uint8_t wifi_psk[] = CONFIG_WIFI_PSK;
static size_t wifi_psk_len = sizeof(CONFIG_WIFI_PSK) - 1;

void net_connect(struct net_if *iface)
{
	int err;

	struct wifi_connect_req_params params = {
		.ssid = wifi_ssid,
		.ssid_length = wifi_ssid_len,
		.psk = wifi_psk,
		.psk_length = wifi_psk_len,
		.channel = WIFI_CHANNEL_ANY,
		.security = wifi_psk_len > 0 ? WIFI_SECURITY_TYPE_PSK : WIFI_SECURITY_TYPE_NONE,
	};

	while (true) {
		err = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
		if (err == -EALREADY) {
			wifi.status = true;
			break;
		} else if (err) {
			LOG_ERR("Failed to request Wi-Fi connect: %d", err);
			goto retry;
		}

		/* wait for notification from connect request */
		err = k_sem_take(&wifi.connect_sem, K_SECONDS(2));
		if (err) {
			// LOG_ERR("Timeout waiting for Wi-Fi connection");
			goto retry;
		}

		/* verify connect status */
		if (wifi.status == true) {
			LOG_INF("Successfully connected to Wi-Fi");
			break;
		} else {
			goto retry;
		}

	retry:
		k_msleep(1000);
	}
}

static void wait_for_iface_up_poll(struct net_if *iface)
{
	while (!net_if_is_up(iface)) {
		k_sleep(K_MSEC(100));
	}
}

void wifi_init(void)
{
	struct net_if *iface = net_if_get_default();

	k_sem_init(&wifi.connect_sem, 0, 1);

	LOG_INF("Waiting for interface to be up");
	wait_for_iface_up_poll(iface);

	net_mgmt_init_event_callback(
		&wifi.wifi_mgmt_cb, wifi_mgmt_event_handler,
		(NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT));
	net_mgmt_add_event_callback(&wifi.wifi_mgmt_cb);
}

void wifi_connect(void)
{
	struct net_if *iface = net_if_get_default();

	LOG_INF("Connecting to Wi-Fi");
	net_connect(iface);
}

bool wifi_connected(void)
{
	return wifi.status;
}
