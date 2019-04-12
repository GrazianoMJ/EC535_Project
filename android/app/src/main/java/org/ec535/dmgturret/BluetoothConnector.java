package org.ec535.dmgturret;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.util.Log;

import java.io.IOException;
import java.util.UUID;

public class BluetoothConnector extends Thread {
    private String mLogTag;
    private BluetoothSocket mSocket;
    private BluetoothDevice mDevice;
    private IBluetoothSetupEventListener mSetupListener;

    public BluetoothConnector(BluetoothDevice turretBtDev,
                              UUID uuid,
                              String logTag,
                              IBluetoothSetupEventListener setupListener) {
        mSocket = null;
        mDevice = turretBtDev;
        mLogTag = logTag;
        mSetupListener = setupListener;
        try {
            mSocket = mDevice.createRfcommSocketToServiceRecord(uuid);
        } catch (IOException e) {
            Log.e(mLogTag, "Failed to initialize bluetooth socket");
        }
    }

    public void run() {
        try {
            mSocket.connect();
        } catch (IOException connectException) {
            Log.e(mLogTag, String.format("Failed to connect to device %s", mDevice.getName()));
            try {
                mSocket.close();
            } catch (IOException closeException) {
                Log.e(mLogTag, "Failed to close bluetooth socket");
                mSetupListener.onConnectionFailed(closeException.toString());
            }
        }
        // Let the main listener (main activity know)
        mSetupListener.onBluetoothConnected(mSocket);
    }
}
