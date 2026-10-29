package com.zg335.host;

import android.os.SystemClock;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.zip.GZIPOutputStream;

/**
 * Debug-only test input for Phase 2B. Java creates one gzip JSON frame with a
 * 4-byte big-endian length prefix and feeds it as a direct ByteBuffer. The
 * frame content is never parsed into Java objects and is never sent to JS.
 */
public final class FrameFeedTest {
    private static final String TAG = "ZG335BRIDGE";

    private FrameFeedTest() {
    }

    public static void run(ZgBridge bridge) {
        Thread thread = new Thread(() -> feed(bridge), "ZG-frame-feed");
        thread.start();
    }

    private static void feed(ZgBridge bridge) {
        try {
            byte[] json = "{\"a\":1,\"b\":\"x\"}".getBytes(StandardCharsets.UTF_8);
            ByteArrayOutputStream compressed = new ByteArrayOutputStream();
            try (GZIPOutputStream gzip = new GZIPOutputStream(compressed)) {
                gzip.write(json);
            }
            byte[] gzipBytes = compressed.toByteArray();

            ByteBuffer frame = ByteBuffer.allocate(4 + gzipBytes.length)
                .order(ByteOrder.BIG_ENDIAN);
            frame.putInt(gzipBytes.length);
            frame.put(gzipBytes);
            byte[] framed = frame.array();

            ByteBuffer direct = ByteBuffer.allocateDirect(framed.length);
            direct.put(framed);
            direct.flip();

            int[] before = bridge.stats();
            int status = bridge.feed(direct, framed.length,
                SystemClock.elapsedRealtime());
            int[] stats = bridge.stats();
            long rxBefore = before == null ? -1 : (before[0] & 0xffffffffL);
            long rxAfter = stats == null ? -1 : (stats[0] & 0xffffffffL);
            long frames = stats == null ? -1 : (stats[1] & 0xffffffffL);
            long error = stats == null ? -1 : stats[2];
            Log.i(TAG, "frame status=" + status + " rx=" + rxAfter
                + " frames=" + frames + " error=" + error
                + " before=" + rxBefore);
        } catch (Throwable error) {
            Log.e(TAG, "frame feed failed", error);
        }
    }
}
