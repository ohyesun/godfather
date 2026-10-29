package com.zg335.host;

import android.util.Log;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
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
    private static Timer timeoutTimer;
    private static boolean finalized;
    private static boolean scenePending;
    private static final Map<String, Req> REQUESTS = new HashMap<>();

    static final class Req {
        String status = "requested";
        int attempts;
    }
    private static final String SCOPE =
        "MICRO-SCOPE map=xianyangcheng spawn=138,184 duration=30s "
            + "paths=87 sha256=4a3a5015";

    private MicroStats() {
    }

    static synchronized void startTimeout() {
        if (finalized) return;
        timeoutTimer = new Timer("micro-timeout", true);
        timeoutTimer.schedule(new TimerTask() {
            @Override
            public void run() {
                summary();
            }
        }, 180000L);
    }

    static synchronized void sceneReady() {
        if (finalized || scenePending) return;
        scenePending = true;
        if (timeoutTimer != null) {
            timeoutTimer.cancel();
            timeoutTimer = null;
        }
        if (timer != null) {
            timer.cancel();
        }
        Log.i(TAG, SCOPE);
        timer = new Timer("micro-final", true);
        timer.schedule(new TimerTask() {
            @Override
            public void run() {
                summary();
            }
        }, 30000L);
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

    static synchronized void requested(String path) {
        Req req = REQUESTS.get(path);
        if (req == null) {
            req = new Req();
            REQUESTS.put(path, req);
        }
        if (req.attempts == 0) req.attempts = 1;
    }

    static synchronized void retrying(String path, int attempt) {
        requested(path);
        REQUESTS.get(path).status = "retry";
        REQUESTS.get(path).attempts = attempt;
    }

    static synchronized void ok(String path, int attempt) {
        Req req = REQUESTS.get(path);
        if (req == null) {
            req = new Req();
            REQUESTS.put(path, req);
        }
        req.status = "ok";
        req.attempts = Math.max(1, attempt);
    }

    static synchronized void failed(String path, int attempt) {
        Req req = REQUESTS.get(path);
        if (req == null) {
            req = new Req();
            REQUESTS.put(path, req);
        }
        req.status = "fail";
        req.attempts = Math.max(1, attempt);
    }

    private static void schedule() {
        if (scenePending) return;
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
        if (timeoutTimer != null) {
            timeoutTimer.cancel();
            timeoutTimer = null;
        }
        if (timer != null) {
            timer.cancel();
            timer = null;
        }
        Log.i(TAG, "MICRO-FAIL events=" + fail + " uniquePaths="
            + FAIL_PATHS.size());
        List<String> required = new ArrayList<>();
        List<String> optional = new ArrayList<>();
        for (String path : FAIL_PATHS) {
            (PakRemote.isRequired(path) ? required : optional).add(path);
        }
        Log.i(TAG, SCOPE);
        Log.i(TAG, "MICRO-RUN-FINAL ok=" + ok + " bytes=" + bytes
            + " uniquePaths=" + UNIQUE.size() + " fail=" + fail
            + " requiredFail=" + required.size()
            + " optionalFail=" + optional.size()
            + " start=" + START + " end=" + System.currentTimeMillis());
        for (String path : MicroScope.paths()) {
            Req req = REQUESTS.get(path);
            if (req == null) {
                Log.i(TAG, "MICRO-REQ path=" + path
                    + " required=1 status=absent referenced=0");
            } else {
                Log.i(TAG, "MICRO-REQ path=" + path + " required=1 status="
                    + req.status + " attempts=" + req.attempts);
            }
        }
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
