package io.github.keisho_emu.keisho;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.OpenableColumns;
import android.provider.Settings;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

public class KeishoActivity extends SDLActivity {
    private static final int REQ_LEGACY_STORAGE = 1001;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
        super.onCreate(savedInstanceState);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            List<String> needed = new ArrayList<>();
            if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.READ_EXTERNAL_STORAGE);
            }
            if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.Q &&
                checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.WRITE_EXTERNAL_STORAGE);
            }
            if (!needed.isEmpty()) {
                requestPermissions(needed.toArray(new String[0]), REQ_LEGACY_STORAGE);
            }
        }
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL3",
            "main"
        };
    }

    @Override
    protected String[] getArguments() {
        Intent intent = getIntent();
        if (intent == null) {
            return new String[0];
        }

        List<String> args = new ArrayList<>();

        for (String key : new String[] {
                "virtual_gamepad", "virtual-gamepad", "touch_gamepad", "touch_controls", "virtual_controller" }) {
            if (intent.hasExtra(key)) {
                Bundle extras = intent.getExtras();
                Object raw = extras != null ? extras.get(key) : null;
                if (raw instanceof Boolean) {
                    args.add("--virtual-gamepad");
                    args.add(((Boolean) raw) ? "on" : "off");
                    break;
                } else if (raw != null) {
                    String s = String.valueOf(raw).trim();
                    if (!s.isEmpty()) {
                        args.add("--virtual-gamepad");
                        args.add(s);
                        break;
                    }
                }
            }
        }

        for (String key : new String[] { "graphics_backend", "graphics-backend", "backend", "renderer" }) {
            String val = intent.getStringExtra(key);
            if (val != null && !val.trim().isEmpty()) {
                args.add("--graphics-backend");
                args.add(val.trim().toLowerCase());
                break;
            }
        }

        for (String key : new String[] { "arguments", "args", "cli_args" }) {
            String raw = intent.getStringExtra(key);
            if (raw != null && !raw.trim().isEmpty()) {
                appendSplitArgs(raw, args);
                break;
            }
        }

        String rom = null;
        for (String key : new String[] { "rom", "ROM", "rom_path", "file", "filepath" }) {
            String val = intent.getStringExtra(key);
            if (val != null && !val.isEmpty()) {
                rom = val;
                break;
            }
        }
        if (rom != null) {
            args.add(rom);
            return args.toArray(new String[0]);
        }

        String game = intent.getStringExtra("game");
        if (game != null && !game.isEmpty()) {
            args.add("--game");
            args.add(game);
            return args.toArray(new String[0]);
        }

        Uri data = intent.getData();
        if (data != null) {
            String scheme = data.getScheme();
            if ((scheme == null || "file".equalsIgnoreCase(scheme)) && data.getPath() != null) {
                args.add(data.getPath());
                return args.toArray(new String[0]);
            }
            if ("content".equalsIgnoreCase(scheme)) {
                String cached = copyContentUriToCache(data);
                if (cached != null) {
                    args.add(cached);
                    return args.toArray(new String[0]);
                }
            }
        }

        return args.toArray(new String[0]);
    }

    private static void appendSplitArgs(String raw, List<String> out) {
        StringBuilder cur = new StringBuilder();
        boolean inQuotes = false;
        for (int i = 0; i < raw.length(); i++) {
            char c = raw.charAt(i);
            if (c == '"') {
                inQuotes = !inQuotes;
            } else if (Character.isWhitespace(c) && !inQuotes) {
                if (cur.length() > 0) {
                    out.add(cur.toString());
                    cur.setLength(0);
                }
            } else {
                cur.append(c);
            }
        }
        if (cur.length() > 0) {
            out.add(cur.toString());
        }
    }

    private String copyContentUriToCache(Uri uri) {
        String name = "launched.zip";
        try (Cursor c = getContentResolver().query(uri, new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (c != null && c.moveToFirst()) {
                String n = c.getString(0);
                if (n != null && !n.isEmpty()) {
                    name = new File(n).getName();
                }
            }
        } catch (Exception ignored) {
        }
        File dir = new File(getCacheDir(), "launched_roms");
        if (!dir.exists() && !dir.mkdirs()) {
            return null;
        }
        File out = new File(dir, name);
        try (InputStream in = getContentResolver().openInputStream(uri);
             FileOutputStream fos = new FileOutputStream(out)) {
            if (in == null) return null;
            byte[] buf = new byte[65536];
            int r;
            while ((r = in.read(buf)) > 0) {
                fos.write(buf, 0, r);
            }
            return out.getAbsolutePath();
        } catch (Exception ignored) {
            return null;
        }
    }

    public static boolean hasStorageAccess() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return Environment.isExternalStorageManager();
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && mSingleton != null) {
            return mSingleton.checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE)
                == PackageManager.PERMISSION_GRANTED;
        }
        return true;
    }

    public static void requestStorageAccess() {
        if (mSingleton == null) {
            return;
        }
        mSingleton.runOnUiThread(() -> {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                try {
                    Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                    intent.setData(Uri.parse("package:" + mSingleton.getPackageName()));
                    mSingleton.startActivity(intent);
                } catch (Exception e) {
                    try {
                        Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                        mSingleton.startActivity(intent);
                    } catch (Exception ignored) {
                    }
                }
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                mSingleton.requestPermissions(new String[] {
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE
                }, REQ_LEGACY_STORAGE);
            }
        });
    }
}
