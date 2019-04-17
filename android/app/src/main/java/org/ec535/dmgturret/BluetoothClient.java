package org.ec535.dmgturret;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.util.Log;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;

public class BluetoothClient extends Thread{
    private String mLogTag;

    private BluetoothDevice mDevice;
    private BluetoothSocket mSocket;
    private InputStream mInputStream;
    private OutputStream mOutputStream;

    public static final int READ_BUFFER_SIZE = 1024;

    public BluetoothClient(BluetoothSocket socket, BluetoothDevice device, String logTag) {
        mSocket = socket;
        mLogTag = logTag;
        mDevice = device;
        mInputStream = null;
        mOutputStream = null;

        try {
            mInputStream = mSocket.getInputStream();
        } catch (IOException e) {
            Log.e(mLogTag, "Failed to acquire socket input stream");
        }
        try {
            mOutputStream = socket.getOutputStream();
        } catch (IOException e) {
            Log.e(mLogTag, "Failed to acquire socket output stream");
        }
    }

    public void run() {
        int readCount = 0;
        byte[] mReadBuffer = new byte[READ_BUFFER_SIZE];

        while (true) {
            try {
                readCount = mInputStream.read(mReadBuffer);
                Log.d(mLogTag, String.format("Received %d byte(s) from %s: %s",
                        readCount, Arrays.toString(mReadBuffer), mDevice.getName()));
            } catch (IOException e) {
                Log.e(mLogTag, "Input stream not longer connected");
                break;
            }
        }
    }

    public void write(byte[] bytes) {
        try {
            Log.d(mLogTag, String.format("Sending %d byte(s) to %s: %s",
                    bytes.length,
                    mDevice.getName(),
                    Arrays.toString(bytes)));
            mOutputStream.write(bytes);
        } catch (IOException e) {
            Log.e(mLogTag, String.format("Failed to send data to device %s", mDevice.getName()));
        }
    }

    public void shutdownConnection() {
        try {
            mSocket.close();
        } catch (IOException e) {
            Log.e(mLogTag, "Failed to close bluetooth socket");
        }
    }

}
