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
        + "else if(type===4)out[id]=u32(v,p)|0;"
        + "else if(type===3)out[id]=u32(v,p);"
        + "else if(type===2)out[id]=u16(v,p);"
        + "else if(type===1)out[id]=v[p];"
        + "o=p+len;}return out;}"
        + "function eventType(t){var n={1:'impact',2:'cast-stop',3:'cast-aim',4:'damage',5:'cast',6:'dodge',7:'jump',8:'motion-result113'};return n[t]||('event'+t);}"
        + "var busy=false,shadow=null,sceneApplied=false,sceneTries=0;"
        + "function applyShadow(){if(!window.sceneQA||!shadow||sceneApplied)return;"
        + "var q=window.sceneQA;sceneApplied=true;"
        + "var self=shadow.self,p={uid:shadow.selfEntity,name:self[0x0002],x:self[0x0010],y:self[0x0011]==null?0:self[0x0011],z:self[0x0012],level:self[0x0007],dead:!!self[0x0022],alive:!self[0x0022],hpKnown:false};"
        + "if(self[0x0020]!=null&&self[0x0021]!=null){p.hp=self[0x0020];p.maxHp=self[0x0021];p.hpKnown=true;}"
        + "if(self[0x0005]!=null)p.profession=self[0x0005];if(self[0x0006]!=null)p.sex=self[0x0006];if(self[0x0013]!=null)p.dir=self[0x0013];if(self[0x0014]!=null)p.moveSpeed=self[0x0014];"
        + "var mobs=[],players=[],npcs=[],drops=[];shadow.actors.forEach(function(a){if(a.kind===2)mobs.push(a);else if(a.kind===1)players.push(a);else if(a.kind===3)npcs.push(a);else if(a.kind===4)drops.push(a);});"
        + "var s={generation:1,map:{id:shadow.map},time:performance.now(),rtt109:0,player:p,players:players,mobs:mobs,npcs:npcs,drops:drops,events:shadow.events,pet:null,mount:null,meta:{},hud:shadow.hud};"
        + "try{q.update(s);}catch(e){console.log('ZG335JS sceneQA-error '+((e&&e.message)||e));return;}"
        + "var firstMob=mobs.length?mobs[0].id:-1;"
        + "setTimeout(function(){console.log('ZG335JS real map='+shadow.map+' self='+p.name+' actors='+shadow.actors.size+' objects='+q.objects.size);var keys=Array.from(q.objects.keys());console.log('ZG335JS real-keys='+keys.length+' no-extra='+(keys.length===shadow.actors.size));},1000);}"
        + "function poll(){if(busy)return;busy=true;"
        + "fetch('/zg/render',{cache:'no-store'}).then(function(response){"
        + "if(response.status!==200)return null;return response.arrayBuffer();"
        + "}).then(function(buffer){if(!buffer)return;var v=new Uint8Array(buffer);"
        + "if(v.length!==829)return;var count=u16(v,20),off=24,map=0,self={},selfEntity=0;"
        + "var actors=new Map(),events=[],hud=null;"
        + "for(var i=0;i<count;i++){var type=u16(v,off),entity=u32(v,off+4),payload=u32(v,off+8),end=off+12+payload;"
        + "if(type===0x0101){var m=fields(v,off+12,end);map=m[0x0100];}"
        + "else if(type===0x0102){self=fields(v,off+12,end);selfEntity=entity;}"
        + "else if(type===0x0103){var a=fields(v,off+12,end);var actor={id:entity,kind:a[0x0001],name:a[0x0002],x:a[0x0010],z:a[0x0012]};if(a[0x0011]!=null)actor.y=a[0x0011];if(a[0x0020]!=null)actor.hp=a[0x0020];if(a[0x0021]!=null)actor.maxHp=a[0x0021];if(a[0x0022]!=null)actor.dead=!!a[0x0022];if(a[0x0023]!=null)actor.hpKnown=!!a[0x0023];if(a[0x0024]!=null)actor.alive=!!a[0x0024];actors.set(entity,actor);}"
        + "else if(type===0x0104)actors.delete(entity);"
        + "else if(type===0x0105){var e=fields(v,off+12,end);events.push({id:e[0x0050],type:eventType(e[0x0051])});}"
        + "else if(type===0x0106){var h=fields(v,off+12,end);hud={gold:h[0x0060],vipLevel:h[0x0062],level:h[0x0007]};}"
        + "off=end;}"
        + "shadow={map:map,self:self,selfEntity:selfEntity,actors:actors,events:events,hud:hud};"
        + "console.log('ZG335JS full='+v.length+' records='+count+' actors='+actors.size+' events='+events.length+' hud='+(hud?1:0)+' map='+map+' self='+self[0x0002]);"
        + "applyShadow();"
        + "}).catch(function(error){console.log('ZG335JS fetch-error '+error);})"
        + ".then(function(){busy=false;});}"
        + "poll();setInterval(poll,100);"
        + "var sceneWait=setInterval(function(){if(window.sceneQA){clearInterval(sceneWait);applyShadow();}else if(++sceneTries>80){clearInterval(sceneWait);console.log('ZG335JS sceneQA=missing');}},100);"
        + "['vendor/build/three.module.js','vendor/examples/jsm/loaders/GLTFLoader.js']"
        + ".forEach(function(path){fetch('/assets/'+path,{cache:'no-store'})"
        + ".then(function(r){console.log('ZG335JS asset '+path+'='+r.status);})"
        + ".catch(function(e){console.log('ZG335JS asset-error '+path+' '+e);});});"
        + "window.addEventListener('error',function(e){console.log('ZG335JS page-error '+((e&&e.message)||e));});"
        + "})();</script>";

    static {
        System.loadLibrary("zg335host");
    }

    private native String runProbe();
    private WebView webView;
    private ZgBridge bridge;
    private WebViewAssetLoader assetLoader;
    private DirectTcpBridge directTcp;

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
            .addPathHandler("/assets/", new PackedAssetsPathHandler(this))
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
        directTcp = new DirectTcpBridge(web);
        web.addJavascriptInterface(directTcp, "Lab");

        setContentView(web, new ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT));

        probe();
        runBridgeByteBuffer();
        runDebugFrameFeed();
        MicroScope.attach(getAssets());
        MicroStats.startTimeout();
        if ((getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0) {
            new Thread(() -> MicroProbe.preflightRequiredTexture(this),
                "micro-preflight").start();
        }
        web.loadUrl(ASSET_ORIGIN + ASSET_INDEX + "?sceneQA=1");
        installSceneReadyProbe(web);
    }

    private void installSceneReadyProbe(final WebView web) {
        final Runnable[] probe = new Runnable[1];
        probe[0] = () -> {
            if (webView != web) {
                return;
            }
            web.evaluateJavascript(
                "(function(){var h=document.getElementById('hud');"
                    + "var l=document.querySelector('.bridgeLoading114');"
                    + "return !!(h&&!h.classList.contains('hidden')&&!l);})()",
                value -> {
                    if ("true".equals(value)) {
                        web.evaluateJavascript(
                            "window.Lab&&window.Lab.sceneReady&&window.Lab.sceneReady()",
                            null);
                    } else {
                        web.postDelayed(probe[0], 1000L);
                    }
                });
        };
        web.postDelayed(probe[0], 1000L);
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
        ByteBuffer packet = ByteBuffer.allocateDirect(1024)
            .order(ByteOrder.LITTLE_ENDIAN);
        int length = bridge.pollRender(packet);
        if (length != 829) {
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
        if (directTcp != null) {
            directTcp.close();
            directTcp = null;
        }
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
