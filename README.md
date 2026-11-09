# Universal KeyMapper (Any Device & WebUI)

Universal native hardware key remapping module for **KernelSU Next**, **KernelSU**, **APatch**, and **Magisk**.

Works on **ANY Android device** (Nothing, CMF Phone, Google Pixel, Samsung Galaxy, Xiaomi, OnePlus, Sony Xperia, Motorola, ASUS ROG, etc.) and AOSP custom ROMs (Voltage OS, LineageOS, crDroid, etc.).

---

## ⚡ Features

- **🌐 KernelSU WebUI / WebView Dashboard**:
  - Full-featured, responsive Material dark-mode dashboard right inside **KernelSU Next**, **KernelSU**, **APatch**, or **MMRL**.
  - Accessible via built-in local micro HTTP server on `http://127.0.0.1:8321`.
- **📜 Live Log Viewer (Built-in WebUI Log Option)**:
  - Real-time stream of `keyactiond.log`.
  - Filter logs by: All, Action Triggers, Key Events, and Errors.
  - One-tap Clear Logs, Copy Logs to clipboard, and Auto-refresh (1s / 2s / off).
- **🔍 Universal Device Hardware Key Auto-Detection**:
  - Automatically scans `/dev/input/event*` on installation or via `keymapper detect`.
  - Discovers all physical keys:
    - **Volume Up & Volume Down** (`KEY_VOLUMEUP`, `KEY_VOLUMEDOWN`)
    - **Power Button** (`KEY_POWER`)
    - **Nothing / CMF Essential Action Key** (`KEY_ESSENTIAL` / `0xfa`)
    - **Sony Xperia Two-Stage Camera Button** (Focus `528` & Shutter `212`)
    - **Samsung Bixby / AI Button** (`703` / `583`)
    - **Mute / Alert Sliders** (`KEY_MUTE`)
    - **Fingerprint sensor swipe gestures**
- **🎮 Multi-Gesture Recognition per Key**:
  - **Single Press (Click)**
  - **Double Press (Double Click)**
  - **Triple Press (Triple Click)**
  - **Long Press (Hold)**
  - **Hardware Key Combinations** (e.g. VolUp + VolDown)
  - **Screen-Off Actions** (Skip / Previous Track, toggle Torch while screen is locked)
- **🚀 High-Performance Native Daemon (`keyactiond`)**:
  - Compiled statically for `aarch64` with Android NDK Clang (`-O3 -static -flto`).
  - Uses Linux kernel `epoll` and `timerfd` for zero CPU usage and zero battery drain when idle.
- **🔄 Hot Reloading**:
  - Edit settings in the WebUI or `keymapper.conf`; changes apply immediately without rebooting!

---

## 🛠 Supported Actions

| Action Name | Description |
|---|---|
| `default` | Standard Android behavior (e.g., volume change, power menu) |
| `none` | Ignore event / do nothing |
| `flashlight` / `torch` | Toggle camera flashlight ON / OFF |
| `screenshot` | Capture screenshot (`input keyevent 120`) |
| `play_pause` | Toggle media playback (`cmd media_session dispatch play-pause`) |
| `next_track` | Skip to next audio track |
| `prev_track` | Skip to previous audio track |
| `volume_up` | Increase media volume |
| `volume_down` | Decrease media volume |
| `mute_toggle` | Cycle ringer: Normal ➔ Vibrate ➔ Silent |
| `screen_off` / `lock` | Turn off display / lock device |
| `screen_on` / `wake` | Wake display up |
| `camera` | Launch default camera application |
| `notifications` | Pull down notification shade |
| `quick_settings` | Pull down Quick Settings tiles |
| `kill_app` | Force close current foreground application |
| `shell <command>` | Run any custom root shell command (e.g., `shell reboot`) |

---

## 📱 Terminal CLI Commands

Open **Termux** or any root shell on your phone:

```bash
# Check daemon status, port, and auto-detected keys
su -c keymapper status

# Auto-detect all hardware keys on this device
su -c keymapper detect
su -c keymapper detect --json

# Live interactive button test (press keys to view keycodes & timings)
su -c keymapper test

# Reload keymapper.conf without restarting daemon
su -c keymapper reload

# View recent daemon activity logs
su -c keymapper log

# Restart daemon
su -c keymapper restart
```

---

## 📦 How to Build and Install

1. Clone repository and run `build.sh`:
   ```bash
   chmod +x build.sh
   ./build.sh
   ```
2. The zip file will be generated in `dist/Universal-KeyMapper-v2.0.0.zip` and automatically pushed to `/sdcard/Download/` if your device is connected via USB debugging!
3. Open **KernelSU Next** (or **KernelSU** / **APatch** / **Magisk**), navigate to the **Modules** tab, tap **Install from storage**, select `Universal-KeyMapper-v2.0.0.zip`, and reboot!
4. Tap **WebUI** on the module card to open the visual manager!
