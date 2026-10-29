package com.zg335.host;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;

/** First-milestone probe: fetch one negative-offset path through ZGMU/18008. */
final class MicroProbe {
    private static final String TAG = "ZG335MICRO";
    private static final String PATH = "native107/textures-report.json";
    private static final String CONFIG = "micro-client.json";
    private static final String INDEX = "resources107-index.json";
    private static final String SHA = "pakfetch-sha256.json";

    private MicroProbe() {
    }

    static void probe(Context context) {
        Zgmu client = null;
        try {
            AssetManager assets = context.getAssets();
            JSONObject config = new JSONObject(readText(assets, CONFIG));
            JSONArray entry = new JSONObject(readText(assets, INDEX))
                .getJSONArray(PATH);
            long offset = entry.getLong(0);
            long storedLength = entry.getLong(1);
            int flag = entry.length() > 2 ? entry.getInt(2) : 0;
            if (offset != -1 || storedLength < 0 || storedLength > Integer.MAX_VALUE) {
                throw new IOException("path is not a remote negative-offset entry");
            }
            String expectedSha = new JSONObject(readText(assets, SHA))
                .optString(PATH, "");
            final ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            client = new Zgmu(config.getString("host"), config.getInt("port"),
                config.optString("token", ""), config.optInt("connectTimeoutMs", 6000),
                config.optInt("readTimeoutMs", 25000));
            long got = client.get("path:" + PATH, 0, (int)storedLength,
                new Zgmu.Sink() {
                    @Override
                    public void onData(long offset, byte[] buffer, int from, int length)
                        throws IOException {
                        bytes.write(buffer, from, length);
                    }
                });
            byte[] raw = bytes.toByteArray();
            if (got != storedLength || raw.length != storedLength) {
                throw new IOException("length mismatch " + raw.length + " != " + storedLength);
            }
            if (flag == 1) {
                throw new IOException("probe expects flag=0; gzip path not used");
            }
            if (expectedSha.length() == 64) {
                String actual = sha256(raw);
                if (!expectedSha.equalsIgnoreCase(actual)) {
                    throw new IOException("sha256 mismatch");
                }
            }
            Log.i(TAG, "MICRO-OK path=" + PATH + " bytes=" + raw.length);
        } catch (Zgmu.ProtoError error) {
            Log.e(TAG, "MICRO-FAIL code=" + error.code);
        } catch (Throwable error) {
            Log.e(TAG, "MICRO-FAIL code=io");
        } finally {
            if (client != null) {
                client.close();
            }
        }
    }

    private static String readText(AssetManager assets, String name) throws Exception {
        InputStream input = assets.open(name);
        try {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
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

    private static String sha256(byte[] bytes) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        byte[] hash = digest.digest(bytes);
        StringBuilder out = new StringBuilder(64);
        for (byte value : hash) {
            out.append(Character.forDigit((value >> 4) & 0xf, 16));
            out.append(Character.forDigit(value & 0xf, 16));
        }
        return out.toString();
    }
}
