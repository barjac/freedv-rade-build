 # freedv-rade-build

This script automates installation of the new RADE development versions of freedv-gui-2.0-dev in modern Linux distibutions.

It is currently tested in **Mageia, LinuxMint, Fedora, Ubuntu, Xubuntu and Debian**

It initially checks which Linux distribution is in use, that disk space is adequate, whether internet with working DNS is available and that the script is being run as a regular user.

Then it uses the distro's native package manager to install some essential system packages for which it requests the root password.

The remaining script then continues as the regular user.

A working directory called freedv-rade is created in the user's home directory which is used to hold the complete installation.

The freedv-gui sources are cloned from github and (at present) the v2.0-dev branch is checked out.

A python3 virtual environment is then automatically created and activated as described in the freedv-gui README.md

Several essential python modules are then installed by python pip in the virtual environment.

build_linux.sh is then run to continue with the download and building of more dependencies and ultimately freedv.

On completion of the build a start script is written to the user's home folder called freedv-start which is then made executable ready for use.
'./freedv-start' or optionally './freedv-start -f yourfreedv.conf' should start freedv v2.0-dev.

The start script includes code to start and stop hamlib rigctld but this needs editing and enabling in the script to suit user's radio etc.

It is currently tested in x86_64 installations of the following:
  
  |Distribution      |Status | Notes | 
  |:---              | :----: | :--- |
  |Mageia 9          |OK    | Current stable|
  |Mageia 10 (-dev)  |OK    |               |
  |Linux Mint 22     |OK    |               |
  |Fedora 41         |OK    | Full release  |
  |Ubuntu 24.04 LTS  |OK    |               |
  |Xubuntu 24.04     |OK    |Thanks to DL7AIS for testing!|
  |*buntu clones     |??    |Should be OK if 'cat /etc/os-release\|grep "ID="\|cut -d= -f2' returns 'ubuntu'|
  |Manjaro           |OK    |Thanks to M0SSN for testing!|
  |Debian 12 (bookworm)|OK | KDE Plasma version does not have pipewire :( |

## Using the script

   N.B. Always **copy/paste** commands from here **excluding** the surrounding ' ' to avoid typos!

    1. Install the 'git' package using your package manager.

    2. Go to a terminal emulator on you machine and type: 'cd' followed by the ENTER key to be sure you are in
    your home directory

    3. Type: 'git clone https://github.com/barjac/freedv-rade-build' then ENTER

    4. Type: 'chmod +x freedv-rade-build/freedv-rade-build' then ENTER

    5. Type: 'freedv-rade-build/freedv-rade-build' then ENTER

Now put the kettle on, it will take a while!

On completion you should see a message to that effect and instructions on how to launch the program.

If you are using hamlib don't forget to add yourself (as root) to the 'dialout' group:

\# usermod -aG dialout <your_user_name>

To make that active you will need to reboot the system.

## Testing new updates
There is now a 'freedv-rade-update' script which allows fast updating of your freedv-rade (created using freedv-rade-build), full rebuilds and backup/restore from a simple text menu.

**New** - added ability to optionally build 'Pull Request' branches using the branch name e.g. 'ms-rade-cport' etc.
          also tags and git# may be used.

N.B. Always **copy/paste** commands from here **excluding** the surrounding ' ' to avoid typos!

If you have a clone of this project then:

    1. 'cd ~/freedv-rade-build'

    2. 'git pull' to update it

    3. 'cd && chmod +x freedv-rade-build/freedv-rade-update' to make the update script executable

    4. 'freedv-rade-build/freedv-rade-update' to run it as often as you like.

Just follow the prompts. If you hit a problem please open an issue here.

***Have fun with FreeDV!***

