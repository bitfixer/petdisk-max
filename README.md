# petdisk-max
Firmware and schematics for the PETdisk MAX storage device for Commodore PET

Configuration

The default configuration of the PETdisk MAX uses the built-in sd card as device 8. 

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

