package com.zg335.host;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.util.Log;
import android.webkit.WebResourceResponse;

import androidx.webkit.WebViewAssetLoader;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.zip.GZIPInputStream;

/**
 * Serves packaged assets with the legacy packed-asset fallback:
 * 1) AssetManager.open(path) first;
 * 2) then resources107-index.json + resources107.pak seek.
 *
 * The pak is never loaded wholly into memory. resources107.pak must be
 * uncompressed in the APK so AssetFileDescriptor can seek.
 */
final class PackedAssetsPathHandler implements WebViewAssetLoader.PathHandler {
    private static final String TAG = "ZG335ASSETS";
    private static final String INDEX = "resources107-index.json";
    private static final String PAK = "resources107.pak";

    private final AssetManager assets;
    private Map<String, Entry> index;

    PackedAssetsPathHandler(Context context) {
        assets = context.getAssets();
    }

    @Override
    public WebResourceResponse handle(String path) {
        String key = normalize(path);
        if (key == null) {
            Log.w(TAG, "reject path " + path);
            return null;
        }
        try {
            InputStream direct = assets.open(key);
            return response(mime(key), direct);
        } catch (FileNotFoundException ignored) {
            /* fall through to pak */
        } catch (IOException error) {
            Log.w(TAG, "asset open failed " + key, error);
        }

        Entry entry;
        try {
            entry = lookup(key);
        } catch (IOException error) {
            Log.e(TAG, "index load failed", error);
            return null;
        }
        if (entry == null) {
            Log.i(TAG, "pak miss " + key);
            return null;
        }
        if (entry.offset < 0 || entry.length < 0) {
            Log.i(TAG, "pak miss negative " + key + " offset=" + entry.offset);
            return null;
        }
        try {
            AssetFileDescriptor descriptor = assets.openFd(PAK);
            FileInputStream file = new FileInputStream(descriptor.getFileDescriptor());
            long start = descriptor.getStartOffset() + entry.offset;
            file.getChannel().position(start);
            InputStream bounded = new BoundedInputStream(
                new CloseAssetStream(file, descriptor), entry.length);
            InputStream body = entry.gzip == 1
                ? new GZIPInputStream(bounded) : bounded;
            Log.i(TAG, "pak hit " + key + " offset=" + entry.offset
                + " length=" + entry.length + " gzip=" + entry.gzip);
            return response(mime(key), body);
        } catch (FileNotFoundException error) {
            Log.e(TAG, "pak openFd failed; pak must be noCompress", error);
            return null;
        } catch (IOException error) {
            Log.e(TAG, "pak read failed " + key, error);
            return null;
        }
    }

    private static String normalize(String path) {
        if (path == null) {
            return null;
        }
        String key = path.startsWith("/") ? path.substring(1) : path;
        if (key.isEmpty() || key.startsWith("/") || key.contains("..")
            || key.indexOf('\\') >= 0) {
            return null;
        }
        return key;
    }

    private static WebResourceResponse response(String mime, InputStream body) {
        return new WebResourceResponse(mime, null, body);
    }

    private Entry lookup(String key) throws IOException {
        synchronized (this) {
            if (index == null) {
                index = loadIndex();
            }
        }
        return index.get(key);
    }

    private Map<String, Entry> loadIndex() throws IOException {
        InputStream input = assets.open(INDEX);
        try {
            String text = new String(readAll(input), "UTF-8");
            JSONObject root = new JSONObject(text);
            Map<String, Entry> result = new HashMap<>(root.length() * 2);
            Iterator<String> keys = root.keys();
            while (keys.hasNext()) {
                String key = keys.next();
                JSONArray value = root.getJSONArray(key);
                if (value.length() < 2) {
                    continue;
                }
                result.put(key, new Entry(value.getLong(0), value.getLong(1),
                    value.length() > 2 ? value.getInt(2) : 0));
            }
            Log.i(TAG, "index loaded entries=" + result.size());
            return result;
        } catch (JSONException error) {
            throw new IOException("invalid " + INDEX, error);
        } finally {
            input.close();
        }
    }

    private static byte[] readAll(InputStream input) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] buffer = new byte[8192];
        int count;
        while ((count = input.read(buffer)) >= 0) {
            output.write(buffer, 0, count);
        }
        return output.toByteArray();
    }

    private static String mime(String path) {
        String lower = path.toLowerCase();
        if (lower.endsWith(".js") || lower.endsWith(".mjs")) {
            return "application/javascript";
        }
        if (lower.endsWith(".json")) {
            return "application/json";
        }
        if (lower.endsWith(".wasm")) {
            return "application/wasm";
        }
        if (lower.endsWith(".html") || lower.endsWith(".htm")) {
            return "text/html";
        }
        if (lower.endsWith(".css")) {
            return "text/css";
        }
        if (lower.endsWith(".png")) {
            return "image/png";
        }
        if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) {
            return "image/jpeg";
        }
        if (lower.endsWith(".webp")) {
            return "image/webp";
        }
        if (lower.endsWith(".svg")) {
            return "image/svg+xml";
        }
        if (lower.endsWith(".glb")) {
            return "model/gltf-binary";
        }
        if (lower.endsWith(".gltf")) {
            return "model/gltf+json";
        }
        return "application/octet-stream";
    }

    private static final class Entry {
        final long offset;
        final long length;
        final int gzip;

        Entry(long offset, long length, int gzip) {
            this.offset = offset;
            this.length = length;
            this.gzip = gzip;
        }
    }

    private static final class CloseAssetStream extends FilterInputStream {
        private final AssetFileDescriptor descriptor;

        CloseAssetStream(FileInputStream input, AssetFileDescriptor descriptor) {
            super(input);
            this.descriptor = descriptor;
        }

        @Override
        public void close() throws IOException {
            try {
                super.close();
            } finally {
                try {
                    descriptor.close();
                } catch (IOException ignored) {
                    /* fd already closed */
                }
            }
        }
    }

    private static final class BoundedInputStream extends FilterInputStream {
        private long remaining;

        BoundedInputStream(InputStream input, long length) {
            super(input);
            remaining = length;
        }

        @Override
        public int read() throws IOException {
            if (remaining <= 0) {
                return -1;
            }
            int value = super.read();
            if (value >= 0) {
                --remaining;
            }
            return value;
        }

        @Override
        public int read(byte[] buffer, int offset, int length) throws IOException {
            if (remaining <= 0) {
                return -1;
            }
            int request = (int)Math.min((long)length, remaining);
            int count = super.read(buffer, offset, request);
            if (count > 0) {
                remaining -= count;
            }
            return count;
        }

        @Override
        public long skip(long length) throws IOException {
            long skipped = super.skip(Math.min(length, remaining));
            remaining -= skipped;
            return skipped;
        }

        @Override
        public int available() throws IOException {
            return (int)Math.min((long)super.available(), remaining);
        }
    }
}
