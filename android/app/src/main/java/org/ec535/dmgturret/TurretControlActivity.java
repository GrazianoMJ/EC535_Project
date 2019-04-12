package org.ec535.dmgturret;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.speech.RecognitionListener;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;
import android.support.annotation.NonNull;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.Snackbar;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.util.Log;
import android.view.View;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.EditText;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Arrays;

public class TurretControlActivity extends AppCompatActivity implements RecognitionListener {

    private EditText mTextBox;
    private TextView mLabel;
    private Intent mSpeechIntent;
    private SpeechRecognizer mSpeechRecognizer;
    private static final int PERMISSIONS_REQUEST_RECORD_AUDIO_ID = 300;
    private static final String TAG = TurretControlActivity.class.getName();

    private enum CommandOP {
        FIRE,
        PRIME,
        TILT_UP,
        TILT_DOWN,
        INVALID
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        FloatingActionButton fab = findViewById(R.id.fab);
        fab.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Snackbar.make(view, "Replace with your own action",
                        Snackbar.LENGTH_LONG)
                        .setAction("Action", null).show();
            }
        });
        mTextBox = (EditText) findViewById(R.id.textBox);
        mLabel = (TextView) findViewById(R.id.textLabel);
        mLabel.setText(R.string.label1);
        // check if we have permissions for recording user audio
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

    }

    protected void setupSpeechRecognition() {
        mSpeechIntent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
        mSpeechIntent.putExtra(RecognizerIntent.EXTRA_LANGUAGE_PREFERENCE, "en");
        mSpeechIntent.putExtra(RecognizerIntent.EXTRA_CALLING_PACKAGE,
                getApplicationContext().getPackageName());
        mSpeechIntent.putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL,
                RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
        mSpeechIntent.putExtra(RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS,
                1500);
        mSpeechIntent.putExtra(
                RecognizerIntent.EXTRA_SPEECH_INPUT_POSSIBLY_COMPLETE_SILENCE_LENGTH_MILLIS,
                1500);
        mSpeechIntent.putExtra(
                RecognizerIntent.EXTRA_SPEECH_INPUT_MINIMUM_LENGTH_MILLIS, 1500
        );
        mSpeechIntent.putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, 5);
        mSpeechIntent.putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true);
        mSpeechRecognizer = SpeechRecognizer.createSpeechRecognizer(
                getApplicationContext()
        );
        mSpeechRecognizer.setRecognitionListener(this);
        mSpeechRecognizer.startListening(mSpeechIntent);
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
        //mSpeechRecognizer.stopListening();
    }

    @Override
    protected void onResume() {
        super.onResume();
        //mSpeechRecognizer.startListening(mSpeechIntent);
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
        mSpeechRecognizer.startListening(mSpeechIntent);
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
                mTextBox.setText(String.format("Tilt turret up by %d degree(s)",
                        cmd.getArgument()));
                break;
            case TILT_DOWN:
                mTextBox.setText(String.format("Tilt turret down by %d degree(s)",
                        cmd.getArgument()));
                break;
            case INVALID:
                mTextBox.setText("Invalid command");
                break;
        }
    }

    @Override
    public void onResults(Bundle results) {
        Log.d(TAG, "Speech capture results ready");
        ArrayList<String> voiceCaptures = results.getStringArrayList(
                SpeechRecognizer.RESULTS_RECOGNITION
        );
        StringBuilder strBuilder = new StringBuilder();
        if (voiceCaptures != null) {
            for (int i = 0; i < voiceCaptures.size(); i++) {
                Log.d(TAG, String.format("Word capture [%d]: %s\n",
                        i + 1, voiceCaptures.get(i)));
                if (strBuilder.length() > 0)
                    strBuilder.append(", ");
                strBuilder.append(voiceCaptures.get(i));
            }
        }
        // mTextBox.setText(strBuilder.toString());
        tryProcessVoiceInput(voiceCaptures);
        mSpeechRecognizer.startListening(mSpeechIntent);
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
