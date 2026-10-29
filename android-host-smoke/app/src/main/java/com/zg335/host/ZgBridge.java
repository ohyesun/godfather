package com.zg335.host;

import java.nio.ByteBuffer;

public final class ZgBridge implements AutoCloseable {
    static {
        System.loadLibrary("zg335host");
    }

    private long handle;

    public boolean create(int maxFrame, int maxPacket, int maxTx, int flags) {
        if (handle != 0L) {
            return true;
        }
        handle = nativeCreate(1, maxFrame, maxPacket, maxTx, flags);
        return handle != 0L;
    }

    public int feed(ByteBuffer source, int length, long nowMs) {
        if (handle == 0L || source == null || !source.isDirect()) {
            return 4; /* ZG_BRIDGE_PROTOCOL_ERROR */
        }
        return nativeFeed(handle, source, length, nowMs);
    }

    /**
     * Returns {rx_bytes_low32, frames_low32, last_error}, or null if unavailable.
     */
    public int[] stats() {
        if (handle == 0L) {
            return null;
        }
        int[] out = new int[3];
        return nativeStats(handle, out, out.length) == 3 ? out : null;
    }

    public int requestFull() {
        return handle == 0L ? 4 : nativeRequestFull(handle);
    }

    public int pollRender(ByteBuffer destination) {
        if (handle == 0L || destination == null || !destination.isDirect()) {
            return 4; /* ZG_BRIDGE_PROTOCOL_ERROR */
        }
        return nativePollRender(handle, destination, destination.capacity());
    }

    public void destroy() {
        if (handle != 0L) {
            nativeDestroy(handle);
            handle = 0L;
        }
    }

    @Override
    public void close() {
        destroy();
    }

    private static native long nativeCreate(int abi, int maxFrame, int maxPacket,
        int maxTx, int flags);

    private static native int nativeFeed(long handle, ByteBuffer source, int length,
        long nowMs);

    private static native int nativeStats(long handle, int[] out, int capacity);

    private static native int nativeRequestFull(long handle);

    private static native int nativePollRender(long handle, ByteBuffer destination,
        int capacity);

    private static native void nativeDestroy(long handle);
}
