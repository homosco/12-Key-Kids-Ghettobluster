# 12 Key Kids-GhettoBluster

Arduino MP3 player for Kids. (Inspired by Hörbert which can only play WAV and 9 albums and has no display)

## Buttons:
- Track Fwd
- Track Bwd
- 9 Buttons to choose folders. 
- Play/Pause

## Hardware
 - Arduino Uno R3
 - Adafruit Music Maker Shield
 - Adafruit PowerBoost 500 Charger - Rechargeable 5V Lipo USB Boost @ 500mA+
 - LiPo Akku  (3.7 V, 2 mm JST)
 - 12 Button Keypad
 - Micro SD card
 - 2 4 Ohm Speaker

## Code
Used Code from: 
- https://github.com/golesny/12mp3/tree/master/mp3player
- MORE TO BE ADDED
- 

## Links and References
 - https://learn.adafruit.com/adafruit-music-maker-shield-vs1053-mp3-wav-wave-ogg-vorbis-player/library-reference
 - https://learn.adafruit.com/adafruit-1-44-color-tft-with-micro-sd-socket/wiring-and-test
 - https://www.arduino.cc/en/Reference/SD
 - https://github.com/adafruit/Adafruit_VS1053_Library/blob/master/Adafruit_VS1053.h
 - https://www.hoerbert.com/
 
## How to add files to sd Card correctly: 

### Todo Für Bespielen: 

1. Alle Ordner und Dateien aufspielen.  Dann umbenennen: nummeriert je 8 Zeichen (bei Dateien 8 Zeichen plus “.mp3”). Keine Leerzeichen, keine Umlaute! 
2. Dann fatsort aus Terminal drüber laufen lassen: 

   - Mount the USB drive or SD card via normal means.
   - Open Terminal. 
   - Run the mount command to find out which device its mounted on.  The device shows up in the first column of the mount output.  In the example below the USB drive is called HARMONY mounted on directory “/Volumes/HARMONY”, and the device is “/dev/disk2s1”:

> macmini:~ me$ mount
> /dev/disk0s2 on / (hfs, local, journaled)
> devfs on /dev (devfs, local, nobrowse)
> map -hosts on /net (autofs, nosuid, automounted, nobrowse)
> map auto_home on /home (autofs, automounted, nobrowse)
> /dev/disk2s1 on /Volumes/HARMONY (msdos, local, nodev, nosuid, read-only, noowners)

    - You now need root / administrative authority, so su to the root account.  You will be prompted to enter your password, not the root user password.

> macmini:~ me$ sudo su -

    - Un-mount the SD-CARD from Finder using the unmount command:

> macmini:~ root# diskutil unmount /Volumes/<YOUR-SD-CARD>/  

    - Run the fatsort program from the location you copied it to.  It has several options which can be obtained by running it with the “-h” parameter.  The default is to sort directories first, then files, which is what I want so no options are needed. You must specify the device you identified above (in this example “/dev/disk2s1”).
    fatsort Website: https://fatsort.sourceforge.io/

>   macmini:~ root# /Users/me/fatsort /dev/disk2s1   

That’s it!  

3. Eject with Clean Eject (App) or just remove without mounting again.


