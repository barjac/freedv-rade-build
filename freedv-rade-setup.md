# freedv-rade-setup (test version)

freedv-rade-setup sets up the sound routing and rig control for FreeDV on a
Linux PC that already has FreeDV installed with **freedv-rade-build**. It
asks simple questions; if you're not sure, press Enter for the recommended
answer.

It makes, for one radio:

- the four FreeDV "virtual cables" (null sinks) and the links between them,
  your radio's sound card and your headset/speakers,
- fixed names for your USB sound cards (they stay the same after a reboot),
- rig control through Hamlib's rigctld (CAT and PTT),
- an optional transmit band-pass filter,
- a start script, a desktop icon ("FreeDV radio *name*") and a FreeDV
  configuration just for this radio. Your existing FreeDV settings and start
  scripts are not changed.

**This is a test version.** Please send any problems, with the log file
(see below), to G4MKT.

## Before you start

- If this PC matters to you, take a backup or system snapshot first (on
  Linux Mint: Timeshift). On a spare test machine that's not needed.
- FreeDV needs **PipeWire**. If your PC still uses PulseAudio, the setup
  will offer to switch it to PipeWire. It shows you the commands to undo the
  switch *before* asking, and saves them in
  `~/.config/freedv-rade-setup/undo-pipewire-switch.txt`. It only switches
  if you answer **y**.
- Have your radio connected and switched on, and know which USB sound card
  is the radio's and which is your headset's.

## Running it

Start **freedv-rade-update** as usual (its desktop icon or
`~/freedv-rade-build/freedv-rade-update`) and choose **(A)udio & radio
setup** from the first menu. That also brings freedv-rade-setup up to date.

Or in a terminal:

    cd ~/freedv-rade-build
    ./freedv-rade-setup --check     # only checks your system (fixes are offered, each with a question)
    ./freedv-rade-setup             # the full setup

When it asks you to plug a USB sound card in, plug it into the socket it
will **always** stay in: its fixed name belongs to that socket.

At the end it offers a test: it links your radio, headset and filter to
FreeDV's FDV_ boxes and shows them in **qpwgraph**, then tidies up again.
FreeDV itself isn't shown in the test. Its own links to the FDV_ boxes
appear only when FreeDV is running and you press **Start Modem**.

Running it again later lets you test or change a radio (sound card, headset,
rig control, filter), or set up another radio.

## If something goes wrong

- Everything the setup did is logged in
  `~/freedv-rade-build/freedv-rade-setup.log` (the previous run is in
  `freedv-rade-setup.log.1`). Please send that file.
- To go back to PulseAudio, run the commands in
  `~/.config/freedv-rade-setup/undo-pipewire-switch.txt`, then reboot (or
  restore your snapshot, if you took one).
- If a script stops with "the sound server is stuck", reboot. This usually
  means a USB sound device dropped out while in use.
