package com.zg335.host;

import android.app.Activity;
import android.util.Log;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.security.KeyStore;
import java.security.SecureRandom;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLServerSocket;
import javax.net.ssl.SSLSocket;

/**
 * Debug-only, self-contained TLS loopback. It sends one 24-byte control frame
 * through a local self-signed TLS server, feeds the received plaintext bytes to
 * the same ZgBridge, and logs the accumulated stats. It never parses frames or
 * creates JSON.
 */
public final class TlsLoopbackTest {
    private static final String TAG = "ZG335BRIDGE";
    private static final char[] STORE_PASS = "changeit".toCharArray();

    private TlsLoopbackTest() {
    }

    public static void run(Activity activity, ZgBridge bridge) {
        Thread thread = new Thread(() -> loopback(activity, bridge), "ZG-TLS-loopback");
        thread.start();
    }

    private static void loopback(Activity activity, ZgBridge bridge) {
        LoopbackServer server = null;
        TlsTransport client = null;
        try {
            server = new LoopbackServer(activity);
            server.start();

            final CountDownLatch fed = new CountDownLatch(1);
            client = new TlsTransport(bridge, new TlsTransport.Listener() {
                @Override
                public void onBytesFed(int length, int status) {
                    int[] stats = bridge.stats();
                    if (stats == null) {
                        Log.i(TAG, "tls status=" + status + " rx=? frames=?");
                    } else {
                        Log.i(TAG, "tls status=" + status
                            + " rx=" + (stats[0] & 0xffffffffL)
                            + " frames=" + (stats[1] & 0xffffffffL));
                    }
                    fed.countDown();
                }

                @Override
                public void onTransportError(String message) {
                    Log.e(TAG, "tls transport error: " + message);
                }
            }, true);
            client.open("127.0.0.1", server.port());

            if (!fed.await(5, TimeUnit.SECONDS)) {
                Log.e(TAG, "tls loopback timeout waiting for feed");
            }
        } catch (Throwable error) {
            Log.e(TAG, "tls loopback failed", error);
        } finally {
            if (client != null) {
                client.close();
            }
            if (server != null) {
                server.close();
            }
        }
    }

    private static byte[] controlFrame() {
        ByteBuffer frame = ByteBuffer.allocateDirect(24).order(ByteOrder.LITTLE_ENDIAN);
        frame.putShort(0, (short) 0x5A47);
        frame.put(2, (byte) 1);
        frame.put(3, (byte) 3);
        frame.putShort(4, (short) 0);
        frame.putShort(6, (short) 24);
        frame.putInt(8, 2);
        frame.putInt(12, 0);
        frame.putInt(16, 0);
        frame.putShort(20, (short) 0);
        frame.putShort(22, (short) 0);
        byte[] out = new byte[24];
        frame.get(out);
        return out;
    }

    private static final class LoopbackServer {
        private final SSLServerSocket server;
        private final Thread thread;

        LoopbackServer(Activity activity) throws Exception {
            KeyStore keyStore = KeyStore.getInstance("PKCS12");
            try (InputStream input = activity.getResources()
                    .openRawResource(R.raw.zg_test_keystore)) {
                keyStore.load(input, STORE_PASS);
            }
            KeyManagerFactory keyManagers = KeyManagerFactory.getInstance(
                KeyManagerFactory.getDefaultAlgorithm());
            keyManagers.init(keyStore, STORE_PASS);

            SSLContext context = SSLContext.getInstance("TLS");
            context.init(keyManagers.getKeyManagers(), null, new SecureRandom());
            server = (SSLServerSocket) context.getServerSocketFactory()
                .createServerSocket(0, 1, InetAddress.getByName("127.0.0.1"));
            server.setSoTimeout(5000);
            thread = new Thread(this::serve, "ZG-TLS-server");
        }

        int port() {
            return server.getLocalPort();
        }

        void start() {
            thread.start();
        }

        private void serve() {
            try (SSLSocket socket = (SSLSocket) server.accept()) {
                socket.setTcpNoDelay(true);
                socket.startHandshake();
                OutputStream output = socket.getOutputStream();
                output.write(controlFrame());
                output.flush();
                Thread.sleep(200);
            } catch (Throwable error) {
                Log.e(TAG, "tls server failed", error);
            }
        }

        void close() {
            try {
                server.close();
            } catch (Exception ignored) {
            }
        }
    }
}
