package com.zg335.host;

import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.util.Arrays;

import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;

/** Passive DES first-frame observer for the 6020 game socket. */
final class Game6020Monitor {
    private static final String TAG = "ZG335GAME";
    private final byte[] key;
    private final ByteArrayOutputStream encrypted = new ByteArrayOutputStream();
    private boolean logged;

    Game6020Monitor(byte[] key) {
        this.key = key == null ? null : Arrays.copyOf(key, key.length);
    }

    synchronized void push(byte[] bytes) {
        if (logged || key == null || key.length != 8 || bytes == null
            || bytes.length == 0) {
            return;
        }
        encrypted.write(bytes, 0, bytes.length);
        byte[] data = encrypted.toByteArray();
        for (int offset = 0; offset + 4 <= data.length; offset += 8) {
            int available = data.length - offset;
            if (available < 8) {
                break;
            }
            int blocks = available / 8;
            byte[] block = Arrays.copyOfRange(data, offset, offset + blocks * 8);
            byte[] plain = decrypt(block);
            if (plain == null || plain.length < 6) {
                continue;
            }
            int lengthAndFlags = (plain[0] & 0xff)
                | ((plain[1] & 0xff) << 8)
                | ((plain[2] & 0xff) << 16)
                | ((plain[3] & 0xff) << 24);
            int length = lengthAndFlags & 0xffffff;
            int flags = (lengthAndFlags >>> 24) & 0xff;
            if (length <= 0 || length > 1024 * 1024
                || plain.length < 4 + length) {
                continue;
            }
            int command = ((plain[4] & 0xff) << 8) | (plain[5] & 0xff);
            logged = true;
            Log.i(TAG, "GAME-OK 6020 cmd=" + String.format("%04x", command)
                + " len=" + length + " encrypted=des");
            return;
        }
    }

    private byte[] decrypt(byte[] blocks) {
        try {
            Cipher cipher = Cipher.getInstance("DES/ECB/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, new SecretKeySpec(key, "DES"));
            return cipher.doFinal(blocks);
        } catch (Exception error) {
            return null;
        }
    }
}
