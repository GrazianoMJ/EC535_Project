#ifndef RC_BLUETOOTH_H
#define RC_BLUETOOTH_H
#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

typedef int (*BluetoothMessageHandler)(const unsigned char* message, size_t message_size);

int run_rfcomm_server(BluetoothMessageHandler message_handler);

#endif /* RC_BLUETOOTH_H */
