# Android Configuration Manual — Keisho

Sega Model 1, Model 2, and Model 3 emulator for Android (`arm64-v8a`), supporting standalone game-picker launch as well as direct frontend launching (**RetroFeVLC**, Daijishō, ES-DE, Pegasus, LaunchBox).

---

## 1. Storage Permissions & App Directory Structure

### Storage Permission
To read ROMs, covers, bezels, and filter lists under `/storage/emulated/0/...`, Keisho requests **All Files Access** (`MANAGE_EXTERNAL_STORAGE`) on Android 11+ or external read/write permissions on Android 6–10.

### App Configuration & Data Directory
Stored by default at:
```text
/storage/emulated/0/Android/data/io.github.keisho_emu.keisho/files/
```
Generated files and folders:

| Path | Purpose |
|---|---|
| `keisho.ini` | Video, Audio, Input, Paths, Network, and System settings |
| `filters.ini` | User-editable `Includes` and `Excludes` ROM lists for Model 1, Model 2, and Model 3 |
| `nvram/` | Per-game arcade NVRAM and EEPROM saves (`.nv`, `.eeprom`, high scores) |
| `nvram/states/` | Quick-save and numbered save states (`<game>.<slot>.kstate`) |
| `nvram/textures/<game>/` | Custom texture `dump/` and `load/` directories |
| `artwork/` | Online scraper cover cache (when scraping is enabled) |
| `screenshots/` | Captured screenshots |

---

## 2. Per-System Folders (ROMs, Covers, Bezels)

In **Settings → Paths** (or directly in `keisho.ini`), each arcade board has its own independent folders.  
Even when pointing multiple systems at one large shared MAME ROM folder (e.g., 4,000+ archives), **Model 1 only lists Model 1 titles**, **Model 2 only lists Model 2 titles**, and **Model 3 only lists Model 3 titles**, strictly matching the `.zip`/`.7z` files present in each folder.

### Example `[paths]` section in `keisho.ini`

```ini
[paths]
model1_rom_dir = /storage/emulated/0/RetroFeVLC/collections/Arcade/roms
model2_rom_dir = /storage/emulated/0/RetroFeVLC/collections/Sega Model 2/roms
model3_rom_dir = /storage/emulated/0/RetroFeVLC/collections/Sega Model 3/roms

model1_artwork_dir = /storage/emulated/0/RetroFeVLC/collections/Arcade/medium_artwork/artwork_front
model2_artwork_dir = /storage/emulated/0/RetroFeVLC/collections/Sega Model 2/medium_artwork/artwork_front
model3_artwork_dir = /storage/emulated/0/RetroFeVLC/collections/Sega Model 3/medium_artwork/artwork_front

model1_bezel_dir = /storage/emulated/0/RetroFeVLC/collections/Arcade/medium_artwork/bezel
model2_bezel_dir = /storage/emulated/0/RetroFeVLC/collections/Sega Model 2/medium_artwork/bezel
model3_bezel_dir = /storage/emulated/0/RetroFeVLC/collections/Sega Model 3/medium_artwork/bezel
```

### Cover & Bezel Filename Matching
- **Covers (`artwork_dir`):** image file must share the **exact ROM filename stem** (`.png`, `.jpg`, `.jpeg`, `.bmp`, `.tga`).
  - Example: `srally2.zip` → `srally2.png` (or `.jpg`)
- **Bezels (`bezel_dir`):** requires **Video → Bezel** enabled. Searched in the active model's bezel folder in this order:
  1. `<rom_name>.png` (e.g., `daytona.png`)
  2. `<parent_set>.png` (e.g., for `doaa.zip`, checks `doaa.png` then `doa.png`)
  3. `default.png` (fallback bezel for that system)

### Required Model 2 I/O Board Device Sets (`daytona`, `desert`, `vcop`)
Keep these MAME device sets inside your **Model 2 ROM folder**:
- `model1io.zip` (`epr-14869.25` / `epr-14869b.25` / `epr-14869c.25`) — required for **Daytona USA** and **Desert Tank**
- `model1io2.zip` (`epr-17181.6` / `epr-16891.6` / `epr-18021.6`) — required for **Virtua Cop**

---

## 3. Editable Filter Lists: `filters.ini` (`Includes` & `Excludes`)

On first launch, Keisho automatically creates:
```text
/storage/emulated/0/Android/data/io.github.keisho_emu.keisho/files/filters.ini
```
It is created **empty by default** so no games are hidden out of the box. If you point a model slot to a shared full MAME ROMs folder, you can freely list stems under `[Model X Excludes]` or `[Model X Includes]` in any text editor.

### Sections in `filters.ini`
```ini
[Model 1 Includes]
; Empty = show all Model 1 games found in the folder (except Excludes).
; Add ROM names here (one per line) to show only those sets.

[Model 1 Excludes]
; Example: swaj

[Model 2 Includes]
; Example whitelist (leave empty to show all non-excluded Model 2 games):
; daytona
; srallyc
; vcop

[Model 2 Excludes]
; Example (useful when pointing at a full MAME ROMs folder):
; model1io
; daytonagtx

[Model 3 Includes]

[Model 3 Excludes]
; Example: srally2p
```

### Filter Rules
1. **Blacklist (`Excludes`):** add one ROM stem per line under `[Model X Excludes]` to hide specific clones, prototypes, or I/O sets (useful with shared MAME folders); comment with `;` or `#` or delete the line to show it again.
2. **Whitelist (`Includes`):** if `[Model X Includes]` has any entries, Keisho shows **only** those ROM stems for that model (explicit includes override excludes).
3. **RetroFeVLC list compatibility:** in addition to `filters.ini`, Keisho also reads `exclude.txt` and `include.txt` inside each ROM folder or its parent collection folder (e.g., `Sega Model 3/exclude.txt`).

---

## 4. Video: Vulkan, OpenGL ES, and Software

In **Settings → Video**:

| Option | Details |
|---|---|
| **Renderer** (`graphics_backend`) | `vulkan` (recommended default for modern Adreno/Mali GPUs), `opengl` (OpenGL ES 3.1), or `software` (CPU). Changing this in Settings and closing the menu immediately restarts the graphics backend in place. |
| **3D render scale** (`render_scale`) | `1x` (native 496×384) up to `8x` (~4K). Applies to Vulkan and OpenGL ES. |
| **Aspect** (`aspect_mode`) | `4:3`, `square`, `16:9` (true widescreen 3D with centered 2D), or `stretch`. |
| **Bezel** (`bezel`) | Draws the bezel overlay from the active model's bezel directory. |
| **FPS counter** (`show_fps`) | Displays live Hz/FPS and the active renderer (e.g., `57.5 FPS [Vulkan]`). |

Example `[video]` section in `keisho.ini`:
```ini
[video]
graphics_backend = vulkan
render_scale = 2
aspect_mode = 4:3
bezel = true
scaling_method = sharp
texture_filter = anisotropic
anisotropy = 4
show_fps = true
```

---

## 5. On-Screen Touch Controls vs. Physical Gamepad

### Virtual Gamepad Modes (`virtual_gamepad`)
Under **Settings → Input → Gamepad → Virtual gamepad** (or `keisho.ini` / frontend args):

| Mode | Value | Behavior |
|---|---|---|
| **Automatic** | `auto` | Shows on-screen touch controls **only** when no physical gamepad or wheel is connected. Automatically hides all touch controls once a Bluetooth/USB controller is detected. |
| **On** | `on` (`true`) | Always shows on-screen touch controls, even with a physical gamepad connected. |
| **Off** | `off` (`false`) | Completely disables the on-screen virtual controller and touch HUD (ideal for Android handhelds or dedicated controller setups with bezels). |

### On-Screen Buttons & Per-Game Cabinet Layouts
- **Top bar:** `FPS` | `SETTINGS` | **`SERVICE`** | **`TEST`** | `EXIT`
  - **`TEST`** opens the arcade board Test Menu (Free Play, wheel/gun calibration, difficulty, region).
  - **`SERVICE`** steps through Test Menu entries (on 2-button / gun boards without menu directions, tap **`SERVICE`** to move the cursor and **`TEST`** to confirm/select) or inserts service credits in game.
- **Bottom bar:** `COIN` | `SAVE` | `SLOT 1..4` | `LOAD` | `START`
- **Right-side controls tailored automatically per game:**
  - *Sega Rally 2*: `GAS`, `BRAKE`, `GEAR-`, `GEAR+`, **`VIEW`**, and **`H.BRAKE`**
  - *Sega Rally / Emergency Call Ambulance / Dirt Devils / Super GT 24h*: `GAS`, `BRAKE`, `GEAR-`, `GEAR+`, **`VIEW`**
  - *Daytona USA 1/2 / Scud Race / Le Mans 24 / Virtua Racing*: `GAS`, `BRAKE`, `GEAR-`, `GEAR+`, **`VR1`–`VR4`**
  - *Harley-Davidson*: `GAS`, `BRAKE`, `GEAR-`, `GEAR+`, **`MUSIC`**, **`VIEW`**
  - *Motor Raid*: `GAS`, `BRAKE`, **`PUNCH`**, **`KICK`** *(without unused gear shift buttons)*
  - *Fighters / Action / Sports / Light Guns / Flight / Ski*: cabinet-specific layouts & labels showing only the buttons each game actually uses (*Virtual On*, *Virtua Fighter*, *SpikeOut*, *Virtua Striker*, *Harikiri Pro Yakyuu '98*, *Desert Tank*, *Star Wars Trilogy*, *Wing War*, *Sega Bass Fishing*, *Top Skater*, *Ski Champ*, *Sega Water Ski*, *Virtua Cop*, *House of the Dead*, *Behind Enemy Lines*, etc.).

### Customizing Visible On-Screen Buttons & Stick Inversion Per Game
Under **Settings → Input → Gamepad → On-screen gamepad**:
- Pick **Buttons for** (`Default` for global settings, or any individual game family such as `Daytona USA`, `Virtua Striker`, `Motor Raid`, `Star Wars Trilogy`, etc.).
- Set **Invert stick** (`Off` / `Horizontal` / `Vertical` / `Both`) to flip left/right only, up/down only (for flight sticks such as *Star Wars*), or both axes — applying to both the on-screen touch stick and physical gamepads (`pad1_axis_aim_x_invert` / `pad1_axis_aim_y_invert` in `[<family>]`).
- Toggle individual on-screen controls or HUD pills (`COIN`, `START`, `SAVE / SLOT / LOAD`, `SERVICE / TEST`, `FPS`).
- Click **Show all** to quickly unhide every button in that profile (`SETTINGS` and `EXIT` remain visible so you can never lock yourself out).
- Per-game settings (`touch_hidden` and axis inversions) are saved automatically in `keisho.ini` under the game's `[<family>]` section.

### Shooting & Light-Gun Games (`Virtua Cop 1/2`, `House of the Dead`, `Behind Enemy Lines`, `Gunblade NY`, `Rail Chase 2`, `The Lost World`, `LA Machineguns`, `Ocean Hunter`)
All shooting/light-gun titles (across both Model 2 and Model 3) are detected automatically from `data/games.xml` and share the same touch aiming and stick settings.

Under **Settings → Input → Gamepad → On-screen gamepad** (and **Settings → Input → Light gun**):

| Setting | INI Key | Behavior |
|---|---|---|
| **Gun touch aim** | `touch_gun_aim = direct` *(default)* | **Direct touch:** tap or drag anywhere on the open picture to aim and fire without a center stick blocking the view. Lower corners hold `FIRE` and `RELOAD` (or `MSL` on *Behind Enemy Lines*). |
| **Gun touch aim** | `touch_gun_aim = stick` | **Analog stick:** places a fixed virtual analog stick in the bottom-left corner so you can aim smoothly without your fingers covering the playfield, with `FIRE` and `RELOAD`/`MSL` on the right. |
| **Stick aim sensitivity** | `lightgun_stick_sensitivity` (`25`–`300`, default `100`) | Scales cursor movement speed in standard stick mode, or center-curve response when **Centred stick aim** (`lightgun_pad_centred`) is turned on. |
| **Centred stick aim** | `lightgun_pad_centred` | Off: stick nudges the crosshair like a mouse. On: crosshair follows absolute stick deflection from center. |

> **Notes for *Behind Enemy Lines* (`bel.zip`):**
> - Full-range `93C46` EEPROM gun calibration (`MIN=00, DEL=FF`) is populated automatically when a blank or uncalibrated `.eeprom` is loaded, keeping the sight unlocked across the whole screen.
> - **`MSL`** drives hardware bit `IN1 0x10`; Stage 1 begins with **`MISSILE 0`** until missile crates/pickups are collected in-stage.

### Physical Controller Shortcuts (`Guide` / `Home` button)
- `Guide + A` → Open Pause / Settings Menu (also accessible via Android **Back** button)
- `Guide + L3` → **Service**
- `Guide + R3` → **Test**
- `Guide + X` / `Y` → Quick-Save / Quick-Load
- `Guide + Start` → Exit game (returns to frontend if launched externally, or to the game picker when opened directly)

---

## 6. Android Frontend Integration (RetroFeVLC, Daijishō, ES-DE, Pegasus)

When launched from an external frontend (ROM passed via Intent/arguments), exiting a game **closes Keisho directly** to return to the frontend. When opened directly via the app icon, exiting returns to the game picker.

### A. Command-Line Flags (CLI)
| Flag | Values | Description |
|---|---|---|
| `--virtual-gamepad <mode>` | `auto`, `on`, `off` | Override on-screen virtual controller mode |
| `--no-virtual-gamepad` | — | Shorthand for `--virtual-gamepad off` |
| `--graphics-backend <name>` | `vulkan`, `opengl`, `software` | Override graphics renderer for this run |
| `--game <set>` | e.g., `srally2` | Launch a specific ROM set (including clones inside merged ZIPs) |
| `--config <dir>` | path | Directory containing `keisho.ini` |
| `--filters <file>` | path | Custom `filters.ini` path |
| `--model1-rom-dir <dir>` | path | Override Model 1 ROM directory |
| `--model2-rom-dir <dir>` | path | Override Model 2 ROM directory |
| `--model3-rom-dir <dir>` | path | Override Model 3 ROM directory |
| `--render-scale <1..8>` | `1` to `8` | 3D internal resolution scale |

---

### B. Supported Android Intent Extras (`KeishoActivity`)

`io.github.keisho_emu.keisho/.KeishoActivity` accepts:
- **ROM path Extra:** `rom`, `ROM`, `rom_path`, `file`, `filepath`
- **Set name Extra:** `game` (e.g., `daytona`, `srally2`)
- **ROM Data URI:** `file:///...`, bare file path, or `content://...`
- **Virtual Gamepad Extra:** `virtual_gamepad`, `virtual-gamepad`, `touch_gamepad`, `touch_controls`, or `virtual_controller` (accepts string `auto` / `on` / `off` or boolean `true` / `false`)
- **Graphics Backend Extra:** `graphics_backend`, `graphics-backend`, `backend`, or `renderer` (`vulkan` / `opengl` / `software`)
- **Raw CLI Args Extra:** `arguments`, `args`, or `cli_args` (e.g., `"--virtual-gamepad off --graphics-backend vulkan"`)

---

### Copy-Paste Examples

#### 1. RetroFeVLC Android (`RetroFeVLC/launchers/Sega Model 2.conf` and `Sega Model 3.conf`)
Automatic virtual gamepad (hides automatically when a controller is connected):
```ini
# Android Standalone Launcher: Keisho (Sega Model 1 / 2 / 3)
executable = /system/bin/am
arguments = start -n io.github.keisho_emu.keisho/.KeishoActivity -e rom "%ITEM_FILEPATH%" -e virtual_gamepad auto --activity-clear-task --activity-exclude-from-recents
```

Force virtual gamepad **off** with **Vulkan** (recommended for dedicated controller / handheld setups):
```ini
# Android Standalone Launcher: Keisho (Sega Model 1 / 2 / 3)
executable = /system/bin/am
arguments = start -n io.github.keisho_emu.keisho/.KeishoActivity -e rom "%ITEM_FILEPATH%" -e virtual_gamepad off -e graphics_backend vulkan --activity-clear-task --activity-exclude-from-recents
```

#### 2. Daijishō (Custom Player / `am start`)
```text
am start -n io.github.keisho_emu.keisho/.KeishoActivity
  -a android.intent.action.VIEW
  -e rom "{file.path}"
  -e virtual_gamepad off
  -e graphics_backend vulkan
```
*(Use `-e virtual_gamepad auto` to auto-detect a connected gamepad.)*

#### 3. Pegasus Frontend (`metadata.pegasus.txt`)
```text
collection: Sega Model 3
shortname: model3
extensions: zip, 7z
launch: am start
  -n io.github.keisho_emu.keisho/.KeishoActivity
  -a android.intent.action.VIEW
  -d "{file.uri}"
  --es virtual_gamepad auto
  --es graphics_backend vulkan
```

#### 4. ES-DE (`es_systems.xml` on Android)
```xml
<command label="Keisho (Vulkan / Controller Only)">
  am start -n io.github.keisho_emu.keisho/.KeishoActivity -a android.intent.action.VIEW -e rom "%ROM%" -e virtual_gamepad off -e graphics_backend vulkan
</command>
```

#### 5. Quick ADB Test Commands
Launch Sega Rally 2 with Vulkan and virtual gamepad disabled:
```sh
adb shell am start -n io.github.keisho_emu.keisho/.KeishoActivity \
  -e rom "/storage/emulated/0/RetroFeVLC/collections/Sega Model 3/roms/srally2.zip" \
  -e virtual_gamepad off \
  -e graphics_backend vulkan \
  --activity-clear-task --activity-exclude-from-recents
```

Launch Daytona USA with automatic virtual gamepad:
```sh
adb shell am start -n io.github.keisho_emu.keisho/.KeishoActivity \
  -e rom "/storage/emulated/0/RetroFeVLC/collections/Sega Model 2/roms/daytona.zip" \
  -e virtual_gamepad auto \
  --activity-clear-task --activity-exclude-from-recents
```

---

## 7. Building the Android APK (`arm64-v8a`)

See [README.md](../README.md) and [BUILDING.md](../BUILDING.md) for cross-platform build documentation.

### Requirements
- **JDK 17+** (or Android Studio's bundled JBR)
- **Android SDK** (`compileSdk 35`, `minSdk 24`) with **NDK `29.0.14206865`**, **CMake 3.24+**, and **Ninja**
- Initialised git submodules: `git submodule update --init --recursive`

### Build steps
1. Create `android/local.properties` pointing to your Android SDK:
   ```properties
   sdk.dir=/path/to/Android/Sdk
   ```
2. Build the release APK (`Vulkan` + `OpenGL ES 3.1` + `software`, 16 KB page aligned):
   ```sh
   # Linux / macOS
   cd android
   ./gradlew assembleRelease
   ```
   ```powershell
   # Windows PowerShell
   $env:JAVA_HOME = 'C:\Program Files\Android\Android Studio\jbr'
   cd android
   .\gradlew.bat assembleRelease --console=plain
   ```
3. Output APK: `android/app/build/outputs/apk/release/app-release.apk`. Verify 16 KB ZIP alignment and install via ADB:
   ```sh
   zipalign -c -v -P 16 4 android/app/build/outputs/apk/release/app-release.apk
   adb install -r android/app/build/outputs/apk/release/app-release.apk
   ```
