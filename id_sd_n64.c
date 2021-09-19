/*
Omnispeak: A Commander Keen Reimplementation
Copyright (C) 2018 Omnispeak Authors
Copyright (C) 2021 Ryan Wendland (Libdragon N64 port)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libdragon.h>
#include <mikmod.h>

#include "id_sd.h"
#include "id_ca.h"
#include "ck_cross.h"

static const int PC_PIT_RATE = 1193182;
static const int SD_SOUND_PART_RATE_BASE = 1192030;
static const int BITRATE = 9600;

MIKMODAPI extern UWORD md_mode __attribute__((section (".data")));
MIKMODAPI extern UWORD md_mixfreq __attribute__((section (".data")));
extern ca_audinfo ca_audInfoE;
static MODULE *bgm = NULL;
static SAMPLE **sfx_samples = NULL;
static bool SD_N64_IsLocked = false;
static bool SD_N64_AudioSubsystem_Up = false;

//Timing backend for the gamelogic which uses the sound system
static timer_link_t *t0_timer;
void SDL_t0Service(void);

//Audio interrupts
static void _t0service(int ovfl)
{
    SDL_t0Service();
    if (audio_can_write())
    {
        MikMod_Update();
    }
    //Loop background music
    if (!Player_Active() && bgm)
    {
        Player_SetPosition(0);
    }
}

static void SD_N64_SetTimer0(int16_t int_8_divisor)
{
    //Create an interrupt that occurs at a certain frequency.
    uint16_t ints_per_sec = PC_PIT_RATE / int_8_divisor;
    delete_timer(t0_timer);
    t0_timer = new_timer(TIMER_TICKS(1000000 / ints_per_sec), TF_CONTINUOUS, _t0service);
}

static void SD_N64_alOut(uint8_t reg, uint8_t val)
{
}

static void SD_N64_PCSpkOn(bool on, int freq)
{
}

static void SD_N64_Startup(void)
{
    if (SD_N64_AudioSubsystem_Up == true)
    {
        return;
    }

    audio_init(BITRATE, 2);
    init_interrupts();
    timer_init();
    t0_timer = NULL;

    MikMod_RegisterAllDrivers();
    MikMod_RegisterAllLoaders();
    md_mode |= DMODE_16BITS;
    md_mode |= DMODE_SOFT_MUSIC;
    md_mode |= DMODE_SOFT_SNDFX;
    md_mixfreq = audio_get_frequency();
    MikMod_Init("");
    MikMod_SetNumVoices(-1, 5);
    MikMod_EnableOutput();

    sfx_samples = (SAMPLE **)malloc(ca_audInfoE.numSounds * sizeof(SAMPLE *));
    memset(sfx_samples, 0, ca_audInfoE.numSounds * sizeof(SAMPLE *));

    SD_N64_AudioSubsystem_Up = true;
}

static void SD_N64_Shutdown(void)
{
    if (SD_N64_AudioSubsystem_Up == false)
    {
        return;
    }

    MikMod_DisableOutput();

    for (int i = 0; i < ca_audInfoE.numSounds; i++)
    {
        if (sfx_samples[i])
        {
            Sample_Free(sfx_samples[i]);
        }
    }
    free(sfx_samples);

    if (bgm)
    {
        Player_Stop();
        Player_Free(bgm);
    }

    audio_close();
    MikMod_Exit();
    SD_N64_AudioSubsystem_Up = false;
}

static void SD_N64_Lock()
{
    if (SD_N64_IsLocked)
    {
        return;
    }
    MikMod_Lock();
    SD_N64_IsLocked = true;
}

static void SD_N64_Unlock()
{
    if (!SD_N64_IsLocked)
    {
        return;
    }
    MikMod_Unlock();
    SD_N64_IsLocked = false;
}

void N64_LoadSound(int16_t sound)
{
    //SFX file names are 0XX.wav
    sound -= ca_audInfoE.startAdlibSounds;
    char fn[32];
    sprintf(fn, "rom://%03d.wav", sound);
    assert(sound <= ca_audInfoE.numSounds);
    if (!sfx_samples[sound])
    {
        sfx_samples[sound] = Sample_Load(fn);
    }
}

void N64_PlayMusic(int16_t song)
{
    //Music file names are m00X.wav
    char fn[32];
    sprintf(fn, "rom://m%03d.mod", song);
    if (bgm)
    {
        Player_Stop();
        Player_Free(bgm);
    }
    bgm = Player_Load(fn, 127, 0);
    if (bgm)
    {
        Player_Start(bgm);
        Player_SetVolume(32);
    }
}

void N64_PlaySound(soundnames sound)
{
    if (sfx_samples[sound])
    {
        Sample_Play(sfx_samples[sound], 0, 0);
    }
}

static SD_Backend sd_n64_backend = {
    .startup = SD_N64_Startup,
    .shutdown = SD_N64_Shutdown,
    .lock = SD_N64_Lock,
    .unlock = SD_N64_Unlock,
    .alOut = SD_N64_alOut,
    .pcSpkOn = SD_N64_PCSpkOn,
    .setTimer0 = SD_N64_SetTimer0
};

SD_Backend *SD_Impl_GetBackend()
{
    return &sd_n64_backend;
}
