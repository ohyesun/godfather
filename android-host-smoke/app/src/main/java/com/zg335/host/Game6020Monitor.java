package com.zg335.host;

import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;

/** Passive DES first-frame observer for the 6020 game socket. */
final class Game6020Monitor {
    private static final String TAG = "ZG335GAME";
    private final byte[] key;
    private final ByteArrayOutputStream encrypted = new ByteArrayOutputStream();
    private boolean logged;
    private boolean sceneLogged;
    private boolean sceneMapLogged;
    private int pendingMapId;
    private String pendingMapName = "";
    private String selfName = "";
    private final Set<Integer> npcIds = new HashSet<>();
    private static volatile int sceneLines;
    private static volatile boolean sceneOk;

    static int sceneLines() {
        return sceneLines;
    }

    static boolean sceneOk() {
        return sceneOk;
    }

    Game6020Monitor(byte[] key) {
        this.key = key == null ? null : Arrays.copyOf(key, key.length);
    }

    synchronized void push(byte[] bytes) {
        if (key == null || key.length != 8 || bytes == null
            || bytes.length == 0) {
            return;
        }
        encrypted.write(bytes, 0, bytes.length);
        byte[] data = encrypted.toByteArray();
        int consumed = 0;
        int offset = 0;
        while (offset + 4 <= data.length) {
            int available = data.length - offset;
            if (available < 8) {
                break;
            }
            int blocks = available / 8;
            byte[] block = Arrays.copyOfRange(data, offset, offset + blocks * 8);
            byte[] plain = decrypt(block);
            if (plain == null || plain.length < 6) {
                offset += 8;
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
                offset += 8;
                continue;
            }
            int frameSize = 4 + length;
            if (offset + frameSize > data.length) {
                break;
            }
            int command = ((plain[4] & 0xff) << 8) | (plain[5] & 0xff);
            int bodyStart = 4;
            int bodyLength = length;
            if (bodyLength < 2 || bodyStart + bodyLength > plain.length) {
                offset += 8;
                continue;
            }
            if (command == 0x050c && bodyLength >= 55) {
                int mapId = getU16(plain, bodyStart + 12);
                String mapName = decodeGbk(plain, bodyStart + 22, 33);
                if (mapId > 0) {
                    pendingMapId = mapId;
                    pendingMapName = mapName;
                    if (!sceneMapLogged) {
                        sceneMapLogged = true;
                        Log.i(TAG, "SCENE-MAP map=" + mapId
                            + " filename=" + mapName);
                    }
                }
            } else if (command == 0x0501 && bodyLength >= 405) {
                String name = name(plain, bodyStart + 37);
                if (name.isEmpty()) name = name(plain, bodyStart + 37);
                if (!name.isEmpty()) selfName = name;
            } else if (command == 0x0507 && bodyLength >= 177) {
                npcIds.add(getU32(plain, bodyStart + 6));
            }
            if (!logged) {
                logged = true;
                Log.i(TAG, "GAME-OK 6020 cmd=" + String.format("%04x", command)
                    + " len=" + length + " encrypted=des");
            }
            consumed = offset + 4 + length;
            offset += 4 + length;
        }
        if (sceneMapLogged && !sceneLogged && validName(selfName)
            && !npcIds.isEmpty()) {
            sceneLogged = true;
            sceneOk = true;
            ++sceneLines;
            Log.i(TAG, "SCENE-OK map=" + pendingMapId + " self=" + selfName
                + " npc=" + npcIds.size());
        }
        if (consumed > 0) {
            encrypted.reset();
            if (consumed < data.length) {
                encrypted.write(data, consumed, data.length - consumed);
            }
        }
    }

    private static int getU16(byte[] bytes, int offset) {
        return (bytes[offset] & 0xff) | ((bytes[offset + 1] & 0xff) << 8);
    }

    private static int getU32(byte[] bytes, int offset) {
        return (bytes[offset] & 0xff)
            | ((bytes[offset + 1] & 0xff) << 8)
            | ((bytes[offset + 2] & 0xff) << 16)
            | ((bytes[offset + 3] & 0xff) << 24);
    }

    private static String name(byte[] bytes, int offset) {
        if (offset < 0 || offset + 32 > bytes.length) return "";
        return decodeGbk(bytes, offset, 32);
    }

    private static String decodeGbk(byte[] bytes, int offset, int length) {
        try {
            String value = new String(bytes, offset, length, "GBK");
            int zero = value.indexOf('\0');
            return (zero >= 0 ? value.substring(0, zero) : value).trim();
        } catch (Exception ignored) {
            return "";
        }
    }

    private static boolean validName(String value) {
        int han = 0;
        for (int i = 0; i < value.length(); ++i) {
            if (Character.UnicodeScript.of(value.charAt(i))
                == Character.UnicodeScript.HAN) {
                ++han;
            }
        }
        return han >= 2;
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
