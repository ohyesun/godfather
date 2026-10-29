package com.zg335.host;

import java.io.Closeable;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;

/** Minimal ZGMU/1 client: HELLO + GET + DATA/END. */
public final class Zgmu implements Closeable {
    public static final int T_HELLO = 0x01;
    public static final int T_HELLO_ACK = 0x81;
    public static final int T_GET = 0x10;
    public static final int T_DATA = 0x90;
    public static final int T_ERROR = 0x93;
    public static final int FLAG_END = 0x02;

    private static final int MAGIC = 0x475A;
    private static final int MAX_BODY = (1 << 20) + 64;

    public static final class ProtoError extends IOException {
        public final int code;
        ProtoError(int code, String message) {
            super(message);
            this.code = code;
        }
    }

    public interface Sink {
        void onData(long offset, byte[] buffer, int offsetInBuffer, int length)
            throws IOException;
    }

    private final Socket socket;
    private final InputStream input;
    private final OutputStream output;
    private final byte[] header = new byte[8];
    private byte[] body = new byte[8192];
    private int sequence;

    public Zgmu(String host, int port, String token, int connectTimeoutMs,
        int readTimeoutMs) throws IOException {
        Socket next = new Socket();
        try {
            next.connect(new InetSocketAddress(host, port), connectTimeoutMs);
            next.setTcpNoDelay(true);
            next.setSoTimeout(readTimeoutMs);
            socket = next;
            input = next.getInputStream();
            output = next.getOutputStream();
            hello(token);
        } catch (IOException error) {
            try {
                next.close();
            } catch (IOException ignored) {
                /* already closed */
            }
            throw error;
        }
    }

    private void hello(String token) throws IOException {
        byte[] tokenBytes = token == null ? new byte[0] : token.getBytes("UTF-8");
        byte[] payload = new byte[3 + tokenBytes.length + 8];
        putU16(payload, 0, 1);
        payload[2] = (byte)tokenBytes.length;
        System.arraycopy(tokenBytes, 0, payload, 3, tokenBytes.length);
        putU64(payload, 3 + tokenBytes.length, System.currentTimeMillis());
        send(T_HELLO, 0, payload, payload.length);
        int type = readHeader();
        if (type == T_ERROR) {
            throw parseError(readBody());
        }
        if (type != T_HELLO_ACK) {
            throw new IOException("unexpected first frame 0x" + Integer.toHexString(type));
        }
        int length = readBody();
        if (length < 55 || body[0] != 0) {
            throw new ProtoError(0x20, "HELLO_ACK rejected");
        }
        // docs: blockSize@1, maxInflight@5, serverTime@7, manifestSize@15.
        // Do not read manifestSize at 7; that is serverTime.
        getU64(body, 15);
    }

    public long get(String id, long offset, int length, Sink sink)
        throws IOException {
        byte[] idBytes = id.getBytes("UTF-8");
        if (idBytes.length > 255) {
            throw new IOException("id too long");
        }
        int requestId = ++sequence;
        byte[] payload = new byte[6 + idBytes.length + 12];
        putU32(payload, 0, requestId);
        putU16(payload, 4, idBytes.length);
        System.arraycopy(idBytes, 0, payload, 6, idBytes.length);
        putU64(payload, 6 + idBytes.length, offset);
        putU32(payload, 14 + idBytes.length, length & 0xFFFFFFFFL);
        send(T_GET, 0, payload, payload.length);
        long expectedOffset = offset;
        long received = 0;
        while (true) {
            int type = readHeader();
            int flags = lastFlags;
            int bodyLength = readBody();
            if (type == T_ERROR) {
                throw parseError(bodyLength);
            }
            if (type != T_DATA || bodyLength < 16) {
                throw new IOException("expected DATA, got 0x"
                    + Integer.toHexString(type));
            }
            int responseId = (int)getU32(body, 0);
            long chunkOffset = getU64(body, 4);
            int dataLength = (int)getU32(body, 12);
            if (responseId != requestId || chunkOffset != expectedOffset
                || dataLength < 0 || 16 + dataLength > bodyLength) {
                throw new IOException("DATA continuity/reqId invalid");
            }
            if (sink != null) {
                sink.onData(chunkOffset, body, 16, dataLength);
            }
            received += dataLength;
            expectedOffset += dataLength;
            if ((flags & FLAG_END) != 0) {
                return received;
            }
        }
    }

    public void close() {
        try {
            send(0x1A, 0, new byte[0], 0);
        } catch (IOException ignored) {
            /* best effort */
        }
        try {
            socket.close();
        } catch (IOException ignored) {
            /* already closed */
        }
    }

    private volatile int lastFlags;

    private void send(int type, int flags, byte[] payload, int length)
        throws IOException {
        putU16(header, 0, MAGIC);
        header[2] = (byte)type;
        header[3] = (byte)flags;
        putU32(header, 4, length);
        output.write(header, 0, 8);
        if (length > 0) {
            output.write(payload, 0, length);
        }
        output.flush();
    }

    private int readHeader() throws IOException {
        int count = input.read(header, 0, 8);
        if (count < 0) {
            throw new EOFException("server closed");
        }
        while (count < 8) {
            int next = input.read(header, count, 8 - count);
            if (next < 0) {
                throw new EOFException("header truncated");
            }
            count += next;
        }
        if (getU16(header, 0) != MAGIC) {
            throw new IOException("bad magic");
        }
        lastFlags = header[3] & 0xff;
        int length = (int)getU32(header, 4);
        if (length < 0 || length > MAX_BODY) {
            throw new IOException("body too large");
        }
        if (body.length < length) {
            body = new byte[length];
        }
        return header[2] & 0xff;
    }

    private int readBody() throws IOException {
        int want = (int)getU32(header, 4);
        int count = 0;
        while (count < want) {
            int next = input.read(body, count, want - count);
            if (next < 0) {
                throw new EOFException("body truncated");
            }
            count += next;
        }
        return count;
    }

    private ProtoError parseError(int length) {
        if (length < 7) {
            return new ProtoError(0x02, "short error");
        }
        int code = getU16(body, 4);
        int messageLength = Math.min(body[6] & 0xff, length - 7);
        String message = new String(body, 7, messageLength,
            java.nio.charset.StandardCharsets.UTF_8);
        return new ProtoError(code, message);
    }

    private static void putU16(byte[] bytes, int offset, int value) {
        bytes[offset] = (byte)value;
        bytes[offset + 1] = (byte)(value >>> 8);
    }

    private static void putU32(byte[] bytes, int offset, long value) {
        bytes[offset] = (byte)value;
        bytes[offset + 1] = (byte)(value >>> 8);
        bytes[offset + 2] = (byte)(value >>> 16);
        bytes[offset + 3] = (byte)(value >>> 24);
    }

    private static void putU64(byte[] bytes, int offset, long value) {
        for (int i = 0; i < 8; ++i) {
            bytes[offset + i] = (byte)(value >>> (8 * i));
        }
    }

    private static int getU16(byte[] bytes, int offset) {
        return (bytes[offset] & 0xff) | ((bytes[offset + 1] & 0xff) << 8);
    }

    private static long getU32(byte[] bytes, int offset) {
        return (bytes[offset] & 0xffL)
            | ((bytes[offset + 1] & 0xffL) << 8)
            | ((bytes[offset + 2] & 0xffL) << 16)
            | ((bytes[offset + 3] & 0xffL) << 24);
    }

    private static long getU64(byte[] bytes, int offset) {
        long value = 0;
        for (int i = 7; i >= 0; --i) {
            value = (value << 8) | (bytes[offset + i] & 0xffL);
        }
        return value;
    }
}
