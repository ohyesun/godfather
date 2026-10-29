package com.zg335.host;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Log;
import android.view.ViewGroup;
import android.webkit.ConsoleMessage;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.webkit.WebViewAssetLoader;
import androidx.webkit.WebViewCompat;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.Collections;

public final class MainActivity extends Activity {
    private static final String TAG_BRIDGE = "ZG335BRIDGE";
    private static final String TAG_JS = "ZG335JS";
    private static final String ASSET_ORIGIN = "https://appassets.androidplatform.net";
    private static final String ASSET_INDEX = "/assets/index.html";
    private static final String BRIDGE_ADAPTER =
        "<script>(function(){"
        + "function u16(v,o){return v[o]|(v[o+1]<<8);}"
        + "function u32(v,o){return (v[o]|(v[o+1]<<8)|(v[o+2]<<16)|(v[o+3]<<24))>>>0;}"
        + "function fields(v,start,end){var out={},o=start;"
        + "while(o+6<=end){var id=u16(v,o),type=v[o+2],len=u16(v,o+4),p=o+6;"
        + "if(p+len>end)break;"
        + "if(type===6)out[id]=new TextDecoder().decode(v.subarray(p,p+len));"
        + "else if(type===5)out[id]=new DataView(v.buffer,v.byteOffset+p,4).getFloat32(0,true);"
        + "else if(type===3)out[id]=u32(v,p);"
        + "else if(type===2)out[id]=u16(v,p);"
        + "else if(type===1)out[id]=v[p];"
        + "o=p+len;}return out;}"
        + "var busy=false;"
        + "function poll(){if(busy)return;busy=true;"
        + "fetch('/zg/render',{cache:'no-store'}).then(function(response){"
        + "if(response.status!==200)return null;return response.arrayBuffer();"
        + "}).then(function(buffer){if(!buffer)return;var v=new Uint8Array(buffer);"
        + "if(v.length!==117)return;var count=u16(v,20),off=24,map=0,self={};"
        + "for(var i=0;i<count;i++){var type=u16(v,off),payload=u32(v,off+8),end=off+12+payload;"
        + "if(type===0x0101){var m=fields(v,off+12,end);map=m[0x0100];}"
        + "if(type===0x0102)self=fields(v,off+12,end);off=end;}"
        + "console.log('ZG335JS full='+v.length+' map='+map+' self='+self[0x0002]"
        + "+' x='+self[0x0010]+' z='+self[0x0012]+' dead='+self[0x0022]);"
        + "}).catch(function(error){console.log('ZG335JS fetch-error '+error);})"
        + ".then(function(){busy=false;});}"
        + "poll();setInterval(poll,100);"
        + "})();</script>";

    static {
        System.loadLibrary("zg335host");
    }

    private native String runProbe();
    private WebView webView;
    private ZgBridge bridge;
    private WebViewAssetLoader assetLoader;

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
        logWebViewVersion();

        assetLoader = new WebViewAssetLoader.Builder()
            .addPathHandler("/assets/", new WebViewAssetLoader.AssetsPathHandler(this))
            .addPathHandler("/zg/", new WebViewAssetLoader.PathHandler() {
                @Override
                public WebResourceResponse handle(String path) {
                    if (!"render".equals(path) && !"/render".equals(path)
                        && !"/zg/render".equals(path)) {
                        return noContent();
                    }
                    byte[] packet = pollRenderPacket();
                    if (packet == null) {
                        return noContent();
                    }
                    return new WebResourceResponse("application/octet-stream",
                        "UTF-8", new ByteArrayInputStream(packet));
                }
            })
            .build();

        webView = new WebView(this);
        WebView web = webView;
        WebSettings settings = web.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setDatabaseEnabled(true);
        settings.setAllowFileAccess(false);
        settings.setAllowContentAccess(false);
        settings.setMediaPlaybackRequiresUserGesture(false);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
        settings.setCacheMode(WebSettings.LOAD_DEFAULT);

        web.setWebViewClient(new WebViewClient() {
            @Override
            public WebResourceResponse shouldInterceptRequest(WebView view,
                WebResourceRequest request) {
                Uri uri = request.getUrl();
                if (ASSET_ORIGIN.equals(uri.getScheme() + "://" + uri.getAuthority())
                    && ASSET_INDEX.equals(uri.getPath())) {
                    return assetWithBridgeAdapter(uri);
                }
                return assetLoader.shouldInterceptRequest(uri);
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                Log.i(TAG_BRIDGE, "ui loaded " + url);
            }
        });
        web.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onConsoleMessage(ConsoleMessage message) {
                String text = message == null ? null : message.message();
                if (text != null && text.startsWith(TAG_JS)) {
                    Log.i(TAG_JS, text);
                }
                return true;
            }
        });
        web.addJavascriptInterface(new Bridge(), "ZG335Host");

        setContentView(web, new ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT));

        probe();
        runBridgeByteBuffer();
        runDebugFrameFeed();
        web.loadUrl(ASSET_ORIGIN + ASSET_INDEX);
    }

    private void logWebViewVersion() {
        try {
            PackageInfo info = WebViewCompat.getCurrentWebViewPackage(this);
            String version = info == null ? "unknown"
                : (info.versionName == null ? "unknown" : info.versionName);
            Log.i(TAG_BRIDGE, "webview=" + version);
        } catch (Throwable error) {
            Log.e(TAG_BRIDGE, "webview version unavailable", error);
        }
    }

    private WebResourceResponse assetWithBridgeAdapter(Uri uri) {
        try (InputStream input = getAssets().open("index.html")) {
            byte[] original = readAll(input);
            String html = new String(original, StandardCharsets.UTF_8);
            int body = html.lastIndexOf("</body>");
            String injected = body >= 0
                ? html.substring(0, body) + BRIDGE_ADAPTER + html.substring(body)
                : html + BRIDGE_ADAPTER;
            return new WebResourceResponse("text/html", "UTF-8",
                new ByteArrayInputStream(injected.getBytes(StandardCharsets.UTF_8)));
        } catch (IOException error) {
            Log.e(TAG_BRIDGE, "asset index adapter injection failed", error);
            return assetLoader.shouldInterceptRequest(uri);
        }
    }

    private static byte[] readAll(InputStream input) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] buffer = new byte[8192];
        int count;
        while ((count = input.read(buffer)) >= 0) {
            output.write(buffer, 0, count);
        }
        return output.toByteArray();
    }

    private static WebResourceResponse noContent() {
        return new WebResourceResponse("application/octet-stream", "UTF-8",
            204, "No Content", Collections.emptyMap(),
            new ByteArrayInputStream(new byte[0]));
    }

    private synchronized byte[] pollRenderPacket() {
        if (bridge == null || bridge.requestFull() != 0) {
            return null;
        }
        ByteBuffer packet = ByteBuffer.allocateDirect(128)
            .order(ByteOrder.LITTLE_ENDIAN);
        int length = bridge.pollRender(packet);
        if (length != 117) {
            return null;
        }
        byte[] bytes = new byte[length];
        packet.position(0);
        packet.get(bytes);
        return bytes;
    }

    private void runDebugFrameFeed() {
        boolean debuggable = (getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
        if (!debuggable || bridge == null) {
            return;
        }
        try {
            Class.forName("com.zg335.host.FrameFeedTest")
                .getMethod("run", ZgBridge.class)
                .invoke(null, bridge);
        } catch (Throwable error) {
            Log.e(TAG_BRIDGE, "debug frame hook unavailable", error);
        }
    }

    private void runBridgeByteBuffer() {
        bridge = new ZgBridge();
        if (!bridge.create(4096, 65536, 4096, 0)) {
            Log.e(TAG_BRIDGE, "create failed");
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
            Log.i(TAG_BRIDGE, "status=" + status + " rx=? frames=? error=?");
        } else {
            Log.i(TAG_BRIDGE, "status=" + status + " rx=" + (stats[0] & 0xffffffffL)
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
