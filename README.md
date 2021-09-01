
# Omnispeak64
A port of Commander Keen "Commander Keen in Goodbye Galaxy!" to the Nintendo 64.

## Controls
* Jump - A
* Fire - B or Z
* Pogo - R
* Main Menu - Start
* Status Menu - C-Buttons
* Movement - Dpad or Analog stick 

## To-Do
- [ ] OPL Adlib sound engine (Currently the performance is too bad to use). (PC Speaker driver is default).
- [ ] Menu to select which Keen episode to load or likely separate ROMS for each episode.
- [ ] Still lots of CPU rendering (Blitting, blending, fills). Convert this to the RDP/RSP.

## Download
You can download a precompiled binary from the [Release section](https://github.com/Ryzee119/Omnispeak64/releases). This include the shareware version of the first episode.
The other 2 episodes are supported, however need files from the original game. Ref https://github.com/sulix/omnispeak. Copy these to the `filesystem` folder and recompile.
A menu to select the epsiode is in the todo list.

## Build
This was developed using the opensource N64 toolchain [libdragon](https://github.com/DragonMinded/libdragon). I developed it using the official docker container. The build process is something like this:
```
apt-get install npm docker.io
git clone --recursive https://github.com/Ryzee119/Omnispeak64.git
cd Omnispeak64
npm install -g libdragon
libdragon download
libdragon start
libdragon make
```
This should produce a `.z64` rom file.

## Credits
* [libdragon](https://github.com/DragonMinded/libdragon) : Licensed under the ["The Unlicense"](https://github.com/DragonMinded/libdragon/blob/trunk/LICENSE.md)
* [omnispeak](https://github.com/sulix/omnispeak) : Licensed under [GNU General Public License v2.0](https://github.com/sulix/omnispeak/blob/master/LICENSE)
* [Raskys mvs64 port](https://github.com/rasky/mvs64) : His RDP rendering work was extremely helpful.
* [3D Realms](https://3drealms.com/catalog/commander-keen-goodbye-galaxy_8/). Buy the game to support the developers. [GOG](https://www.gog.com/game/commander_keen_complete_pack), [Steam](https://store.steampowered.com/app/9180/Commander_Keen/).
