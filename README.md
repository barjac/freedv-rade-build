
This script is intended to automate the install of the new RADE development versions of freedv-gui-2.0-dev in modern Linux distibutions.

It initially checks which Linux distribution is in use, that disk space is adequate, whether internet with working DNS is available and that the script is being run as a regular user.

Then it uses the distros native package manager to install some essential system packages for which it requests the root password.

The remaining script then continues as the regular user.

A working directory called freedv-rade is created in the user's home directory which is used to hold the complete installation.

The freedv-gui sources are cloned from github and (at present) the v2.0-dev branch is checked out.

A python3 virtual environment is then automatically created and activated as described in the freedv-gui README.md

Several essential python modules are then installed by python pip in the virtual environment.

build_linux.sh is then run to continue with the download and building of more dependencies and ultimately freedv.

On completion of the build a start script is written to the user's home folder called freedv-start which is then made executable ready for use.
'./freedv-start' or optionally './freedv-start -f yourfreedv.conf' should start freedv v2.0-dev.

The start script includes code to start and stop hamlib rigctld but this needs editing and enabling in the script to suit user's radio etc.

It is currently tested in:

   Mageia 9                        OK

   Mageia 10 (-dev)                OK

   Linux Mint 22                   OK

   Fedora 41                       OK

   Ubuntu 24.04 LTS                OK



