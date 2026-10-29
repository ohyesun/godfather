package com.zg335.host;

import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.zip.DataFormatException;
import java.util.zip.Inflater;

/** Passive 7000-frame observer; only logs LOGIN-OK metadata. */
final class Login7000Monitor {
    private static final String TAG = "ZG335LOGIN";
    private static final byte[] KEY = hex(
        "3f79d5e24a8cb6c1af315ec7eb9d6ecb");
    private static final int ROUNDS = 12;
    private static final int P = 0xb7e15163;
    private static final int Q = 0x9e3779b9;

    private final int[] subkeys;
    private final ByteArrayOutputStream encrypted = new ByteArrayOutputStream();
    private final ByteArrayOutputStream plain = new ByteArrayOutputStream();
    private boolean logged;

    Login7000Monitor() {
        subkeys = new int[2 * (ROUNDS + 1)];
        for (int i = 0; i < subkeys.length; ++i) {
            subkeys[i] = P + i * Q;
        }
        int[] words = new int[4];
        for (int i = 0; i < 4; ++i) {
            words[i] = (KEY[i * 4] & 0xff)
                | ((KEY[i * 4 + 1] & 0xff) << 8)
                | ((KEY[i * 4 + 2] & 0xff) << 16)
                | ((KEY[i * 4 + 3] & 0xff) << 24);
        }
        int a = 0;
        int b = 0;
        int i = 0;
        int j = 0;
        int iterations = 3 * Math.max(subkeys.length, words.length);
        for (int k = 0; k < iterations; ++k) {
            subkeys[i] = Integer.rotateLeft(subkeys[i] + a + b, 3);
            a = subkeys[i];
            words[j] = Integer.rotateLeft(words[j] + a + b, a + b);
            b = words[j];
            i = (i + 1) % subkeys.length;
            j = (j + 1) % words.length;
        }
    }

    synchronized void push(byte[] bytes) {
        if (logged || bytes == null || bytes.length == 0) {
            return;
        }
        encrypted.write(bytes, 0, bytes.length);
        byte[] all = encrypted.toByteArray();
        int blocks = all.length / 8 * 8;
        if (blocks == 0) {
            return;
        }
        byte[] decrypted = new byte[blocks];
        for (int offset = 0; offset < blocks; offset += 8) {
            decryptBlock(all, offset, decrypted, offset);
        }
        plain.write(decrypted, 0, decrypted.length);
        byte[] remaining = new byte[all.length - blocks];
        System.arraycopy(all, blocks, remaining, 0, remaining.length);
        encrypted.reset();
        encrypted.write(remaining, 0, remaining.length);
        parse();
    }

    private void parse() {
        byte[] data = plain.toByteArray();
        int consumed = 0;
        while (data.length - consumed >= 4) {
            int lengthAndFlags = get32(data, consumed);
            int bodyLength = lengthAndFlags & 0xffffff;
            int flags = (lengthAndFlags >>> 24) & 0xff;
            if (bodyLength == 0 || bodyLength > 1024 * 1024) {
                break;
            }
            if (data.length - consumed < 4 + bodyLength) {
                break;
            }
            byte[] body = new byte[bodyLength];
            System.arraycopy(data, consumed + 4, body, 0, bodyLength);
            consumed += 4 + bodyLength;
            if ((flags & 0x40) != 0) {
                body = inflate(body);
                if (body == null) {
                    continue;
                }
            }
            inspect(body);
        }
        if (consumed > 0) {
            plain.reset();
            plain.write(data, consumed, data.length - consumed);
        }
    }

    private void inspect(byte[] body) {
        if (logged || body.length < 0x20 || body[0] != 0x68 || body[1] != 0x04) {
            return;
        }
        int accid = get32(body, 6);
        int end = 14;
        while (end < 30 && body[end] != 0) {
            ++end;
        }
        String host = new String(body, 14, end - 14, StandardCharsets.US_ASCII);
        int port = get16(body, 0x1e);
        int ticketLength = body.length - 0x20;
        logged = true;
        Log.i(TAG, "LOGIN-OK accid=" + accid + " game=" + host + ":"
            + port + " ticket=" + ticketLength + "B");
    }

    private static byte[] inflate(byte[] body) {
        Inflater inflater = new Inflater();
        inflater.setInput(body);
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] buffer = new byte[4096];
        try {
            while (!inflater.finished()) {
                int count = inflater.inflate(buffer);
                if (count == 0) {
                    if (inflater.needsInput() || inflater.needsDictionary()) {
                        return null;
                    }
                } else {
                    out.write(buffer, 0, count);
                }
            }
            return out.toByteArray();
        } catch (DataFormatException error) {
            return null;
        } finally {
            inflater.end();
        }
    }

    private void decryptBlock(byte[] in, int inOffset, byte[] out, int outOffset) {
        int a = get32(in, inOffset);
        int b = get32(in, inOffset + 4);
        for (int k = ROUNDS; k >= 1; --k) {
            b = Integer.rotateRight(b - subkeys[2 * k + 1], a) ^ a;
            a = Integer.rotateRight(a - subkeys[2 * k], b) ^ b;
        }
        b -= subkeys[1];
        a -= subkeys[0];
        put32(out, outOffset, a);
        put32(out, outOffset + 4, b);
    }

    private static int get16(byte[] bytes, int offset) {
        return (bytes[offset] & 0xff) | ((bytes[offset + 1] & 0xff) << 8);
    }

    private static int get32(byte[] bytes, int offset) {
        return (bytes[offset] & 0xff)
            | ((bytes[offset + 1] & 0xff) << 8)
            | ((bytes[offset + 2] & 0xff) << 16)
            | ((bytes[offset + 3] & 0xff) << 24);
    }

    private static void put32(byte[] bytes, int offset, int value) {
        bytes[offset] = (byte)value;
        bytes[offset + 1] = (byte)(value >>> 8);
        bytes[offset + 2] = (byte)(value >>> 16);
        bytes[offset + 3] = (byte)(value >>> 24);
    }

    private static byte[] hex(String value) {
        byte[] out = new byte[value.length() / 2];
        for (int i = 0; i < out.length; ++i) {
            out[i] = (byte)Integer.parseInt(value.substring(i * 2, i * 2 + 2), 16);
        }
        return out;
    }
}
