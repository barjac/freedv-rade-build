 # freedv-rade-build

This script automates installation of FreeDV RADE in modern Linux distibutions.

It is currently tested in **Mageia, LinuxMint, Fedora, Ubuntu, Xubuntu, Kubuntu, Manjaro OpenSUSE Tumbleweed Garuda EndevourOS Gentoo and Debian**

  |**Distribution**      |**Status** | **Notes** | 
  |:---              | :----: | :--- |
  |Mageia 9          |OK    | Current stable|
  |Mageia 10 (-dev)  |OK    | Next release  |
  |Linux Mint 21 & 22|OK    |               |
  |OpenSUSE Tumbleweed|OK   |               |
  |Debian 12 & 13    |OK    |Thanks to Uli DF7SC for testing!
  |Fedora 40, 41, 42 |OK    |               |
  |Ubuntu 22/24/25.04 LTS   |OK    |Thanks to LU3JIJ for testing!|
  |Manjaro           |OK    |Thanks to M0SSN for testing!|
  |Xubuntu 24.04     |OK    |Thanks to DL7AIS for testing!|
  |Kubuntu Oracular (Dev)|OK |Thanks to sblandford for testing!|
  |Arch              |??    |Supported - not tetsed|
  |Garuda            |OK    |Thanks to Dave Baxter NB4S for testing! |
  |EndeavourOS       |OK    |Thanks to Dave Baxter NB4S for testing! |
  |Arco Linux        |??    |Supported - not tetsed       |
  |RebornOS          |??    |Supported - not tetsed       |
  |Gentoo            |OK    |Thanks to W6KS for help and testing!
  
## What it does
If you are not interested in how this all works and just want to use it then skip to the **Using the script** section below.

It initially checks which Linux distribution is in use, that it is 64 bit, that disk space is adequate, whether internet with working DNS is
available and that the script is being run as a regular user.

It also checks for a .freedv-rade-build.cfg file which may be used to set a specific build directory outside the user's home folder.
This change has been made in response to Issue#3, however unless you have a specific need to use this feature then just ignore it,
especially if you are not a regular Linux user.
More help on this is in ~/freedv-rade-build/freedv-rade-anywhere.txt

Then it uses the distro's native package manager to install some essential system packages for which it may request the root password.

The remaining script then continues as the regular user.

A working directory called freedv-rade is created in the user's home folder (or alternative 'base' folder) which is used to hold the complete installation.

The freedv-gui sources are cloned from github master branch.

A python3 virtual environment is then automatically created and activated as described in the freedv-gui README.md

Several essential python modules are then installed by python pip in the virtual environment.

build_linux.sh is then run to continue with the download and building of more dependencies and ultimately freedv.

On completion of the build a start script is written to the user's home folder called freedv-start which is then made executable ready for use.
'./freedv-start' or optionally './freedv-start -f yourfreedv.conf' should start freedv.

The 'freedv-start' script already includes code (commented out by default) to start and stop hamlib rigctld, this needs editing to suit the user's radio. See notes in the script.

A desktop file to start FreeDV is added to the user's ~/Desktop folder. This calls the 'freedv-start' script so changes there e.g. rigctld settings will still work. 

**Important** If you start rigctld from 'freedv-start' and want to run two instances of FreeDV-RADE at the same time, e.g. a second one to monitor an SDR,  then you
 must use two differently named start scripts.

Copy the original under a new name for the SDR instance without rigctld enabled and use the original for your TX/RX instance with rigctld activated.

If you want a second desktop file for the SDR instance then you can edit the original to point at the new start script and then create a new main one using the 'Update-RADE' script.

## News

- 7 December 2024 Added a desktop icon to start FreeDV from the desktop

- 8 December 2024 mk-start and mk-desktop scripts may be used stand alone to create 'freedv-start' or install
'freedv-rade.desktop' files respectively, without running freedv-rade-build.

- 10 December 2024 Icon added to desktop for Update-FreeDV during a full build.

- 28 March 2025 Added option to use a build folder anywhere, not only in the user's home.

- 4 June 2025 Updated for the release of FreeDV-2.0.0 which now uses master branch.

- July 2025 Added log of the build to help with debugging when needed.
Also added support for several Arch Linux based distros.

## Using the script

   N.B. Always **copy/paste** commands from here **excluding** the surrounding ' ' to avoid typos!

    1. Install the 'git' package using your package manager (git-core in SUSE).

    2. Go to a terminal emulator on your machine and type: 'cd' followed by the 
    ENTER key to be sure you are in your home directory.

    3. Type: 'git clone https://github.com/barjac/freedv-rade-build' then ENTER

    4. Type: 'chmod +x freedv-rade-build/freedv-rade-build' then ENTER

    5. Type: 'freedv-rade-build/freedv-rade-build' then ENTER

    6. Enter the root password when prompted to install some system files then ENTER

Now put the kettle on, it will take a while!

On completion you should see a message to that effect and instructions on how to launch the program.

If you are using hamlib don't forget to add yourself (as root) to the 'dialout' group:

\# usermod -aG dialout <your_user_name>

Or in Arch based distros like Manjaro, Garuda etc. the 'uucp' group:

\# usermod -aG uucp <your_user_name>

To make that active you will need to reboot the system.

## Testing new updates
There is a 'freedv-rade-update' script which allows updating of your freedv-rade (created using freedv-rade-build), full rebuilds, backup/restore and new desktop file creation from a simple text menu.
This can now be run from the Update-FreeDV desktop icon, which for recent installs will already be installed.

**NOTE* As this installation of FreeDV is not under your system's package management control, a system update (especially in a 'Rolling release' distro)
could break FreeDV. If this happens then you will need to run the FreeDV update script and use the 'Full rebuild' option. This will not destroy any settings you have made or re-create any default start scripts or desktop files.

N.B. Always **copy/paste** commands from here **excluding** the surrounding ' ' to avoid typos!

If you have an early clone of freedv-rade-build then you can update it as follows:

    1. 'cd ~/freedv-rade-build'

    2. 'git pull' to update it

    3. 'cd && chmod +x freedv-rade-build/freedv-rade-update' to make the update script executable

    4. 'freedv-rade-build/freedv-rade-update' to run it as often as you like
        OR use the Update-RADE desktop icon to run it.

Just follow the prompts.

If you hit a problem please open an issue here and attach the file: ~/freedv-rade-build/freedv-rade-build.log

Please do not create issues related to freedv-rade-build to the upstream freedv-gui project! They are busy enough!

***Have fun with FreeDV!***

