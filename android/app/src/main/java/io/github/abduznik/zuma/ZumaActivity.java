/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 *
 * PvZ-Portable is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PvZ-Portable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PvZ-Portable. If not, see <https://www.gnu.org/licenses/>.
 */

package io.github.abduznik.zuma;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.RelativeLayout;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;

import org.libsdl.app.SDLActivity;

import android.content.res.AssetManager;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.util.ArrayList;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;

public class ZumaActivity extends SDLActivity {
    private static final String TAG = "ZumaPortable";
    private OnBackInvokedCallback mBackCallback;
    private Thread mLogMirrorThread;
    private volatile boolean mLogMirrorStop;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        File extDir = getExternalFilesDir(null);
        if (extDir != null && !extDir.exists()) extDir.mkdirs();

        // Diagnostics: truncate the log and write a Java-side header BEFORE the
        // native layer starts (native appends to this same file from SDL_main),
        // so the log has content from the very first moment of the process.
        truncateDiag();
        appendDiag("=== Zuma Android diag (Java) start ===");
        appendDiag("SDK=" + Build.VERSION.SDK_INT + " release=" + Build.VERSION.RELEASE
                + " model=" + Build.MODEL + " device=" + Build.DEVICE);
        appendDiag("resource dir=" + (extDir != null ? extDir.getAbsolutePath() : "null"));

        extractBundledAssetsIfNeeded(extDir);
        appendDiag("assets extracted; hasGameResources=" + hasGameResources(extDir));
        Log.i(TAG, "Resource dir: " + (extDir != null ? extDir.getAbsolutePath() : "null"));
        ensureLegacyStoragePermission();
        startLogMirror(extDir);

        if (!hasGameResources(extDir)) {
            super.onCreate(savedInstanceState);
            startActivity(new Intent(this, ResourceImportActivity.class));
            finish();
            return;
        }

        super.onCreate(savedInstanceState);
        hideSystemUI();
        setupImeLayoutAdjustment();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            mBackCallback = this::dispatchBackToSdl;
            getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                OnBackInvokedDispatcher.PRIORITY_DEFAULT,
                mBackCallback
            );
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemUI();
    }

    @Override
    public void onBackPressed() {
        dispatchBackToSdl();
    }

    @Override
    protected void onDestroy() {
        mLogMirrorStop = true;
        if (mBackCallback != null) {
            getOnBackInvokedDispatcher().unregisterOnBackInvokedCallback(mBackCallback);
            mBackCallback = null;
        }
        super.onDestroy();
    }

    private void dispatchBackToSdl() {
        SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_BACK);
        SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_BACK);
    }

    private void hideSystemUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Window window = getWindow();
            window.setDecorFitsSystemWindows(false);
            WindowInsetsController controller = window.getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
    }

    private void setupImeLayoutAdjustment() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING); // legacy adjustPan is inert with edge-to-edge on R+
            View content = findViewById(android.R.id.content);
            content.setOnApplyWindowInsetsListener((v, insets) -> {
                int shift = 0;
                if (insets.isVisible(WindowInsets.Type.ime()) && mTextEdit != null) {
                    int imeBottom = insets.getInsets(WindowInsets.Type.ime()).bottom;
                    View dummyEdit = mTextEdit; // DummyEdit is package-private, access via View
                    if (dummyEdit.getLayoutParams() instanceof RelativeLayout.LayoutParams) {
                        RelativeLayout.LayoutParams p = (RelativeLayout.LayoutParams) dummyEdit.getLayoutParams();
                        shift = Math.max(0, p.topMargin + p.height - (v.getHeight() - imeBottom));
                    }
                }
                mLayout.setTranslationY(-shift);
                return v.onApplyWindowInsets(insets);
            });
        } else {
            getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN);
        }
    }

    @Override
    protected String[] getLibraries() {
        return new String[]{
            "SDL2",
            "main"
        };
    }

    @Override
    public void setRequestedOrientation(int requestedOrientation) {
        if (requestedOrientation == android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE) {
            requestedOrientation = android.content.pm.ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE;
        }
        super.setRequestedOrientation(requestedOrientation);
    }

    private static boolean hasGameResources(File dir) {
        if (dir == null || !dir.isDirectory()) return false;
        // Port note: Zuma Deluxe ships loose resources (no main.pak), so the
        // archive is not required - properties/resources.xml is the real marker.
        File props = new File(dir, "properties");
        return props.isDirectory() && new File(props, "resources.xml").isFile();
    }

    // Port note: for this test build the game data ships inside the APK under
    // assets/zuma/ and is unpacked once into getExternalFilesDir(), which is
    // where the framework looks for resources on Android. The user-driven
    // import flow (ResourceImportActivity) remains for the public release.
    private void extractBundledAssetsIfNeeded(File dest) {
        if (dest == null) return;
        if (new File(dest, "properties/resources.xml").isFile()) return;

        ArrayList<String> pending = new ArrayList<>();
        pending.add("zuma");
        long copied = 0;

        while (!pending.isEmpty()) {
            String path = pending.remove(pending.size() - 1);
            String[] children;
            try {
                children = getAssets().list(path);
            } catch (IOException e) {
                Log.e(TAG, "Asset listing failed for " + path, e);
                continue;
            }
            if (children == null) continue;

            for (String child : children) {
                String childPath = path + "/" + child;
                if (isAssetDir(childPath)) {
                    pending.add(childPath);
                } else {
                    copied += copyAsset(childPath,
                            new File(dest, childPath.substring("zuma/".length())));
                }
            }
        }

        Log.i(TAG, "Bundled assets extracted: " + copied + " bytes");
    }

    private boolean isAssetDir(String path) {
        try {
            String[] children = getAssets().list(path);
            return children != null && children.length > 0;
        } catch (IOException e) {
            return false;
        }
    }

    private long copyAsset(String assetPath, File outFile) {
        File parent = outFile.getParentFile();
        if (parent != null && !parent.exists()) parent.mkdirs();
        try (InputStream in = getAssets().open(assetPath);
             OutputStream out = new FileOutputStream(outFile)) {
            byte[] buf = new byte[65536];
            int n;
            long total = 0;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
                total += n;
            }
            return total;
        } catch (IOException e) {
            Log.e(TAG, "Failed copying asset " + assetPath, e);
            return 0;
        }
    }

    // --- Diagnostic log mirror ---------------------------------------------
    // The native side writes zuma-diag.log into getExternalFilesDir() from the
    // very start of SDL_main. This thread mirrors that file into the public
    // Downloads folder as zuma-diag.txt every couple of seconds so it can be
    // read from any file manager - even if the game hangs (this thread is
    // independent of SDL). Android 10+ uses MediaStore (no permission needed).
    private void truncateDiag() {
        File dir = getExternalFilesDir(null);
        if (dir == null) return;
        try (FileOutputStream fos = new FileOutputStream(new File(dir, "zuma-diag.log"), false)) {
            // truncate to zero for a clean per-launch log
        } catch (IOException e) {
            Log.e(TAG, "truncateDiag failed", e);
        }
    }

    private void appendDiag(String line) {
        File dir = getExternalFilesDir(null);
        if (dir == null) return;
        try (FileOutputStream fos = new FileOutputStream(new File(dir, "zuma-diag.log"), true)) {
            fos.write(("[java] " + line + "\n").getBytes(java.nio.charset.StandardCharsets.UTF_8));
        } catch (IOException e) {
            Log.e(TAG, "appendDiag failed", e);
        }
    }

    private void ensureLegacyStoragePermission() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            if (checkSelfPermission(android.Manifest.permission.WRITE_EXTERNAL_STORAGE)
                    != android.content.pm.PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{ android.Manifest.permission.WRITE_EXTERNAL_STORAGE }, 1001);
            }
        }
    }

    private void startLogMirror(File extDir) {
        if (extDir == null) return;
        final File nativeLog = new File(extDir, "zuma-diag.log");
        mLogMirrorStop = false;
        mLogMirrorThread = new Thread(() -> {
            long lastLen = -1;
            while (!mLogMirrorStop) {
                try { Thread.sleep(2000); } catch (InterruptedException ie) { return; }
                try {
                    long len = nativeLog.isFile() ? nativeLog.length() : 0;
                    if (len != lastLen) {
                        publishLogToDownloads(nativeLog);
                        lastLen = len;
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Log mirror failed", e);
                }
            }
        }, "zuma-log-mirror");
        mLogMirrorThread.setDaemon(true);
        mLogMirrorThread.start();
    }

    private void publishLogToDownloads(File logFile) {
        try {
            byte[] data = readAllBytes(logFile);
            boolean ok;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                ok = publishViaMediaStore(data);
                if (!ok) ok = publishDirect(data);
            } else {
                ok = publishDirect(data);
            }
            if (!ok) Log.e(TAG, "All Downloads publish strategies failed");
        } catch (Exception e) {
            Log.e(TAG, "publishLogToDownloads failed", e);
        }
    }

    private boolean publishViaMediaStore(byte[] data) {
        try {
            ContentResolver cr = getContentResolver();
            Uri collection = MediaStore.Downloads.EXTERNAL_CONTENT_URI;
            Uri existing = null;
            String[] proj = { MediaStore.Downloads._ID };
            String sel = MediaStore.Downloads.DISPLAY_NAME + "=?";
            String[] args = { "zuma-diag.txt" };
            try (Cursor c = cr.query(collection, proj, sel, args, null)) {
                if (c != null && c.moveToFirst()) {
                    existing = Uri.withAppendedPath(collection, String.valueOf(c.getLong(0)));
                }
            }
            Uri target = existing;
            if (target == null) {
                ContentValues values = new ContentValues();
                values.put(MediaStore.Downloads.DISPLAY_NAME, "zuma-diag.txt");
                values.put(MediaStore.Downloads.MIME_TYPE, "text/plain");
                values.put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS);
                target = cr.insert(collection, values);
            }
            if (target == null) return false;
            try (OutputStream os = cr.openOutputStream(target, "wt")) {
                if (os == null) return false;
                os.write(data);
                return true;
            }
        } catch (Exception e) {
            Log.e(TAG, "MediaStore publish failed", e);
            return false;
        }
    }

    private boolean publishDirect(byte[] data) {
        try {
            File dir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
            if (!dir.exists() && !dir.mkdirs()) return false;
            File out = new File(dir, "zuma-diag.txt");
            try (OutputStream os = new FileOutputStream(out)) {
                os.write(data);
            }
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Direct Downloads write failed", e);
            return false;
        }
    }

    private static byte[] readAllBytes(File f) throws IOException {
        if (!f.isFile()) return new byte[0];
        try (FileInputStream in = new FileInputStream(f);
             ByteArrayOutputStream bos = new ByteArrayOutputStream()) {
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) bos.write(buf, 0, n);
            return bos.toByteArray();
        }
    }
}
