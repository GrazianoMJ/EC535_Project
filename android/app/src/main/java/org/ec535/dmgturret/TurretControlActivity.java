package org.ec535.dmgturret;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.os.Parcelable;
import android.speech.RecognitionListener;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;
import android.support.annotation.NonNull;
import android.support.annotation.RequiresApi;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.util.Log;
import android.view.View;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.UUID;

public class TurretControlActivity extends AppCompatActivity
        implements RecognitionListener, IBluetoothSetupEventListener {

    private Toast mToast;
    private EditText mTextBox;
    private TextView mLabel;
    private Intent mSpeechIntent;
    private boolean mTurrentConnected;
    private boolean mTurrentDiscovered;
    private boolean mCmdButtonIsUp = true;
    private boolean mHasEarlyResults = false;
    private SpeechRecognizer mSpeechRecognizer;
    private BluetoothAdapter mBluetoothAdapter;
    private BluetoothDevice mTurretBluetoothDevice;
    private BluetoothClient mBluetoothClient;
    private BluetoothConnector mBluetoothConnector;
    private ArrayList<String> mVoiceCaptures;
    // TODO: Don't rely on hardcoding MAC address
    private static final String TURRET_MAC_ADDRESS = "00:03:19:50:28:AE";
    private static final int REQUEST_ENABLE_BT_ID = 299;
    private static final int PERMISSIONS_REQUEST_RECORD_AUDIO_ID = 300;
    private static final UUID TURRET_UUID = UUID.fromString("ce025ea4-00d6-44f3-ae1c-a5cba97381fd");
    private static final String TAG = TurretControlActivity.class.getName();


    private ArrayList<BluetoothDevice> mDeviceList = new ArrayList<>();

    private final BroadcastReceiver mBroadcastReceiver = new BroadcastReceiver() {
        @RequiresApi(api = Build.VERSION_CODES.KITKAT)
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (BluetoothDevice.ACTION_FOUND.equals(action)) {
                BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                String deviceName = device.getName();
                String deviceMAC = device.getAddress();
                Log.d(TAG, String.format("Discovered %s:%s over Bluetooth",
                        deviceName, deviceMAC));
                mDeviceList.add(device);
                mToast.setText("Scanning for turret...");
                mToast.show();
            } else if (BluetoothAdapter.ACTION_DISCOVERY_FINISHED.equals(action)) {
                Log.d(TAG, "Bluetooth discovery finished");
                if (!mDeviceList.isEmpty()) {
                    // search for turret by service ID disabled for now
                    //BluetoothDevice dev = mDeviceList.remove(0);
                    //boolean result = dev.fetchUuidsWithSdp();
                    for (BluetoothDevice dev : mDeviceList) {
                        if (dev.getAddress().equals(TURRET_MAC_ADDRESS)) {
                            mTurretBluetoothDevice = dev;
                            mTurrentDiscovered = true;
                            connectToTurret();
                            break;
                        }
                    }

                }
                if (!mTurrentDiscovered) {
                    mToast.setText("Turret not found!");
                    mToast.show();
                }
            } else if (BluetoothDevice.ACTION_UUID.equals(action)) {
                Log.d(TAG, "SDP asysnc call results ready");
                BluetoothDevice devExtra = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                Parcelable[] uuidExtra = intent.getParcelableArrayExtra(BluetoothDevice.EXTRA_UUID);
                if (uuidExtra != null) {
                    for (Parcelable uuid : uuidExtra) {
                        Log.d(TAG, String.format("%s:%s has service ID %s detected",
                                devExtra.getName(), devExtra.getAddress(), uuid.toString()));
                        if (uuid.toString().equals(TURRET_UUID.toString()))  {
                            mTurretBluetoothDevice = devExtra;
                            Log.d(TAG, "Connecting to turret..");
                            mTurrentDiscovered = true;
                            //pairWithTurret(UUID.fromString(uuidExtra[0].toString()));
                            break;
                        }
                    }
                }
                mToast.setText("Scanning for turret");
                if (!mDeviceList.isEmpty()) {
                    BluetoothDevice device = mDeviceList.remove(0);
                    boolean result = device.fetchUuidsWithSdp();
                }

            }
        }
    };

    protected void requestForRuntimeBluetoothPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            int permissionCheck = checkSelfPermission("Manifest.permission.ACCESS_FINE_LOCATION");
            permissionCheck += checkSelfPermission("Manifest.permission.ACCESS_COARSE_LOCATION");
            if (permissionCheck != 0) {
                this.requestPermissions(new
                        String[]{Manifest.permission.ACCESS_FINE_LOCATION,
                        Manifest.permission.ACCESS_COARSE_LOCATION}, 1001);
            }
        }
    }

    @Override
    public void onConnectionFailed(String reason) {
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mToast.setText("Failed to connect to turret");
                mToast.show();
            }
        });
    }

    @Override
    public void onBluetoothConnected(BluetoothSocket socket) {
        // create bluetooth client to establish connection with turret
        Log.d(TAG, String.format("Connected to turret %s:%s",
                mTurretBluetoothDevice.getName(), mTurretBluetoothDevice.getAddress()));
        mBluetoothClient = new BluetoothClient(socket, mTurretBluetoothDevice, TAG);
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mToast.setText("Connected to turret!");
                mToast.show();
            }
        });
        Log.d(TAG, "Spawning bluetooth client thread..");
        mTurrentConnected = true;
        mBluetoothClient.run();
    }

    protected void connectToTurret() {
        if (mTurretBluetoothDevice == null)
            return;
        mToast.setText("Connecting to turret...");
        mToast.show();
        mBluetoothConnector = new BluetoothConnector(
                mTurretBluetoothDevice,
                TurretControlActivity.TURRET_UUID, TAG, this);
        mBluetoothConnector.start();
    }

    @SuppressLint({"ShowToast", "ClickableViewAccessibility"})
    @RequiresApi(api = Build.VERSION_CODES.KITKAT)
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        final ImageButton cmdButton = findViewById(R.id.commandButton);
        cmdButton.setImageResource(R.drawable.button_normal_small);
        cmdButton.setOnClickListener(new View.OnClickListener() {
            @SuppressLint("SetTextI18n")
            @Override
            public void onClick(View v) {
                if (!mTurrentConnected) {
                    mToast.setText("Turret not connected yet!");
                    mToast.show();
                    return;
                }
                if (mCmdButtonIsUp) {
                    cmdButton.setImageResource(R.drawable.button_pressed_small);
                    Log.d(TAG, "Command button pressed down");
                    mSpeechRecognizer.startListening(mSpeechIntent);
                    mTextBox.setText("[Waiting for command...]");
                    mTextBox.setTextColor(Color.GRAY);
                    mToast.setText("Say your command now...");
                    mToast.show();
                } else {
                    cmdButton.setImageResource(R.drawable.button_normal_small);
                    Log.d(TAG, "Command button released");
                    mToast.setText("Processing your command...");
                    mToast.show();
                    if (mHasEarlyResults) {
                        mHasEarlyResults = false;
                        tryProcessVoiceInput(mVoiceCaptures);
                    } else
                        mSpeechRecognizer.stopListening();
                }
                mCmdButtonIsUp = !mCmdButtonIsUp;
            }
        });
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread t, Throwable e) {
                Log.e(TAG, "Fatal uncaught exception " + e.toString());
            }
        });
        mTextBox = (EditText) findViewById(R.id.textBox);
        mLabel = (TextView) findViewById(R.id.textLabel);
        mToast = Toast.makeText(this, "", Toast.LENGTH_SHORT);
        mLabel.setText(R.string.label1);
        // check if we have permissions for recording user audio
        /**
         * ==================================
         * SPEECH RECOGNITION SETUP
         * ==================================
         */
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                != PackageManager.PERMISSION_GRANTED) {
            // We don't have permissions for this run
            // we should check if we need to justify this capability
            Log.d(TAG, "Record audio recording permissions not granted!");
            Log.d(TAG, "Requesting audio recording permissions");
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.RECORD_AUDIO},
                    PERMISSIONS_REQUEST_RECORD_AUDIO_ID);
            if (ActivityCompat.shouldShowRequestPermissionRationale(
                    this, Manifest.permission.RECORD_AUDIO)) {
                // provide an explanation through some textview or something else
            } else {
                // lets just request permissions

            }
        } else {
            Log.d(TAG, "Setting up speech recognition");
            setupSpeechRecognition();
        }
        /**
         * ===================================
         *  BLUETOOTH SETUP
         *  ==================================
         */
        mBluetoothClient = null;
        mBluetoothConnector = null;
        requestForRuntimeBluetoothPermissions();
        // Register for device found broadcasts over bluetooth
        IntentFilter filter = new IntentFilter(BluetoothDevice.ACTION_FOUND);
        filter.addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED);
        filter.addAction(BluetoothDevice.ACTION_UUID);
        registerReceiver(mBroadcastReceiver, filter);
        // Make sure bluetooth is enabled
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        if (mBluetoothAdapter == null) {
            // TDOO: exit app?
            Log.d(TAG, "Device has not bluetooth support");
        }
        if (mBluetoothAdapter.isEnabled()) {
            startBluetoothScan();
        } else {
            Log.d(TAG, "Requesting user enable bluetooth adapter..");
            Intent enableBtIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT_ID);
        }
    }

    protected void startBluetoothScan() {
        // start scanning for the turret device
        boolean devScanStarted = mBluetoothAdapter.startDiscovery();
        if (devScanStarted) {
            Log.d(TAG, "Bluetooth scanning started..");
        } else {
            Log.d(TAG, "Failed to start bluetooth discovery");
            // TODO: show error or allow user to try again
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_ENABLE_BT_ID) {
            if (resultCode == RESULT_OK) {
                Log.d(TAG, "Bluetooth enabled by user");
                startBluetoothScan();
            } else if (resultCode == RESULT_CANCELED) {
                // TDOO: Handle cancellation by user
                Log.d(TAG, "Bluetooth disabled by user");
            }
        }
    }

    protected void setupSpeechRecognition() {
        mSpeechIntent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
        mSpeechIntent.putExtra(RecognizerIntent.EXTRA_LANGUAGE_PREFERENCE, "en");
        mSpeechIntent.putExtra(RecognizerIntent.EXTRA_CALLING_PACKAGE,
                getApplicationContext().getPackageName());
        mSpeechIntent.putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL,
                RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
        mSpeechIntent.putExtra(RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS,
                150000);
        mSpeechIntent.putExtra(
                RecognizerIntent.EXTRA_SPEECH_INPUT_POSSIBLY_COMPLETE_SILENCE_LENGTH_MILLIS,
                150000);
        mSpeechIntent.putExtra(
                RecognizerIntent.EXTRA_SPEECH_INPUT_MINIMUM_LENGTH_MILLIS, 150000
        );
        mSpeechIntent.putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, 10);
        //mSpeechIntent.putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true);
        mSpeechRecognizer = SpeechRecognizer.createSpeechRecognizer(
                getApplicationContext()
        );
        mSpeechRecognizer.setRecognitionListener(this);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        switch (requestCode) {
            case PERMISSIONS_REQUEST_RECORD_AUDIO_ID:
                if (grantResults.length > 0 &&
                        grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    Log.d(TAG, "Audio recording permissions granted!");
                    Log.d(TAG, "Setting up speech recognition");
                    setupSpeechRecognition();
                }
                break;
            default:
                break;
        }
    }


    @Override
    public void onReadyForSpeech(Bundle params) {
        Log.d(TAG, "Ready for speech");
    }

    @Override
    public void onBeginningOfSpeech() {
        Log.d(TAG, "Beginning to listen to speech");
    }

    @Override
    public void onRmsChanged(float rmsdB) {
        //System.out.println("RMS changes");
    }

    @Override
    public void onBufferReceived(byte[] buffer) {
        Log.d(TAG, String.format("Got voice data buffer of size %d\n",
                buffer.length));
    }

    protected void resetSpeechRecognition() {
        mSpeechRecognizer.destroy();
        mSpeechRecognizer = SpeechRecognizer.createSpeechRecognizer(
                getApplicationContext()
        );
        mSpeechRecognizer.setRecognitionListener(this);
    }

    @Override
    public void onEndOfSpeech() {
        Log.d(TAG, "End of speech");
        //restartSpeechRecognition();
    }

    @Override
    public void onError(int error) {
        Log.d(TAG, String.format("Error %d occurred\n", error));
        switch (error) {
            case SpeechRecognizer.ERROR_AUDIO:
                Log.d(TAG,"Error recording audio");
                break;
            case SpeechRecognizer.ERROR_CLIENT:
                Log.d(TAG, "Client side error occurred");
                break;
            case SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS:
                Log.d(TAG,"Insufficient permissions error occurred");
                break;
            case SpeechRecognizer.ERROR_SPEECH_TIMEOUT:
                Log.d(TAG, "Resetting speech recognition..");
                resetSpeechRecognition();
                break;
            default:
                Log.d( TAG, String.format("Encountered error code %d\n", error));
                break;
        }
    }

    protected ArrayList<String> getWordList(ArrayList<String> sentenceList) {
        // returns flattened list of words from sentences
        ArrayList<String> output = new ArrayList<>();
        for (String str : sentenceList) {
            output.addAll(Arrays.asList(str.split(" ")));
        }
        return output;
    }

    @SuppressLint({"DefaultLocale", "SetTextI18n"})
    protected void tryProcessVoiceInput(ArrayList<String> voiceInput) {
        VoiceCommand cmd = VoiceCommand.getCommand(
                getWordList(voiceInput)
        );
        switch (cmd.getCommandName()) {
            case FIRE:
                mTextBox.setText("Fire payload");
                break;
            case PRIME:
                mTextBox.setText("Prime payload");
                break;
            case TILT_UP:
                mTextBox.setText(String.format("Tilt turret up by %d tick(s)",
                        cmd.getArgument()));
                break;
            case TILT_DOWN:
                mTextBox.setText(String.format("Tilt turret down by %d tick(s)",
                        cmd.getArgument()));
                break;
            case ROTATE_LEFT:
                mTextBox.setText(String.format("Rotate turret left by %d tick(s)",
                        cmd.getArgument()));
                break;
            case ROTATE_RIGHT:
                mTextBox.setText(String.format("Rotate turret right by %d tick(s)",
                        cmd.getArgument()));
                break;
            case INVALID:
                mTextBox.setText("Invalid command");
                break;
        }
        mTextBox.setTextColor(Color.BLACK);
        // send command to turret
        if (mBluetoothClient != null &&
                cmd.getCommandName() != VoiceCommand.CommandName.INVALID) {
            mBluetoothClient.write(cmd.toBytes());
        }
    }

    @Override
    public void onResults(Bundle results) {
        Log.d(TAG, "Speech capture results ready");
        mVoiceCaptures = results.getStringArrayList(
                SpeechRecognizer.RESULTS_RECOGNITION
        );
        StringBuilder strBuilder = new StringBuilder();
        if (mVoiceCaptures != null) {
            for (int i = 0; i < mVoiceCaptures.size(); i++) {
                Log.d(TAG, String.format("Word capture [%d]: %s\n",
                        i + 1, mVoiceCaptures.get(i)));
                if (strBuilder.length() > 0)
                    strBuilder.append(", ");
                strBuilder.append(mVoiceCaptures.get(i));
            }
        }
        if (mCmdButtonIsUp)
            tryProcessVoiceInput(mVoiceCaptures);
        else
            mHasEarlyResults = true;
    }

    protected void onDestroy() {
        super.onDestroy();
        // Unregister broadcast receiver
        unregisterReceiver(mBroadcastReceiver);
    }

    @Override
    public void onPartialResults(Bundle partialResults) {
        Log.d(TAG, "Partial results received");
    }

    @Override
    public void onEvent(int eventType, Bundle params) {
        Log.d(TAG, String.format("Event type %d occurred\n", eventType));
    }
}
