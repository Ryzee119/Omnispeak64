
# Omnispeak64
A port of Commander Keen "Commander Keen in Goodbye Galaxy!" to the Nintendo 64.

## Controls
* Jump - A
* Fire - Z or R
* Pogo - B
* Main Menu - Start
* Status Menu - C-Buttons
* Movement - Dpad or Analog stick 

## To-Do
- [ ] Still lots of CPU rendering (Blitting, blending, fills). Convert this to the RDP/RSP.

## Warnings
- Currently, this relies on SRAM96K support for game saves. Make sure your flashcart(or emulator is setup to use SRAM96kByte (or SRAM768kbit) save types.

## Download
You can download a precompiled binary from the [Release section](https://github.com/Ryzee119/Omnispeak64/releases). This include the shareware version of the first episode.
The other 2 episodes are supported, however need files from the original game:
* Copy Keen 5 files `AUDIO.CK5`, `EGAGRAPH.CK5` and `GAMEMAPS.CK6` into `filesystem/CK5` and recompile to play episode 5.
* Copy Keen 6 V1.5 files `AUDIO.CK6`, `EGAGRAPH.CK6` and `GAMEMAPS.CK6` into `filesystem/CK6` and recompile to play episode 6.

## Build
This was developed using the opensource N64 toolchain [libdragon](https://github.com/DragonMinded/libdragon). I developed it using the official docker container. The build process is something like this (after setting up the docker image as per the instructions):
```
git clone --recursive https://github.com/Ryzee119/Omnispeak64.git
cd Omnispeak64
libdragon init

#Build Episode 4 (The shareware game files are already in the filesystem/CK4 folder)
libdragon make EP=4

#Build Episode 5 (Make sure the game files are in the filesystem/CK5 folder)
libdragon make EP=5

#Build Episode 6 (Make sure the game files are in the filesystem/CK6 folder)
libdragon make EP=6
```
This should produce a `omnispeak_epX.z64` rom file.

<img src="https://i.imgur.com/ZqfeGym.png" alt="basic" width="100%"/>  

## Credits
* [libdragon](https://github.com/DragonMinded/libdragon) : Licensed under the ["The Unlicense"](https://github.com/DragonMinded/libdragon/blob/trunk/LICENSE.md)
* [omnispeak](https://github.com/sulix/omnispeak) : Licensed under [GNU General Public License v2.0](https://github.com/sulix/omnispeak/blob/master/LICENSE)
* [Raskys mvs64 port](https://github.com/rasky/mvs64) : His RDP rendering work was extremely helpful.
* [3D Realms](https://3drealms.com/catalog/commander-keen-goodbye-galaxy_8/). Buy the game to support the developers. [GOG](https://www.gog.com/game/commander_keen_complete_pack), [Steam](https://store.steampowered.com/app/9180/Commander_Keen/).
