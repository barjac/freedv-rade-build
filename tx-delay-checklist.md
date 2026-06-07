# FreeDV RADE TX Delay Checklist (RPi4)

## Symptom
- PTT button colour changes instantly, Hamlib PTT fires instantly
- "From mic" level meter shows immediately
- TX audio at radio output delayed 2-3 seconds after every PTT press
- Happens on ALL PTT presses in first session after clean boot
- Restarting FreeDV (via start script with freedv-audio-setup) fixes it for the whole session
- Nothing in FreeDV terminal output

## What was ruled out
- **RADE startup latency** — RADE is pure C, no Python. `RADETransmitStep::reset()` only drains FIFOs, no reinit.
- **CM108AH hardware suspension** — LED flashes throughout any FreeDV session; hardware never sleeps.
- **FreeDV TX output stream corked/idle** — `StreamWriteCallback_` always writes (silence at minimum via memset) so the PA stream is never idle.
- **BPF per-PTT startup** — bpf6.c loops continuously: reads capture → filters → writes playback. No per-PTT init.
- **CPU frequency** — not exposed on this Pi4.
- **RADE vocoder speed** — if RADE were too slow, TX would never work at all; it works fine after the delay.

## Root cause (most likely)
`session.suspend-timeout-seconds=0` in `freedv-main-sinks` has **ambiguous semantics** in WirePlumber.
Some builds treat `0` as "disable timeout → never suspend" (intended). Others treat it as
"suspend immediately on idle". If the latter:

1. During RX, FreeDV writes silence to `FDV_TX_out` null sink.
2. WirePlumber suspends `FDV_TX_out` immediately (0-second timeout).
3. PipeWire stops calling `StreamWriteCallback_` — RADE audio piles up in `outfifo1` undelivered.
4. On PTT: WirePlumber takes 2-3 s to wake the null sink. `StreamWriteCallback_` resumes and buffered audio flows.
5. CM108AH LED stays lit throughout — the **capture** side (radio RX audio) is unaffected.

"Restart fixes it" likely because `reset_wireplumber` restores a backed-up `stream-properties` that
contains a per-stream override (possibly `media.role=phone` recorded from a working session).

## Secondary suspect — `remove_bad_links` format mismatch
`remove_bad_links` does `grep -qF` to check whether each current link is in `freedv-links`.
The `freedv-links` file was captured **with FreeDV running**. On a fresh boot (FreeDV not yet running),
if `pw-link-ls` output format differs at all (different IDs, slightly different port names),
the grep fails and **BPF↔CM108AH links get deleted** after `restore_links`. FreeDV then starts
with BPF unconnected. Your plan to strip the four FDV_* sinks from the links file should help here.

## Patches already applied
| Patch | File | Change | Status |
|-------|------|--------|--------|
| B | `~/freedv_make/bpf6.c` | Add `media.role=phone` to `PIPEWIRE_PROPS` | Applied |

## Patches ready but not yet tested
| Patch | File | Change |
|-------|------|--------|
| A | `src/audio/PulseAudioDevice.cpp` | Use `pa_stream_new_with_proplist()` with `PA_PROP_MEDIA_ROLE="phone"` on all FreeDV PA streams |

Patch files: `~/freedv_make/patch-A-freedv-pipewire-role.patch`, `patch-B-bpf-pipewire-role.patch`

## Fix for null sink creation (not yet applied)
In `freedv-main-sinks`, change:
```bash
pactl load-module module-null-sink sink_name="$sink" \
    sink_properties=session.suspend-timeout-seconds=0
```
to:
```bash
pactl load-module module-null-sink sink_name="$sink" \
    sink_properties="media.role=phone session.suspend-timeout-seconds=9999"
```
`media.role=phone` tells WirePlumber the sink is a communication stream (exempt from suspension).
`9999` replaces the ambiguous `0` with a large explicit timeout as belt-and-braces.

## Diagnostic steps (if/when issue reappears)
1. **Confirm null sink state during RX** (first boot, before any PTT):
   ```bash
   pactl list sinks | grep -E "Name:|State:"
   ```
   If any `FDV_*` sink shows `State: SUSPENDED` → null sink suspension is confirmed cause.

2. **Check if StreamWriteCallback_ is firing** — enable FIFO state logging in FreeDV:
   Set `g_dump_fifo_state = 1` (search in `main.cpp` for how it's set) to log `outfifo1`
   used/free counts. If `outfifo1` fills but PipeWire doesn't drain it → stream not being called.

3. **Check BPF connectivity**:
   ```bash
   pw-link -l | grep -i bpf
   ```
   Should show two links: one from `FDV_TX_out` monitor into bpf, one from bpf out to CM108AH.
   If missing after `freedv-sound-setup` runs → `remove_bad_links` is deleting them.

4. **Bypass BPF entirely** (via qpwgraph + comment out of freedv-sound-setup) to isolate whether
   delay is in FreeDV→PipeWire leg or BPF→CM108AH leg.

5. **Check WirePlumber version**: `wpctl --version` — behaviour of `session.suspend-timeout-seconds=0`
   differs between WirePlumber 0.4.x and 0.5.x.

## Notes
- "From mic" meter is driven by `ResampleForPlotStep` which taps the pipeline **before** RADE encode —
  it showing immediately is expected and gives no information about RADE output.
- `outfifo1` capacity = 640 ms; `infifo2` capacity = 75 s.
- `deferReset_`: on first TX iteration after RX, pipeline is reset and FIFOs cleared, then returns
  without processing. RADE output begins on the second iteration (~20 ms later). Total theoretical
  startup delay from PTT ≈ 140-200 ms (RADE inherent latency) on a fast machine.
- RADE modem frame = 960 samples at 8 kHz = 120 ms; needs 12 LPCNet frames (432 features) per TX call.
