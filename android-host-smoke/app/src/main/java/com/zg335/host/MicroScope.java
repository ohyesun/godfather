package com.zg335.host;

import android.content.res.AssetManager;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.Set;

/** Frozen T85 required path scope (87 paths from PHASE6-T85-SCOPE.md). */
final class MicroScope {
    private static final String ASSET = "micro-scope-t85.txt";
    private static volatile Set<String> required = Collections.emptySet();

    private MicroScope() {
    }

    static synchronized void attach(AssetManager assets) {
        if (!required.isEmpty() || assets == null) {
            return;
        }
        Set<String> next = new LinkedHashSet<>();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(
            assets.open(ASSET), "UTF-8"))) {
            String line;
            while ((line = reader.readLine()) != null) {
                line = line.trim();
                if (!line.isEmpty() && !line.startsWith("#")) {
                    next.add(line);
                }
            }
        } catch (Exception ignored) {
            /* fall back to legacy rules */
            return;
        }
        required = Collections.unmodifiableSet(next);
    }

    static boolean isRequired(String path) {
        Set<String> current = required;
        if (!current.isEmpty()) {
            return current.contains(path);
        }
        return path.startsWith("native107/textures/")
            || (path.startsWith("native107/") && path.endsWith(".glb"))
            || (path.startsWith("scenes/")
                && (path.endsWith(".bin") || path.endsWith("/map.json")));
    }

    static Set<String> paths() {
        return required;
    }
}
