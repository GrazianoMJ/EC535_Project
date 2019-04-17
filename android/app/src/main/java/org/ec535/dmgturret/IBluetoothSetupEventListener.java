package org.ec535.dmgturret;

import android.bluetooth.BluetoothSocket;

public interface IBluetoothSetupEventListener {
    void onConnectionFailed(String reason);
    void onBluetoothConnected(BluetoothSocket socket);
}

