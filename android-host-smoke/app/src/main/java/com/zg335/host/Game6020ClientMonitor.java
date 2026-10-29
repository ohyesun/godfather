package com.zg335.host;

import android.util.Log;

import java.util.Arrays;

import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;

/** Diagnostic-only observer for outgoing 6020 DES frames. */
final class Game6020ClientMonitor {
    private static final String TAG = "ZG335GAME";
    private final byte[] key;
    private int count;
    private final java.io.ByteArrayOutputStream raw = new java.io.ByteArrayOutputStream();
    private boolean rawLogged;
    private long lastTimeSync;
    private long tailTimestamp;
    private boolean tailTaken;

    synchronized long takeTailTimestamp() {
        if (tailTaken || tailTimestamp == 0) {
            return 0;
        }
        tailTaken = true;
        return tailTimestamp;
    }

    Game6020ClientMonitor(byte[] key) {
        this.key = key == null ? null : Arrays.copyOf(key, key.length);
    }

    synchronized void push(byte[] encrypted) {
        if (key == null || key.length != 8
            || encrypted == null || encrypted.length < 8) {
            return;
        }
        raw.write(encrypted, 0, encrypted.length);
        if (!rawLogged && raw.size() >= 384) {
            rawLogged = true;
            Log.i(TAG, "GAME-RAW 6020 tx=" + hex(raw.toByteArray(), 384));
        }
        if (count >= 20) {
            return;
        }
        byte[] plain;
        try {
            Cipher cipher = Cipher.getInstance("DES/ECB/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, new SecretKeySpec(key, "DES"));
            plain = cipher.doFinal(encrypted);
        } catch (Exception error) {
            return;
        }
        int offset = 0;
        while (offset + 4 <= plain.length && count < 20) {
            int lengthAndFlags = (plain[offset] & 0xff)
                | ((plain[offset + 1] & 0xff) << 8)
                | ((plain[offset + 2] & 0xff) << 16)
                | ((plain[offset + 3] & 0xff) << 24);
            int length = lengthAndFlags & 0xffffff;
            if (length <= 0 || offset + 4 + length > plain.length) {
                break;
            }
            int command = ((plain[offset + 4] & 0xff) << 8)
                | (plain[offset + 5] & 0xff);
            if (command == 0x0a03) {
                lastTimeSync = get32(plain, offset + 6);
            } else if (command == 0x6c0e) {
                tailTimestamp = lastTimeSync != 0
                    ? lastTimeSync : get32(plain, offset + 6);
            }
            if (count < 20) {
                Log.i(TAG, "GAME-CLIENT 6020 cmd="
                    + String.format("%04x", command) + " len=" + length);
                if (command == 0x0a03 || command == 0x1703
                    || command == 0x8822 || command == 0x6c0e) {
                    Log.i(TAG, "GAME-PLAIN 6020 cmd="
                        + String.format("%04x", command) + " body="
                        + hex(plain, offset + 4, length));
                }
                ++count;
            }
            offset += 4 + length;
        }
    }

    private static int get32(byte[] bytes, int offset) {
        return (bytes[offset] & 0xff)
            | ((bytes[offset + 1] & 0xff) << 8)
            | ((bytes[offset + 2] & 0xff) << 16)
            | ((bytes[offset + 3] & 0xff) << 24);
    }

    private static String hex(byte[] bytes, int limit) {
        StringBuilder out = new StringBuilder();
        int end = Math.min(bytes.length, limit);
        for (int i = 0; i < end; ++i) {
            out.append(String.format("%02x", bytes[i] & 0xff));
        }
        return out.toString();
    }

    private static String hex(byte[] bytes, int offset, int length) {
        StringBuilder out = new StringBuilder();
        int end = Math.min(bytes.length, offset + length);
        for (int i = offset; i < end; ++i) {
            out.append(String.format("%02x", bytes[i] & 0xff));
        }
        return out.toString();
    }
}
