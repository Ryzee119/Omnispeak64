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
#include "opl/dbopl.h"

#include "id_sd.h"
#include "id_ca.h"
#include "ck_cross.h"

#define ADLIB_NUM_CHANNELS 1
#define ADLIB_BYTES_PER_SAMPLE 2
#define ADLIB_SAMPLE_RATE 19200
#define ADLIB_MIXER_CHANNEL 0

extern bool sd_musicStarted;
extern volatile int sd_al_currentSfxLength;
static waveform_t music;
static Chip oplChip;

static const int PC_PIT_RATE = 1193182;
static const int AUDIO_BITRATE = 19200;

static bool SD_N64_IsLocked = false;
static bool SD_N64_AudioSubsystem_Up = false;

//Timing backend for the gamelogic which uses the sound system
static timer_link_t *t0_timer;
void SDL_t0Service(void);

static inline void _do_audio_update()
{
    if (audio_can_write())
    {
        short *buf = audio_write_begin();
        mixer_poll(buf, audio_get_buffer_length());
        audio_write_end();
    }
}

static void music_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    (void)ctx;
    int16_t *dst = CachedAddr(samplebuffer_append(sbuf, wlen));
    if (sd_musicStarted || sd_al_currentSfxLength)
    {
        int32_t _data[wlen];
        Chip__GenerateBlock2(&oplChip, wlen, _data);
        for (int i = 0; i < wlen; i++)
        {
            dst[i] = (int16_t)(_data[i]);
        }
    }
    else
    {
        memset(dst, 0, wlen * ADLIB_NUM_CHANNELS * ADLIB_BYTES_PER_SAMPLE);
    }
    data_cache_hit_writeback_invalidate(dst, wlen * ADLIB_NUM_CHANNELS * ADLIB_BYTES_PER_SAMPLE);
}

//Audio interrupts
static void _t0service(int ovfl)
{
    SDL_t0Service();
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
    Chip__WriteReg(&oplChip, reg, val);
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

    audio_init(AUDIO_BITRATE, 2);
    mixer_init(16);

    //Init adlib engine for music
    DBOPL_InitTables();
    Chip__Chip(&oplChip);
    Chip__Setup(&oplChip, ADLIB_SAMPLE_RATE);

    music.bits = ADLIB_BYTES_PER_SAMPLE * 8;
    music.channels = ADLIB_NUM_CHANNELS;
    music.frequency = ADLIB_SAMPLE_RATE;
    music.len = WAVEFORM_UNKNOWN_LEN;
    music.read = music_read;
    music.loop_len = 0;
    music.ctx = (void *)&music;
    mixer_ch_play(ADLIB_MIXER_CHANNEL, &music);
    SD_N64_AudioSubsystem_Up = true;
}

static void SD_N64_Shutdown(void)
{
    if (SD_N64_AudioSubsystem_Up == false)
    {
        return;
    }
    audio_close();
    SD_N64_AudioSubsystem_Up = false;
}

static void SD_N64_Lock()
{
    SD_N64_IsLocked = true;
}

static void SD_N64_Unlock()
{
    SD_N64_IsLocked = false;
}

void SD_N64_WaitTick()
{
    _do_audio_update();
}

static SD_Backend sd_n64_backend = {
    .startup = SD_N64_Startup,
    .shutdown = SD_N64_Shutdown,
    .lock = SD_N64_Lock,
    .unlock = SD_N64_Unlock,
    .alOut = SD_N64_alOut,
    .pcSpkOn = SD_N64_PCSpkOn,
    .setTimer0 = SD_N64_SetTimer0,
    .waitTick = SD_N64_WaitTick
};

SD_Backend *SD_Impl_GetBackend()
{
    return &sd_n64_backend;
}
