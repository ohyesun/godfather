package com.zg335.host;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Log;
import android.view.ViewGroup;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public final class MainActivity extends Activity {
    static {
        System.loadLibrary("zg335host");
    }

    private native String runProbe();
    private WebView webView;
    private ZgBridge bridge;

    private String probe() {
        String value = runProbe();
        Log.i("ZG335HOST", value);
        return value;
    }

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        WebView.setWebContentsDebuggingEnabled(true);

        webView = new WebView(this);
        WebView web = webView;
        WebSettings settings = web.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setDatabaseEnabled(true);
        settings.setAllowFileAccess(true);
        settings.setAllowContentAccess(true);
        settings.setAllowFileAccessFromFileURLs(true);
        settings.setAllowUniversalAccessFromFileURLs(true);
        settings.setMediaPlaybackRequiresUserGesture(false);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
        settings.setCacheMode(WebSettings.LOAD_DEFAULT);

        web.setWebViewClient(new WebViewClient());
        web.setWebChromeClient(new WebChromeClient());
        web.addJavascriptInterface(new Bridge(), "ZG335Host");

        setContentView(web, new ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT));

        probe();
        runBridgeByteBuffer();
        web.loadUrl("file:///android_asset/index.html");
    }

    private void runBridgeByteBuffer() {
        bridge = new ZgBridge();
        if (!bridge.create(4096, 65536, 4096, 0)) {
            Log.e("ZG335BRIDGE", "create failed");
            bridge = null;
            return;
        }

        ByteBuffer frame = ByteBuffer.allocateDirect(24).order(ByteOrder.LITTLE_ENDIAN);
        frame.putShort(0, (short) 0x5A47);
        frame.put(2, (byte) 1);       /* version */
        frame.put(3, (byte) 3);       /* CONTROL */
        frame.putShort(4, (short) 0); /* flags */
        frame.putShort(6, (short) 24); /* header_bytes */
        frame.putInt(8, 1);           /* frame_id */
        frame.putInt(12, 0);          /* base_frame_id */
        frame.putInt(16, 0);          /* payload_bytes */
        frame.putShort(20, (short) 0); /* record_count */
        frame.putShort(22, (short) 0); /* reserved */

        int status = bridge.feed(frame, 24, SystemClock.elapsedRealtime());
        int[] stats = bridge.stats();
        if (stats == null) {
            Log.i("ZG335BRIDGE", "status=" + status + " rx=? frames=? error=?");
        } else {
            Log.i("ZG335BRIDGE", "status=" + status + " rx=" + (stats[0] & 0xffffffffL)
                + " frames=" + (stats[1] & 0xffffffffL) + " error=" + stats[2]);
        }
    }

    @Override
    protected void onDestroy() {
        if (bridge != null) {
            bridge.close();
            bridge = null;
        }
        if (webView != null) {
            webView.destroy();
            webView = null;
        }
        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
        if (webView != null && webView.canGoBack()) {
            webView.goBack();
        } else {
            super.onBackPressed();
        }
    }

    public final class Bridge {
        @JavascriptInterface
        public String runProbe() {
            return MainActivity.this.probe();
        }
    }
}
