package com.zg335.host;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

public final class MainActivity extends Activity {
    static {
        System.loadLibrary("zg335host");
    }

    private native String runProbe();

    private void runAndShow(TextView result) {
        String value = runProbe();
        result.setText(value);
        Log.i("ZG335HOST", value);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final TextView result = new TextView(this);
        result.setText("ready");
        result.setTextSize(18.0f);
        result.setGravity(Gravity.CENTER_HORIZONTAL);

        Button run = new Button(this);
        run.setText("Run ZG335 probe");
        run.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                runAndShow(result);
            }
        });

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(48, 48, 48, 48);
        layout.addView(run);
        layout.addView(result);
        setContentView(layout);
        runAndShow(result);
    }
}
