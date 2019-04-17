/*
 * Bluetooth interface for communication with the turret controller.
 *
 * Some nice bluez docs at:
 *   https://people.csail.mit.edu/albert/bluez-intro/x604.html
 */
#include "bluetooth.h"
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <bluetooth/rfcomm.h>
#include <unistd.h>
#include <stdlib.h>

#include <errno.h>
#include <string.h>

#define SERVER_QUEUE_LENGTH 1
#define RFCOMM_CHANNEL 11
#define SERVER_BUFFER_SIZE 256

/*
 * GUID Generated with uuidgen on vlsi32
 * Only XXXXXXXX-0000-1000-8000-00805F9B34FB are reserved
 * Source: https://www.bluetooth.com/specifications/assigned-numbers/service-discovery
 * (The top 32 bits in BASE_UUID are reserved for current and future revisions)
 *
 * ce025ea4-00d6-44f3-ae1c-a5cba97381fd
 */
const char svc_uuid_bytes[] = {0xce, 0x02, 0x5e, 0xa4, 0x00, 0xd6, 0x44, 0xf3,
                               0xae, 0x1c, 0xa5, 0xcb, 0xa9, 0x73, 0x81, 0xfd};

/*
 * Create, fill, and register the service discovery configuration.
 */
static sdp_session_t*
bluetooth_register_service()
{
	uint8_t rfcomm_channel = RFCOMM_CHANNEL;
	const char *service_name = "DMG Turret Control";
	const char *service_dsc = "Blue Pew pew";
	const char *service_prov = "EC535 Team DMG";
	uuid_t service_uuid, rfcomm_uuid, l2cap_uuid, root_uuid;
	sdp_data_t *channel = NULL;
	sdp_list_t *rfcomm_list = NULL;
	sdp_list_t *proto_list = NULL;
	sdp_list_t *root_list = NULL;
	sdp_list_t *l2cap_list = NULL;
	sdp_list_t *access_proto_list = NULL;

	sdp_record_t *record = sdp_record_alloc();
	if (!record)
	{
		fprintf(stderr, "Unable to allocate SDP record: %s\n", strerror(errno));
		return NULL;
	}

	sdp_uuid128_create(&service_uuid, &svc_uuid_bytes);
	sdp_set_service_id(record, service_uuid);

	/* make the service record publicly browsable */
	sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
	root_list = sdp_list_append(NULL, &root_uuid);
	sdp_set_browse_groups(record, root_list);

	/* set l2cap information */
	sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
	l2cap_list = sdp_list_append(NULL, &l2cap_uuid);
	proto_list = sdp_list_append(NULL, l2cap_list);
 
	/* RFCOMM settings */
	sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
	channel = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
	rfcomm_list = sdp_list_append(NULL, &rfcomm_uuid);
	sdp_list_append(rfcomm_list, channel);

	/* Add the protocol info */
	sdp_list_append(proto_list, rfcomm_list);
	access_proto_list = sdp_list_append(NULL, proto_list);
	sdp_set_access_protos(record, access_proto_list);

	sdp_set_info_attr(record, service_name, service_prov, service_dsc);

	/*
	 * Done packing all of the config into lists. Now actually do the
	 * registration work.
	 */
	int err = 0;
	sdp_session_t *session = NULL;

	session = sdp_connect(BDADDR_ANY, BDADDR_LOCAL, SDP_RETRY_IF_BUSY);
	if (session)
	{
		err = sdp_record_register(session, record, 0);
		if (err == -1)
		{
			fprintf(stderr, "Error in sdp_record_register: %s\n", strerror(errno));
			sdp_close(session);
			session = NULL;
		}
	}
	else
	{
		fprintf(stderr, "Unable to register bluetooth service: %s\n", strerror(errno));
	}

	sdp_data_free(channel);
	sdp_list_free(l2cap_list, 0);
	sdp_list_free(rfcomm_list, 0);
	sdp_list_free(root_list, 0);
	sdp_list_free(access_proto_list, 0);

	return session;
}

/*
 * Wait for connections and handle their requests. Only supports up to one
 * connection at a time.
 */
static int
wait_for_connections(int server_socket, BluetoothMessageHandler message_handler)
{
	int client_connection = -1;
	struct sockaddr_rc peer_addr = { 0 };
	socklen_t peer_addr_size = sizeof(peer_addr);
	char buf[SERVER_BUFFER_SIZE] = { 0 };
	int bytes_read;

	/* Wait for new connections */
	fprintf(stderr, "Waiting for a connection\n");
	while ((client_connection = accept(server_socket, (struct sockaddr *)&peer_addr, &peer_addr_size)) != -1)
	{
		ba2str(&peer_addr.rc_bdaddr, buf);
		fprintf(stderr, "Accepted connection from %s\n", buf);
		memset(buf, 0, sizeof(buf));

		/* read data from the client */
		while ((bytes_read = read(client_connection, buf, sizeof(buf))) > 0)
		{
			if (message_handler((unsigned char*)buf, bytes_read) != 0)
			{
				fprintf(stderr, "Unable to handle message: %.*s\n", bytes_read, buf);
			}
		}
		close(client_connection);
		fprintf(stderr, "Disconnected\n");
	}

	if (client_connection == -1)
	{
		fprintf(stderr, "Unable to accept on socket: %s\n", strerror(errno));
	}

	return 0;
}

int
run_rfcomm_server(BluetoothMessageHandler message_handler)
{
	struct sockaddr_rc local_address = { 0 };
	int server_socket = -1;
	int ret;
	sdp_session_t *sdp_session = NULL;

	server_socket = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	if (server_socket == -1)
	{
		fprintf(stderr, "Unable to create socket: %s\n", strerror(errno));
		goto cleanup;
	}

	local_address.rc_family = AF_BLUETOOTH;
	local_address.rc_bdaddr = *BDADDR_ANY;
	local_address.rc_channel = (uint8_t)RFCOMM_CHANNEL;
	ret = bind(server_socket, (struct sockaddr *)&local_address, sizeof(local_address));
	if (ret == -1)
	{
		fprintf(stderr, "Unable to bind socket: %s\n", strerror(errno));
		goto cleanup;
	}

	ret = listen(server_socket, SERVER_QUEUE_LENGTH);
	if (ret == -1)
	{
		fprintf(stderr, "Unable to listen to socket: %s\n", strerror(errno));
		goto cleanup;
	}

	sdp_session = bluetooth_register_service();
	if (!sdp_session)
	{
		/* Specific error message already printed */
		goto cleanup;
	}

	wait_for_connections(server_socket, message_handler);

cleanup:
	if (sdp_session)
	{
		sdp_close(sdp_session);
	}
	if (server_socket != -1)
	{
		close(server_socket);
	}
	return 0;
}

