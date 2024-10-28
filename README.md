This script is intended to simplify install of the new RADE development versions of freedv-gui in modern Linux distibutions.

It initially checks which Linux distribution is in use, that disk space is adequate, whether internet with working DNS is available and that the script is being run as a regular user.

It then uses the distros native package manager to install some essential packages for which it requests the root password.

The remaining script then continues as the regular user.

A working directory called freedv-rade is created in the user's home directory which is used to hold the complete installation.

The freedv-gui sources are cloned from github and (at present) the v2.0-dev branch is checked out.

A python3 virtual environment is then created and activated as described in the freedv-gui README.md

Several essential python modules are then installed by python pip in the virtual environment.

build_linux.sh is then run to continue with the download and building of more dependencies and ultimately freedv.

On completion of the build a start script is written to the user's home folder called freedv-start which is then made executable ready for use.
'./freedv-start' or optionally './freedv-start -f yourfreedv.conf' should start freedv v2.0-dev.

It is currently tested in Mageia 9 and Cauldron (Mageia 10-dev) and should be OK in Fedora but Ubuntu is not tested at all yet.

Please test in other distros and report the errors encountered.
