package com.zg335.host;

import android.util.Log;
import android.webkit.JavascriptInterface;
import android.webkit.WebView;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Minimal window.Lab request95 bridge for direct raw TCP. */
public final class DirectTcpBridge {
    private static final String TAG = "ZG335DIRECT";
    private static final String ZONES_CONFIG =
        "[Server]\n"
        + "1=PC|新开四区|新开四区|45.124.78.8|7000|4|0|6020\n"
        + "2=PC|复古三区|复古三区|45.124.76.59|7000|3|0|6020\n"
        + "3=PC|复古二区|复古二区|45.124.76.73|7000|2|0|6020\n"
        + "4=PC|复古一区|复古一区|45.124.78.3|7000|1|0|6020\n";

    private final WebView webView;
    private final ExecutorService executor = Executors.newFixedThreadPool(4);
    private final Object socketLock = new Object();
    private final Object readLock = new Object();
    private final Object writeLock = new Object();
    private Socket socket;
    private InputStream input;
    private OutputStream output;
    private Login7000Monitor loginMonitor;
    private Game6020Monitor gameMonitor;
    private byte[] desKey;
    private volatile boolean closed = true;

    public DirectTcpBridge(WebView webView) {
        this.webView = webView;
    }

    @JavascriptInterface
    public void request95(final int id, final String endpoint, final String route,
        final String credential, final String account, final String bodyJson) {
        executor.execute(() -> {
            try {
                JSONObject body = bodyJson == null || bodyJson.isEmpty()
                    ? new JSONObject() : new JSONObject(bodyJson);
                JSONObject result = dispatch(route, body);
                if (!"/direct/send".equals(route) && !"/direct/receive".equals(route)) {
                    Log.i(TAG, "route=" + route + " status=200");
                }
                reply(id, 200, result);
            } catch (Throwable error) {
                Log.e(TAG, "direct route failed " + route, error);
                JSONObject failure = new JSONObject();
                try {
                    failure.put("error", error.getMessage() == null
                        ? "直连请求失败" : error.getMessage());
                } catch (Exception ignored) {
                    /* JSONObject cannot fail for this string */
                }
                reply(id, 500, failure);
            }
        });
    }

    @JavascriptInterface
    public void request(final int id, final String endpoint, final String route,
        final String credential, final String bodyJson) {
        request95(id, endpoint, route, credential, "", bodyJson);
    }

    @JavascriptInterface
    public void report(String ignored) {
        /* flow diagnostics are not part of the direct login bridge */
    }

    private JSONObject dispatch(String route, JSONObject body) throws Exception {
        if ("/direct/zones".equals(route)) {
            JSONObject result = new JSONObject();
            result.put("configText", ZONES_CONFIG);
            return result;
        }
        if ("/direct/open".equals(route)) {
            String host = body.optString("host", "");
            int port = body.optInt("port", 0);
            open(host, port);
            JSONObject result = new JSONObject();
            result.put("connected", true);
            result.put("host", host);
            result.put("port", port);
            return result;
        }
        if ("/direct/send".equals(route)) {
            byte[] bytes = decodeHex(body.optString("packetHex", ""));
            synchronized (writeLock) {
                if (socket == null || output == null) {
                    throw new IllegalStateException("直连连接未打开");
                }
                output.write(bytes);
                output.flush();
            }
            JSONObject result = new JSONObject();
            result.put("dataHex", "");
            return result;
        }
        if ("/direct/receive".equals(route)) {
            byte[] bytes = receive();
            JSONObject result = new JSONObject();
            result.put("dataHex", encodeHex(bytes));
            return result;
        }
        if ("/direct/close".equals(route)) {
            close();
            JSONObject result = new JSONObject();
            result.put("closed", true);
            return result;
        }
        throw new IllegalArgumentException("未支持的直连路由: " + route);
    }

    private void open(String host, int port) throws Exception {
        if (host == null || !host.matches("\\d{1,3}(?:\\.\\d{1,3}){3}")
            || port < 1 || port > 65535) {
            throw new IllegalArgumentException("直连区服地址无效");
        }
        close();
        Socket next = connect(host, port);
        next.setSoTimeout(25);
        synchronized (socketLock) {
            socket = next;
            input = next.getInputStream();
            output = next.getOutputStream();
            closed = false;
            loginMonitor = port == 7000 ? new Login7000Monitor() : null;
            gameMonitor = port == 6020 ? new Game6020Monitor(desKey) : null;
            if (port == 6020 && desKey == null) {
                Log.e("ZG335GAME", "GAME-FAIL 6020 reason=des-key-missing");
            }
        }
        Log.i(TAG, "open " + host + ":" + port);
    }

    private Socket connect(String host, int port) throws Exception {
        try {
            return connectOnce(host, port);
        } catch (SocketTimeoutException first) {
            if (port != 6020) {
                throw first;
            }
            Log.i("ZG335GAME", "GAME-RETRY 6020 attempt=2 reason=timeout");
            try {
                Thread.sleep(1000L);
            } catch (InterruptedException interrupted) {
                Thread.currentThread().interrupt();
                throw first;
            }
            try {
                return connectOnce(host, port);
            } catch (SocketTimeoutException second) {
                Log.e("ZG335GAME",
                    "GAME-FAIL 6020 attempt=2 reason=timeout possible-old-client-session");
                throw second;
            }
        }
    }

    private Socket connectOnce(String host, int port) throws Exception {
        Socket socket = new Socket();
        try {
            socket.setTcpNoDelay(true);
            socket.connect(new InetSocketAddress(host, port), 6000);
            return socket;
        } catch (Exception error) {
            try {
                socket.close();
            } catch (Exception ignored) {
                /* connection never opened */
            }
            throw error;
        }
    }

    private byte[] receive() throws Exception {
        synchronized (readLock) {
            if (socket == null || input == null) {
                throw new IllegalStateException("直连连接未打开");
            }
            byte[] buffer = new byte[8192];
            try {
                int count = input.read(buffer);
                if (count < 0) {
                    close();
                    return new byte[0];
                }
                byte[] bytes = new byte[count];
                System.arraycopy(buffer, 0, bytes, 0, count);
                Login7000Monitor monitor = loginMonitor;
                if (monitor != null) {
                    monitor.push(bytes);
                    if (monitor.hasDesKey()) {
                        desKey = monitor.desKey();
                    }
                }
                Game6020Monitor game = gameMonitor;
                if (game != null) {
                    game.push(bytes);
                }
                return bytes;
            } catch (SocketTimeoutException timeout) {
                return new byte[0];
            }
        }
    }

    void close() {
        synchronized (socketLock) {
            closed = true;
            try {
                if (socket != null) {
                    socket.close();
                }
            } catch (Exception ignored) {
                /* already closed */
            }
            socket = null;
            input = null;
            output = null;
            loginMonitor = null;
            gameMonitor = null;
        }
    }

    private void reply(int id, int status, JSONObject body) {
        try {
            JSONObject envelope = new JSONObject();
            envelope.put("status", status);
            envelope.put("body", body.toString());
            envelope.put("encoding", "identity");
            String script = "window.nativeReply(" + id + "," + envelope + ")";
            webView.post(() -> webView.evaluateJavascript(script, null));
        } catch (Exception error) {
            Log.e(TAG, "reply failed", error);
        }
    }

    private static byte[] decodeHex(String value) {
        if (value == null || (value.length() & 1) != 0
            || !value.matches("[0-9a-fA-F]*")) {
            throw new IllegalArgumentException("packetHex 无效");
        }
        byte[] out = new byte[value.length() / 2];
        for (int i = 0; i < out.length; ++i) {
            out[i] = (byte)Integer.parseInt(value.substring(i * 2, i * 2 + 2), 16);
        }
        return out;
    }

    private static String encodeHex(byte[] bytes) {
        StringBuilder out = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            out.append(Character.forDigit((value >> 4) & 0xf, 16));
            out.append(Character.forDigit(value & 0xf, 16));
        }
        return out.toString();
    }
}
