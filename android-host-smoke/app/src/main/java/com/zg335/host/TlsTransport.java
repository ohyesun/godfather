package com.zg335.host;

import android.os.SystemClock;

import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.security.SecureRandom;
import java.security.cert.X509Certificate;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeUnit;

import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

/**
 * TLS plaintext byte transport. It does not parse game frames and never creates
 * JSON. The reader feeds raw bytes to ZgBridge; the writer writes byte[] only.
 */
public final class TlsTransport implements Closeable {
    public interface Listener {
        void onBytesFed(int length, int status);
        void onTransportError(String message);
    }

    private static final TrustManager[] TEST_TRUST_ALL = new TrustManager[] {
        new X509TrustManager() {
            @Override
            public void checkClientTrusted(X509Certificate[] chain, String authType) {
            }

            @Override
            public void checkServerTrusted(X509Certificate[] chain, String authType) {
            }

            @Override
            public X509Certificate[] getAcceptedIssuers() {
                return new X509Certificate[0];
            }
        }
    };

    private final ZgBridge bridge;
    private final Listener listener;
    private final boolean allowTestCertificate;
    private final BlockingQueue<byte[]> outgoing = new ArrayBlockingQueue<>(16);
    private volatile SSLSocket socket;
    private volatile Thread reader;
    private volatile Thread writer;
    private volatile boolean closed;

    public TlsTransport(ZgBridge bridge, Listener listener,
        boolean allowTestCertificate) {
        this.bridge = bridge;
        this.listener = listener;
        this.allowTestCertificate = allowTestCertificate;
    }

    public synchronized void open(String host, int port) throws Exception {
        if (socket != null || closed) {
            throw new IllegalStateException("TLS transport already opened or closed");
        }

        SSLContext context = SSLContext.getInstance("TLS");
        context.init(null, allowTestCertificate ? TEST_TRUST_ALL : null,
            new SecureRandom());

        Socket raw = new Socket();
        raw.setTcpNoDelay(true);
        raw.connect(new InetSocketAddress(host, port), 5000);
        SSLSocket tls = (SSLSocket) context.getSocketFactory()
            .createSocket(raw, host, port, true);
        tls.setTcpNoDelay(true);
        tls.startHandshake();
        socket = tls;

        reader = new Thread(this::readLoop, "ZG-TLS-reader");
        writer = new Thread(this::writeLoop, "ZG-TLS-writer");
        reader.start();
        writer.start();
    }

    public boolean send(byte[] bytes) {
        if (closed || bytes == null || bytes.length == 0) {
            return false;
        }
        return outgoing.offer(bytes);
    }

    private void readLoop() {
        final ByteBuffer direct = ByteBuffer.allocateDirect(4096);
        final byte[] scratch = new byte[4096];
        try {
            InputStream input = socket.getInputStream();
            while (!closed) {
                int length = input.read(scratch);
                if (length < 0) {
                    break;
                }
                direct.clear();
                direct.put(scratch, 0, length);
                direct.flip();
                int status = bridge.feed(direct, length, SystemClock.elapsedRealtime());
                listener.onBytesFed(length, status);
            }
        } catch (Exception e) {
            if (!closed) {
                listener.onTransportError(String.valueOf(e.getMessage()));
            }
        }
    }

    private void writeLoop() {
        try {
            OutputStream output = socket.getOutputStream();
            while (!closed) {
                byte[] bytes = outgoing.poll(1, TimeUnit.SECONDS);
                if (bytes == null) {
                    continue;
                }
                output.write(bytes);
                output.flush();
            }
        } catch (Exception e) {
            if (!closed) {
                listener.onTransportError(String.valueOf(e.getMessage()));
            }
        }
    }

    @Override
    public synchronized void close() {
        closed = true;
        outgoing.clear();
        SSLSocket current = socket;
        socket = null;
        if (current != null) {
            try {
                current.close();
            } catch (IOException ignored) {
            }
        }
        if (reader != null) {
            reader.interrupt();
            reader = null;
        }
        if (writer != null) {
            writer.interrupt();
            writer = null;
        }
    }
}
