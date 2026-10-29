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
    private boolean selfProbeLogged;
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

    private int plainConsumed;

    synchronized void push(byte[] bytes) {
        if (key == null || key.length != 8 || bytes == null
            || bytes.length == 0) {
            return;
        }
        encrypted.write(bytes, 0, bytes.length);
        byte[] data = encrypted.toByteArray();
        int usable = data.length & ~7;
        if (usable < 8 || usable <= plainConsumed) {
            return;
        }
        byte[] plain = decrypt(Arrays.copyOf(data, usable));
        if (plain == null) {
            return;
        }
        int off = plainConsumed;
        while (off + 4 <= plain.length) {
            int flags = plain[off + 3] & 0xff;
            int length = (plain[off] & 0xff)
                | ((plain[off + 1] & 0xff) << 8)
                | ((plain[off + 2] & 0xff) << 16);
            if (length == 0 || length > 0xfffffb) {
                break;
            }
            int total = 4 + length;
            if (off + total > plain.length) {
                break;
            }
            byte[] payload = Arrays.copyOfRange(plain, off + 4, off + 4 + length);
            if ((flags & 0x40) != 0) {
                byte[] inflated = inflate(payload);
                if (inflated == null) {
                    off += total;
                    continue;
                }
                payload = inflated;
            }
            if (payload.length < 2) {
                off += total;
                continue;
            }
            int command = ((payload[0] & 0xff) << 8) | (payload[1] & 0xff);
            if (command == 0x050c && payload.length >= 55) {
                int mapId = getU16(payload, 12);
                String mapName = decodeGbk(payload, 22, 33);
                if (mapId > 0) {
                    pendingMapId = mapId;
                    pendingMapName = mapName;
                    if (!sceneMapLogged) {
                        sceneMapLogged = true;
                        Log.i(TAG, "SCENE-MAP map=" + mapId
                            + " filename=" + mapName);
                    }
                }
            } else if (command == 0x0501 && payload.length >= 405) {
                if (!selfProbeLogged) {
                    selfProbeLogged = true;
                    Log.i(TAG, "SELF-FRAME len=" + payload.length
                        + " offset=0 hex=" + hex(payload, 0, Math.min(80, payload.length)));
                }
                String name = name(payload, 38);
                if (!name.isEmpty()) selfName = name;
            } else if (command == 0x0507 && payload.length >= 177) {
                npcIds.add(getU32(payload, 6));
            }
            if (!logged) {
                logged = true;
                Log.i(TAG, "GAME-OK 6020 cmd=" + String.format("%04x", command)
                    + " len=" + length + " encrypted=des");
            }
            Log.i(TAG, "GAME-CMD 6020 cmd=" + String.format("%04x", command)
                + " len=" + length + " flags=" + flags);
            off += total;
        }
        plainConsumed = off;
        if (sceneMapLogged && !sceneLogged && validName(selfName)
            && !npcIds.isEmpty()) {
            sceneLogged = true;
            sceneOk = true;
            ++sceneLines;
            Log.i(TAG, "SCENE-OK map=" + pendingMapId + " self=" + selfName
                + " npc=" + npcIds.size());
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

    private static String hex(byte[] bytes, int offset, int length) {
        StringBuilder out = new StringBuilder(length * 2);
        for (int i = 0; i < length; ++i) {
            out.append(Character.forDigit((bytes[offset + i] >> 4) & 0xf, 16));
            out.append(Character.forDigit(bytes[offset + i] & 0xf, 16));
        }
        return out.toString();
    }

    private static byte[] inflate(byte[] data) {
        try (java.util.zip.InflaterInputStream in =
                 new java.util.zip.InflaterInputStream(
                     new java.io.ByteArrayInputStream(data))) {
            java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
            byte[] buf = new byte[4096];
            int n;
            while ((n = in.read(buf)) >= 0) out.write(buf, 0, n);
            return out.toByteArray();
        } catch (Exception ignored) {
            return null;
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
