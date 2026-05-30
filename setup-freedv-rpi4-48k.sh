#!/bin/bash
# setup-freedv-rpi4-48k.sh — FreeDV RPi4 audio setup at 48000 Hz
# Tested: Raspberry Pi OS Trixie
# Use this to configure PipeWire and FreeDV at 48000 Hz (native BCM2835 rate)
# for single-card testing with no external USB audio devices attached.
set -e

echo "=== FreeDV RPi4 Audio Setup (48000 Hz) ==="

# --- Step 1: PipeWire 48000 Hz ---
echo "[1/4] Configuring PipeWire for 48000 Hz..."
mkdir -p ~/.config/pipewire/pipewire.conf.d
cat > ~/.config/pipewire/pipewire.conf.d/10-clock-rate.conf << 'EOF'
context.properties = {
    default.clock.rate = 48000
    default.clock.allowed-rates = [ 48000 ]
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
echo "[4/4] Updating FreeDV sample rates to 48000..."
freedv_conf="${HOME}/.config/freedv/freedv.conf"

if [ -f "$freedv_conf" ]; then
    for key in soundCard1InSampleRate soundCard1OutSampleRate \
               soundCard2InSampleRate soundCard2OutSampleRate; do
        if grep -q "^${key}=" "$freedv_conf"; then
            sed -i "s/^${key}=.*/${key}=48000/" "$freedv_conf"
        else
            echo "${key}=48000" >> "$freedv_conf"
        fi
    done
    echo "    Updated $freedv_conf"
else
    echo "    $freedv_conf not found — run FreeDV once, then re-run this script,"
    echo "    or set sample rates to 48000 manually in Tools → Audio Configuration."
fi

echo ""
echo "=== Done ==="
echo ""
echo "Next steps:"
echo "  1. Reboot (required for boot config changes)"
echo "  2. After reboot, restart PipeWire:"
echo "       systemctl --user restart pipewire pipewire-pulse wireplumber"
echo "  3. Verify PipeWire clock:"
echo "       pw-cli info 0 | grep clock.rate"
echo "       (should show: default.clock.rate = 48000)"
echo "  4. Launch FreeDV and confirm audio devices show 48000 Hz"
