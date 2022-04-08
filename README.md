# petdisk-max
Firmware and schematics for the PETdisk MAX storage device for Commodore PET

The PETdisk MAX is a storage device for the Commodore PET series of computers. It can use both a microSD card and can also access up to 4 network drives.
The network drives are essentially directories of files hosted on standard web servers. They can be configured as read/write or readonly. Currently PHP support is required on a web server to be used as a network drive - simply copy the [petdisk.php](https://raw.githubusercontent.com/bitfixer/petdisk-max/main/www/petdisk.php) file into a directory on a web server along with some PET files, and you're ready to go.

## Installation

The PETdisk MAX plugs into the IEEE-488 port on the back of your PET. Looking at the back of the PET, this is the edge connector on the right side.
Since the IEEE-488 port does not supply power, a separate power connection is required.
Depending on when you received your PETdisk MAX, this will either be a small board which fits into one of the PET's ROM sockets and provides a 5v power pin, or a clip which connects to the PETdisk with a wire lead.

First, open up the PET.

### For the ROM socket: 
If you have any ROM sockets free, choose the one closest to the PETdisk and install the board into the socket on the mainboard, making sure to align the notches on both sockets. 
If no ROM sockets are free, first remove a ROM chip from the mainboard, install the socket, and then reinstall the ROM chip into the new socket. Make sure to line the notches up.
Then connect the wire lead included with the PETdisk to the 5v pin, and feed the other end out the back of the PET through the opening around the IEEE-488 connector.

### For the clip: 
You'll need to clip onto a 5v source on the mainboard. An easy place to find this is on one of the 6520 chips, there are 2 of them in the PET. They are located close on the board to the IEEE-488 port. Locate one of the 6520 chips, they will be close to the back of the board and labeled '6520' somewhere on the chip. Not to be confused with the '6502' also on the board.

Locations of 6520s in some PET motherboards:
2001: UB8 and UG8
2001N: UC6 and UC7
8032: UB16 and UB12

Pick the more conveniently located 6520, and find pin 20. If you are looking at the chip with the notch up, go to the left side of the chip and go down to the lower left corner. Clip on to this pin and then feed the wire lead out the back through the opening around the IEEE-488 connector.
If you prefer, you can clip on to any capacitor lead connected to 5v in the computer, there are several. 

Moving to the back of the PET, connect the wire lead to the PETdisk MAX. There is a connector on the back of the PETdisk, and the position on the connector furthest to the right side of the device is labeled '5v'. Connect here, double check that you are connected in the furthest right position, closest to the edge of the board.

After this, just plug the PETdisk MAX into the IEEE-488 connector and you're ready to go.

## Configuration

The default configuration of the PETdisk MAX uses the built-in sd card as device 8.
You can change this device number as well as setting up a maximum of 4 network drives by editing this file template:
[PETDISK.CFG](https://raw.githubusercontent.com/bitfixer/petdisk-max/main/PETDISK.CFG)
Each line of the file consists of the drive number, comma, text specifying the drive.
SD0 refers to the built-in SD card. Change the drive number here if you like.
For network drives, the specifier is the full URL of the drive script. Up to 4 can be used at once.
Remove any lines with URLs if you don't want to use any network drives at that time.

The last 2 lines of the file contain the ssid and password for your wifi network, replace the placeholders with the information for your network. Note that only 2.4 Ghz wifi networks are supported.

Copy the edited file onto the root directory of your SD card. On the next power up, the new configuration will be read.

## Operation

Basic drive operations are the same as other Commodore storage devices.
Load a directory with
LOAD"$",devicenumber
and 
LIST

For BASIC 4, the CATALOG and DIRECTORY commands will also work.

Load a program (.PRG) with
LOAD"PROGRAM",devicenumber

or save with
SAVE"PROGRAM",devicenumber

The DLOAD command also works.

## Subdirectories

To change directories on a FAT32 volume, use
LOAD"$:dirname",devicenumber

This also loads the directory listing of the new directory.

Return to the previous directory with
LOAD"$:..",devicenumber

## D64

The PETdisk MAX currently supports D64 files for both reading and writing.
To mount a D64 file stored on either an SD card or network drive,
use the following command:
LOAD"$:filename.d64",devicenumber

This will mount the D64 file as the disk for the specified device number. It will also load the directory of the disk image which can be viewed with LIST.

To unmount the D64 file and return to the original device, use
LOAD"$:..",devicenumber

## Updating the PETdisk MAX

Firmware updates will come periodically with fixes and new features. The latest firmware for both the v1 and v2 PETdisk MAX models will be located here:\
PETdisk MAX v1 (Atmel ATmega1284)
[FIRMWARE.BIN](https://github.com/bitfixer/petdisk-max/raw/main/firmware/FIRMWARE.BIN)

PETdisk MAX v2 (ESP32):
[FIRMWARE.PD2](https://github.com/bitfixer/petdisk-max/raw/main/firmware/FIRMWARE.PD2)

To update the firmware on the device, copy this file onto the root directory of an empty microSD card formatted to FAT32. Insert the card into the PETdisk and power up. After a few seconds the new firmware will be loaded onto the device.
You can verify that the update worked by loading a directory from the SD card. 
At the end of the directory listing the git commit id will be listed, so you can verify it changed from before the update. This also provides a firmware ID which you can use if you need to report any bugs.

## Coming Soon

Things known to currently not work (but working on it):
1. Support for BASIC 1 ROM (i.e. non-upgraded original PET 2001)
2. REL files
3. U2 (block write) drive command

Please let me know if you discover any bugs or have feature requests.
bitfixer at (removeforantispam) bitfixer dot com



