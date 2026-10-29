package com.zg335.host;

import android.util.Log;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
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
    private static boolean finalized;

    private MicroStats() {
    }

    static synchronized void finalizeRun() {
        summary();
    }

    static synchronized void recordOk(String path, long size) {
        if (finalized) return;
        ++ok;
        bytes += Math.max(0, size);
        UNIQUE.add(path);
        schedule();
    }

    static synchronized void recordFail(String path, int code) {
        if (finalized) return;
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
        }, 20000L);
    }

    private static synchronized void summary() {
        if (finalized) return;
        finalized = true;
        Log.i(TAG, "MICRO-FAIL events=" + fail + " uniquePaths="
            + FAIL_PATHS.size());
        List<String> required = new ArrayList<>();
        List<String> optional = new ArrayList<>();
        for (String path : FAIL_PATHS) {
            (PakRemote.isRequired(path) ? required : optional).add(path);
        }
        Log.i(TAG, "MICRO-RUN-FINAL ok=" + ok + " bytes=" + bytes
            + " uniquePaths=" + UNIQUE.size() + " fail=" + fail
            + " requiredFail=" + required.size()
            + " optionalFail=" + optional.size()
            + " start=" + START + " end=" + System.currentTimeMillis());
        if (!required.isEmpty()) {
            Log.i(TAG, "MICRO-FAIL-REQUIRED " + String.join(",", required));
        }
        if (!optional.isEmpty()) {
            Log.i(TAG, "MICRO-FAIL-OPTIONAL " + String.join(",", optional));
        }
        Log.i(TAG, "SCENE-OK=" + (Game6020Monitor.sceneOk() ? 1 : 0)
            + " 05/0c lines=" + Game6020Monitor.sceneLines());
    }
}
