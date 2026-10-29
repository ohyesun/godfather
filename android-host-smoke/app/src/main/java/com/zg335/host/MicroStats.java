package com.zg335.host;

import android.util.Log;

import java.util.HashSet;
import java.util.Set;
import java.util.Timer;
import java.util.TimerTask;

/** Process-level MICRO counters for one app run. */
final class MicroStats {
    private static final String TAG = "ZG335MICRO";
    private static final long START = System.currentTimeMillis();
    private static final Set<String> UNIQUE = new HashSet<>();
    private static final Set<String> FAIL_PATHS = new HashSet<>();
    private static int ok;
    private static long bytes;
    private static int fail;
    private static Timer timer;

    private MicroStats() {
    }

    static synchronized void recordOk(String path, long size) {
        ++ok;
        bytes += Math.max(0, size);
        UNIQUE.add(path);
        schedule();
    }

    static synchronized void recordFail(String path, int code) {
        ++fail;
        FAIL_PATHS.add(path);
        schedule();
    }

    private static void schedule() {
        if (timer != null) {
            timer.cancel();
        }
        timer = new Timer("micro-summary", true);
        timer.schedule(new TimerTask() {
            @Override
            public void run() {
                summary();
            }
        }, 5000L);
    }

    private static synchronized void summary() {
        Log.i(TAG, "MICRO-RUN ok=" + ok + " bytes=" + bytes
            + " uniquePaths=" + UNIQUE.size() + " fail=" + fail
            + " start=" + START + " end=" + System.currentTimeMillis());
        Log.i(TAG, "MICRO-FAIL events=" + fail + " uniquePaths="
            + FAIL_PATHS.size());
        if (!FAIL_PATHS.isEmpty()) {
            Log.i(TAG, "MICRO-FAIL-PATHS " + String.join(",", FAIL_PATHS));
        }
        Log.i(TAG, "SCENE-OK=" + (Game6020Monitor.sceneOk() ? 1 : 0)
            + " 05/0c lines=" + Game6020Monitor.sceneLines());
    }
}
