# petdisk-max
Firmware and schematics for the PETdisk MAX storage device for Commodore PET

## Installation

## Configuration

The default configuration of the PETdisk MAX uses the built-in sd card as device 8.
You can change this device number as well as setting up a maximum of 4 network drives by editing this file:

Copy the edited file onto the root directory of your SD card. On the next power up, the new configuration will be read.


Subdirectories

To change directories on a FAT32 volume, use
LOAD"$:dirname",devicenumber

This also loads the directory listing of the new directory.

D64
The PETdisk MAX currently supports D64 files in read-only mode.
To mount a D64 file stored on either an SD card or network drive,
use the following command:
LOAD"$:filename.d64",devicenumber

This will mount the D64 file as the disk for the specified device number. It will also load the directory of the disk image which can be viewed with LIST.

To unmount the D64 file and return to the original device, use
LOAD"$:..",devicenumber

Things known to currently not work (but working on it):
Support for BASIC 1 ROM (i.e. non-upgraded original PET 2001)
DIRECTORY command
REL files
U2 (block write) drive command

Please let me know if you discover any bugs or have feature requests.
bitfixer at (removeforantispam) bitfixer dot com



