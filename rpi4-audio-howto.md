# FreeDV on Raspberry Pi 4: Audio Setup Guide

This guide covers the audio configuration issues encountered running FreeDV with PipeWire on RPi4,
and how to fix them. Tested on **Raspberry Pi OS Trixie**.

---

## The Problem

With PipeWire running at its default rate of 48000 Hz, FreeDV on the RPi4 can exhibit:

- Crackling and fizzing on received audio
- TX mic signal that sounds like processor noise / spikes
- Possible GUI instability under load

The same hardware and software works without issue on x86_64. The root cause on RPi4 is not
fully established, but the fix is consistent and reliable: set PipeWire to run at 44100 Hz.
This resolves the problems on every RPi4 installation tested.

---

## Prerequisites

Install these packages via your distro's package manager before starting:

- `pipewire`, `wireplumber` — audio server (usually pre-installed on desktop RPi distros)
- `rtkit` — real-time scheduling for audio threads
- `pipewire-pulse` — PulseAudio compatibility layer (needed by some apps)

---

## Step 1: Force PipeWire to 44100 Hz

Create a PipeWire configuration drop-in. This works on any PipeWire-based distro:

```bash
mkdir -p ~/.config/pipewire/pipewire.conf.d
cat > ~/.config/pipewire/pipewire.conf.d/10-clock-rate.conf << 'EOF'
context.properties = {
    default.clock.rate = 44100
    default.clock.allowed-rates = [ 44100 ]
}
EOF
```

Restart PipeWire to apply:

```bash
systemctl --user restart pipewire pipewire-pulse wireplumber
```

Verify the clock changed:

```bash
pw-cli info 0 | grep clock.rate
# Expected output includes: default.clock.rate = 44100
```

### If the clock rate doesn't change: check for a conflicting WirePlumber config

Some distros ship a WirePlumber drop-in that overrides the USB device rate back to 48000 Hz.
Check for it and remove if present:

```bash
ls ~/.config/wireplumber/wireplumber.conf.d/
# If a file exists that contains rate = 48000, remove it and restart:
systemctl --user restart wireplumber
```

---

## Step 2: Check Boot Audio Configuration

The RPi4 has onboard audio: a BCM2835 headphone jack (3.5mm) and HDMI audio via the
vc4-kms-v3d driver. The BCM2835 headphone jack is useful as an additional audio routing
option. HDMI audio should be kept disabled — enabling it can cause the Pi to negotiate a
different HDMI display mode, which may change the screen resolution unexpectedly.

Check `/boot/firmware/config.txt` contains:

```
dtparam=audio=on
dtoverlay=vc4-kms-v3d,noaudio
```

- `dtparam=audio=on` enables the BCM2835 headphone jack
- `noaudio` on the `vc4-kms-v3d` line disables HDMI audio only — it does not affect the headphone jack

If `dtparam=audio=off` is set, change it to `on` to enable the headphone jack. A **reboot** is required for boot config changes to take effect.

---

## Step 3: Enable RTKit for Real-Time Scheduling

RTKit allows PipeWire's audio threads to run at real-time priority, reducing latency and drop-outs.

```bash
sudo systemctl enable rtkit-daemon
sudo systemctl start rtkit-daemon
```

---

## Step 4: Set FreeDV Sample Rates to 44100 Hz

FreeDV stores its audio configuration in `~/.config/freedv/freedv.conf`.
All four sample rate keys must match the PipeWire clock rate:

```bash
freedv_conf="${HOME}/.config/freedv/freedv.conf"

for key in soundCard1InSampleRate soundCard1OutSampleRate \
           soundCard2InSampleRate soundCard2OutSampleRate; do
    sed -i "s/^${key}=.*/${key}=44100/" "$freedv_conf"
done
```

If the config file doesn't exist yet (first run): launch FreeDV, go to
**Tools → Audio Configuration**, set all four device sample rates to 44100 Hz, and save.

---

## Setup Script

This script automates steps 1–4. Run as your normal user (not root); it uses `sudo` only where
needed.

```bash
#!/bin/bash
# setup-freedv-rpi4.sh — FreeDV RPi4 audio setup
# Tested: Raspberry Pi OS Trixie
set -e

echo "=== FreeDV RPi4 Audio Setup ==="

# --- Step 1: PipeWire 44100 Hz ---
echo "[1/4] Configuring PipeWire for 44100 Hz..."
mkdir -p ~/.config/pipewire/pipewire.conf.d
cat > ~/.config/pipewire/pipewire.conf.d/10-clock-rate.conf << 'EOF'
context.properties = {
    default.clock.rate = 44100
    default.clock.allowed-rates = [ 44100 ]
}
EOF
echo "    Created ~/.config/pipewire/pipewire.conf.d/10-clock-rate.conf"

# --- Step 2: Ensure BCM2835 headphone jack is enabled, HDMI audio stays disabled ---
echo "[2/4] Checking boot audio config..."
for cfg in /boot/firmware/config.txt /boot/config.txt; do
    if [ -f "$cfg" ]; then
        echo "    Found: $cfg"
        sudo sed -i 's/dtparam=audio=off/dtparam=audio=on/' "$cfg"
        # Leave vc4-kms-v3d,noaudio unchanged — HDMI audio stays disabled so
        # PipeWire only sees the BCM2835 device (one clock source).
        break
    fi
done

# --- Step 3: RTKit ---
echo "[3/4] Enabling RTKit real-time daemon..."
if systemctl list-unit-files rtkit-daemon.service &>/dev/null; then
    sudo systemctl enable rtkit-daemon
    sudo systemctl start rtkit-daemon
    echo "    RTKit enabled and started"
else
    echo "    rtkit-daemon not found — install rtkit via your package manager first"
fi

# --- Step 4: FreeDV config ---
echo "[4/4] Updating FreeDV sample rates to 44100..."
freedv_conf="${HOME}/.config/freedv/freedv.conf"

if [ -f "$freedv_conf" ]; then
    for key in soundCard1InSampleRate soundCard1OutSampleRate \
               soundCard2InSampleRate soundCard2OutSampleRate; do
        if grep -q "^${key}=" "$freedv_conf"; then
            sed -i "s/^${key}=.*/${key}=44100/" "$freedv_conf"
        else
            echo "${key}=44100" >> "$freedv_conf"
        fi
    done
    echo "    Updated $freedv_conf"
else
    echo "    $freedv_conf not found — run FreeDV once, then re-run this script,"
    echo "    or set sample rates to 44100 manually in Tools → Audio Configuration."
fi

echo ""
echo "=== Done ==="
echo ""
echo "Next steps:"
echo "  1. Reboot (required for boot config changes to disable HDMI audio)"
echo "  2. After reboot, restart PipeWire:"
echo "       systemctl --user restart pipewire pipewire-pulse wireplumber"
echo "  3. Verify PipeWire clock:"
echo "       pw-cli info 0 | grep clock.rate"
echo "       (should show: default.clock.rate = 44100)"
echo "  4. Launch FreeDV and confirm audio devices show 44100 Hz"
```

---

## Verification

After rebooting and restarting PipeWire:

```bash
# PipeWire clock rate
pw-cli info 0 | grep clock.rate
# Expected: default.clock.rate = 44100

# Active audio nodes — should NOT show bcm2835 or hdmi audio
pw-cli list-objects | grep -i audio

# RTKit running
systemctl status rtkit-daemon
```

---

## Notes

- **RNNoise and AGC**: PiOS Trixie has sufficient CPU headroom to run both together without
  issues — enabling them is fine. On lower-spec hardware or other distros with higher CPU
  overhead, try them individually first and disable if you see audio glitching.

- **Volume control tools** (e.g. pw-volctl): restart them after any PipeWire restart — node IDs
  change when PipeWire restarts, and stale IDs cause volume changes to silently have no effect.

- If you still hear crackling after applying the fix, check whether your specific device prefers
  a different rate:
  ```bash
  pw-cli dump | grep -A10 "audio.rate"
  ```

- FreeDV's config file is `~/.config/freedv/freedv.conf` on all platforms.

- **Window positions not remembered**: On Wayland, compositors ignore application position
  requests, so FreeDV's saved window positions are not restored on next launch. Setting
  `GDK_BACKEND=x11` in the startup script forces FreeDV through XWayland (which does honour
  position requests), but it is not included by default as it has caused problems on some RPi
  configurations. On RPi4/PiOS Trixie: the main window position is restored correctly, but the
  Reporter dialogue ignores its saved position and always opens mid-screen. Use it if main
  window placement matters more than the Reporter dialogue position.

---

## Bonus: Show Desktop button and keyboard shortcut (PiOS Trixie / labwc)

PiOS Trixie uses the **labwc** Wayland compositor with **wf-panel-pi**. Neither provides a
show-desktop function out of the box. This adds both a panel button and a `Super+D` shortcut.

**Install wlrctl** (native Wayland window control for wlroots compositors):

```bash
sudo apt install wlrctl
```

**Create the script:**

```bash
sudo tee /usr/local/bin/show-desktop > /dev/null << 'EOF'
#!/bin/sh
WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-wayland-0} wlrctl toplevel minimize state:unminimized
EOF
sudo chmod +x /usr/local/bin/show-desktop
```

**Create a .desktop file** (for the panel launcher):

```bash
sudo tee /usr/local/share/applications/show-desktop.desktop > /dev/null << 'EOF'
[Desktop Entry]
Name=Show Desktop
Comment=Minimise all windows
Exec=/usr/local/bin/show-desktop
Icon=user-desktop
Type=Application
Categories=Utility;
EOF
```

**Add `Super+D` keybinding to labwc.** labwc is launched with `-m` (merge mode) so a minimal
user config is merged with the system config — existing shortcuts are preserved:

```bash
mkdir -p ~/.config/labwc
cat > ~/.config/labwc/rc.xml << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<openbox_config xmlns="http://openbox.org/3.4/rc">
  <keyboard>
    <keybind key="W-d">
      <action name="Execute">
        <command>/usr/local/bin/show-desktop</command>
      </action>
    </keybind>
  </keyboard>
</openbox_config>
EOF
```

**Add the panel button.** Copy the default panel config (if not already customised) and add
`show-desktop` to the launchers list:

```bash
[ -f ~/.config/wf-panel-pi/wf-panel-pi.ini ] || \
    cp /etc/xdg/wf-panel-pi/wf-panel-pi.ini ~/.config/wf-panel-pi/wf-panel-pi.ini

sed -i 's/^launchers=\(.*\)/launchers=\1 show-desktop/' ~/.config/wf-panel-pi/wf-panel-pi.ini
```

**Apply without rebooting:**

```bash
killall -s SIGHUP labwc          # reload keybindings
killall -s SIGHUP wf-panel-pi   # reload panel
```

---

## Bonus: qpwgraph — PipeWire connection graph

**qpwgraph** is a Qt6 GUI that shows all PipeWire nodes and lets you draw connections between
them. Useful for verifying that FreeDV is routed to the correct audio devices and for diagnosing
routing problems.

```bash
sudo apt install qpwgraph
```

It appears in the application menu after install. Node IDs and connections update live as
devices connect or disconnect.

---

## Bonus: pw-volctl — PipeWire volume control

**pw-volctl** is a lightweight GTK4 volume control for PipeWire. It lists all audio nodes with
their current levels and lets you adjust them in 1% or 5% steps, including above 100% (up to
200%) for devices that need a software boost. Levels can be saved to a named preset file and
reloaded, which is useful for quickly restoring known-good levels after a PipeWire restart.

It is not packaged — build from source using the script below. The source file (`pw-volctl.c`)
must be obtained separately.

**Build dependencies** (install via your package manager):
- `libgtk-4-dev`, `gcc`, `pkg-config`

**Build and install script:**

```bash
#!/bin/bash
# build-pw-volctl.sh — build and install pw-volctl from source
# Run from the directory containing pw-volctl.c
set -e

echo "Building pw-volctl..."
gcc $(pkg-config --cflags gtk4) -o pw-volctl pw-volctl.c $(pkg-config --libs gtk4) -lm

echo "Installing to /usr/local/bin/..."
sudo install -m 755 pw-volctl /usr/local/bin/pw-volctl

echo "Installing desktop entry..."
sudo mkdir -p /usr/local/share/applications
sudo tee /usr/local/share/applications/pw-volctl.desktop > /dev/null << 'EOF'
[Desktop Entry]
Name=PipeWire Volume Control
Comment=Adjust PipeWire audio device volumes
Exec=pw-volctl
Icon=audio-volume-high
Type=Application
Categories=AudioVideo;Audio;Mixer;
EOF

cp /usr/local/share/applications/pw-volctl.desktop ~/Desktop/
chmod +x ~/Desktop/pw-volctl.desktop

echo "Done. Run pw-volctl or use the desktop icon."
```

**Important:** pw-volctl reads live PipeWire node IDs at startup. If PipeWire is restarted
(e.g. after changing the clock rate), node IDs change and pw-volctl must be restarted — or
click **Refresh live** — otherwise volume changes will have no effect.
