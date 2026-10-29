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
        scanMetadata(data);
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
            int frameSize = ((4 + length + 7) / 8) * 8;
            if (offset + frameSize > data.length) {
                break;
            }
            int command = ((plain[4] & 0xff) << 8) | (plain[5] & 0xff);
            if (!logged) {
                logged = true;
                Log.i(TAG, "GAME-OK 6020 cmd=" + String.format("%04x", command)
                    + " len=" + length + " encrypted=des");
            }
            if (command == 0x050c) {
                ++sceneLines;
                sceneOk = true;
                Log.i(TAG, "05/0c lines=" + sceneLines + " len=" + length);
            }
            consumed = offset + frameSize;
            offset += frameSize;
        }
        if (consumed > 0) {
            encrypted.reset();
            if (consumed < data.length) {
                encrypted.write(data, consumed, data.length - consumed);
            }
        }
    }

    private void scanMetadata(byte[] data) {
        int usable = data.length & ~7;
        if (usable < 8) {
            return;
        }
        byte[] plain = decrypt(Arrays.copyOf(data, usable));
        if (plain == null) {
            return;
        }
        boolean pendingScene = false;
        int sceneOffset = -1;
        for (int i = 0; i + 1 < plain.length; ++i) {
            if ((plain[i] & 0xff) == 0x05 && (plain[i + 1] & 0xff) == 0x0c) {
                if (sceneOffset < 0) {
                    sceneOffset = i;
                    Log.i(TAG, "SCENE-FRAME offset=" + i + " hex="
                        + hex(plain, i, Math.min(55, plain.length - i)));
                }
                if (i + 55 <= plain.length) {
                    int mapId = getU16(plain, i + 12);
                    String mapName = decodeGbk(plain, i + 22, 33);
                    if (mapId > 0) {
                        pendingScene = true;
                        pendingMapId = mapId;
                        pendingMapName = mapName;
                        if (!sceneMapLogged) {
                            sceneMapLogged = true;
                            Log.i(TAG, "SCENE-MAP map=" + mapId
                                + " filename=" + mapName);
                        }
                    }
                }
            }
            if ((plain[i] & 0xff) == 0x0a && (plain[i + 1] & 0xff) == 0x01
                && i + 42 <= plain.length) {
                String name = name(plain, i + 10);
                if (name.isEmpty()) name = name(plain, i + 6);
                if (!name.isEmpty()) selfName = name;
            }
            if ((plain[i] & 0xff) == 0x05 && (plain[i + 1] & 0xff) == 0x01
                && i + 405 <= plain.length) {
                String name = name(plain, i + 38);
                if (name.isEmpty()) name = name(plain, i + 34);
                if (!name.isEmpty()) selfName = name;
            }
            if ((plain[i] & 0xff) == 0x05 && (plain[i + 1] & 0xff) == 0x07
                && i + 177 <= plain.length) {
                npcIds.add(getU32(plain, i + 6));
            }
        }
        if (sceneMapLogged && !sceneLogged && validName(selfName)
            && !npcIds.isEmpty()) {
            sceneLogged = true;
            sceneOk = true;
            ++sceneLines;
            Log.i(TAG, "SCENE-OK map=" + pendingMapId + " self=" + selfName
                + " npc=" + npcIds.size());
        } else if (pendingScene && !sceneLogged) {
            Log.i(TAG, "SCENE-PENDING map=" + pendingMapId
                + " filename=" + pendingMapName + " self="
                + (validName(selfName) ? selfName : "unknown") + " npc="
                + npcIds.size());
        } else if (!pendingScene && sceneOffset < 0) {
            Log.i(TAG, "SCENE-NO 05c plain=" + plain.length);
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
        String value = decodeGbk(bytes, offset, 32);
        return value.isEmpty() ? "" : value;
    }

    private static String hex(byte[] bytes, int offset, int length) {
        StringBuilder out = new StringBuilder(length * 2);
        for (int i = 0; i < length; ++i) {
            out.append(Character.forDigit((bytes[offset + i] >> 4) & 0xf, 16));
            out.append(Character.forDigit(bytes[offset + i] & 0xf, 16));
        }
        return out.toString();
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
