package com.zg335.host;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;

/** Negative-offset pak fallback backed by the ZGMU/18008 client. */
final class PakRemote {
    private static final String TAG = "ZG335MICRO";
    private static final String CONFIG = "micro-client.json";
    private static final String[] CODE_EXT = {
        ".js", ".mjs", ".html", ".htm", ".css", ".svg"
    };

    private static volatile File cacheDir;
    private static volatile Config config;

    private PakRemote() {
    }

    static void attach(Context context) {
        if (cacheDir == null && context != null) {
            cacheDir = new File(context.getApplicationContext().getFilesDir(),
                "pakfetch");
        }
    }

    static InputStream openRemote(AssetManager assets, String name, long storedLen,
        int flag) throws IOException {
        refuseCode(name);
        File dir = requireCacheDir();
        Config cfg = requireConfig(assets);
        String id = "path:" + name;
        File cache = new File(dir, sha256(id.getBytes(StandardCharsets.UTF_8))
            + ".bin");
        if (cache.isFile() && (storedLen < 0 || cache.length() == storedLen)) {
            Log.i(TAG, "MICRO-OK path=" + name + " bytes=" + cache.length());
            return wrap(cache, flag);
        }
        if (cache.exists() && !cache.delete()) {
            throw new IOException("cannot remove stale cache");
        }
        File part = new File(cache.getPath() + ".part");
        if (part.exists() && !part.delete()) {
            throw new IOException("cannot remove stale part");
        }
        Zgmu client = null;
        try {
            client = new Zgmu(cfg.host, cfg.port, cfg.token,
                cfg.connectTimeoutMs, cfg.readTimeoutMs);
            final FileOutputStream out = new FileOutputStream(part);
            try {
                final long[] received = {0};
                client.get(id, 0, storedLen >= 0
                    ? (int)Math.min(Integer.MAX_VALUE - 16, storedLen)
                    : 0x7fffff00, new Zgmu.Sink() {
                    @Override
                    public void onData(long offset, byte[] buffer, int from,
                        int length) throws IOException {
                        out.write(buffer, from, length);
                        received[0] += length;
                    }
                });
                out.flush();
                out.getFD().sync();
                if (storedLen >= 0 && received[0] != storedLen) {
                    throw new IOException("length mismatch");
                }
            } finally {
                out.close();
            }
            if (!part.renameTo(cache)) {
                if (!cache.delete() || !part.renameTo(cache)) {
                    throw new IOException("cannot commit cache");
                }
            }
            Log.i(TAG, "MICRO-OK path=" + name + " bytes=" + cache.length());
            return wrap(cache, flag);
        } catch (Zgmu.ProtoError error) {
            Log.e(TAG, "MICRO-FAIL code=" + error.code + " path=" + name);
            throw error;
        } catch (Throwable error) {
            Log.e(TAG, "MICRO-FAIL code=io path=" + name);
            throw new IOException("micro fetch failed", error);
        } finally {
            if (client != null) {
                client.close();
            }
        }
    }

    private static InputStream wrap(File file, int flag) throws IOException {
        InputStream input = new java.io.FileInputStream(file);
        return flag == 1 ? new java.util.zip.GZIPInputStream(input, 0x8000)
            : input;
    }

    private static void refuseCode(String name) throws IOException {
        String lower = name.toLowerCase();
        for (String suffix : CODE_EXT) {
            if (lower.endsWith(suffix)) {
                throw new IOException("remote code is not allowed");
            }
        }
    }

    private static File requireCacheDir() throws IOException {
        File dir = cacheDir;
        if (dir == null) {
            String tmp = System.getProperty("java.io.tmpdir");
            if (tmp == null) {
                throw new IOException("no cache directory");
            }
            dir = new File(tmp, "pakfetch");
            cacheDir = dir;
        }
        if (!dir.isDirectory() && !dir.mkdirs()) {
            throw new IOException("cannot create cache directory");
        }
        return dir;
    }

    private static Config requireConfig(AssetManager assets) throws IOException {
        Config current = config;
        if (current != null) {
            return current;
        }
        synchronized (PakRemote.class) {
            if (config != null) {
                return config;
            }
            try {
                String text = readText(assets, CONFIG);
                JSONObject json = new JSONObject(text);
                Config next = new Config();
                next.host = json.getString("host");
                next.port = json.getInt("port");
                next.token = json.optString("token", "");
                next.connectTimeoutMs = json.optInt("connectTimeoutMs", 6000);
                next.readTimeoutMs = json.optInt("readTimeoutMs", 25000);
                config = next;
                return next;
            } catch (Exception error) {
                throw new IOException("invalid micro config", error);
            }
        }
    }

    private static String readText(AssetManager assets, String name)
        throws IOException {
        InputStream input = assets.open(name);
        try {
            java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
            byte[] buffer = new byte[8192];
            int count;
            while ((count = input.read(buffer)) >= 0) {
                out.write(buffer, 0, count);
            }
            return new String(out.toByteArray(), StandardCharsets.UTF_8);
        } finally {
            input.close();
        }
    }

    private static String sha256(byte[] bytes) {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            byte[] hash = digest.digest(bytes);
            StringBuilder out = new StringBuilder(64);
            for (byte value : hash) {
                out.append(Character.forDigit((value >> 4) & 0xf, 16));
                out.append(Character.forDigit(value & 0xf, 16));
            }
            return out.toString();
        } catch (Exception error) {
            throw new RuntimeException(error);
        }
    }

    private static final class Config {
        String host;
        int port;
        String token;
        int connectTimeoutMs;
        int readTimeoutMs;
    }
}
