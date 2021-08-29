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
#include "opl/nuked_opl3.h"

#include "id_sd.h"
#include "ck_cross.h"


static const int PC_PIT_RATE = 1193182;
static const int SD_SFX_PART_RATE = 140;
static const int SD_SOUND_PART_RATE_BASE = 1192030;
static const int BITRATE = 9600;
uint16_t ints_per_sec;

static int16_t *stream = NULL;
static bool SD_N64_IsLocked = false;

//Timing backend for the gamelogic which uses the sound system
static timer_link_t *t0_timer;
void SDL_t0Service(void);

enum backend
{
    NONE,
    ADLIB,
    SPEAKER
};
enum backend backend_t = NONE;

//OPL (for Adlib backend)
static opl3_chip nuked_oplChip;
static bool SD_N64_AudioSubsystem_Up = false;
static bool generate_stream = false;

//Beep Driver (For PC Speaker backend)
static bool beep_on;
static int16_t beep_current_sample;
static uint32_t beep_half_cycle_cnt, beep_half_cycle_cnt_max;
static uint32_t beep_samples_per_service;
static int beep_cursor = 0;

//Audio interrupts
static void _t0service(int ovfl)
{
    SDL_t0Service();

    if (backend_t == ADLIB)
    {
        //int chunk = audio_get_buffer_length() * ints_per_sec / BITRATE;
        //OPL3_GenerateStream(&nuked_oplChip, &stream[beep_cursor], chunk);
        //beep_cursor = (beep_cursor + (chunk * 2)) % (audio_get_buffer_length() * 2);
        return;
    }
    //Each service a number of audio samples is generated (beep_samples_per_service)
    //Put this in the audio stream
    for (int i = 0; i < (beep_samples_per_service * 2); i += 2)
    {
        if (backend_t != SPEAKER)
        {
            break;
        }
        if (!beep_on)
        {
            stream[beep_cursor + 0] = 0;
            stream[beep_cursor + 1] = 0;
        }
        else
        {
            stream[beep_cursor + 0] = beep_current_sample;
            stream[beep_cursor + 1] = beep_current_sample;
            beep_half_cycle_cnt += 2 * PC_PIT_RATE;
            if (beep_half_cycle_cnt >= beep_half_cycle_cnt_max)
            {
                beep_half_cycle_cnt %= beep_half_cycle_cnt_max;
                beep_current_sample = 0x5FFF - beep_current_sample;
            }
        }
        beep_cursor = (beep_cursor + 2) % (audio_get_buffer_length() * 2);
    }
}

static void _audio_callback(short *buffer, size_t numsamples)
{
    for (int i = 0; i < (numsamples * 2); i += 2)
    {
        int j = (beep_cursor + i) % (numsamples * 2);
        buffer[i] = stream[j];
        buffer[i + 1] = stream[j + 1];
    }
}

void SD_N64_SetTimer0(int16_t int_8_divisor)
{
    //Create an interrupt that occurs at a certain frequency.
    ints_per_sec = SD_SOUND_PART_RATE_BASE / int_8_divisor;
    delete_timer(t0_timer);
    t0_timer = new_timer(TIMER_TICKS(1000000 / ints_per_sec), TF_CONTINUOUS, _t0service);
    beep_samples_per_service = int_8_divisor * BITRATE / PC_PIT_RATE;
}

void SD_N64_alOut(uint8_t reg, uint8_t val)
{
    backend_t = ADLIB;
    OPL3_WriteReg(&nuked_oplChip, reg, val);
}

void SD_N64_PCSpkOn(bool on, int freq)
{
    backend_t = SPEAKER;
    beep_on = on;
	beep_current_sample = 0;
	beep_half_cycle_cnt = 0;
	beep_half_cycle_cnt_max = BITRATE * freq;
}

void SD_N64_Startup(void)
{
    if (SD_N64_AudioSubsystem_Up)
    {
        return;
    }
    audio_init(BITRATE, 2);
    OPL3_Reset(&nuked_oplChip, BITRATE);
    stream = malloc(2 * sizeof(short) * audio_get_buffer_length());
    memset(stream, 0x00, 2 * sizeof(short) * audio_get_buffer_length());
    audio_write_silence();
    audio_write_silence();
    audio_set_buffer_callback(&_audio_callback);
    init_interrupts();
    timer_init();
    t0_timer = NULL;
    SD_N64_AudioSubsystem_Up = true;
}

void SD_N64_Shutdown(void)
{
    if (SD_N64_AudioSubsystem_Up)
    {
        free(stream);
        audio_close();
        SD_N64_AudioSubsystem_Up = false;
    }
}

void SD_N64_Lock()
{
    if (SD_N64_IsLocked)
    {
        CK_Cross_LogMessage(CK_LOG_MSG_ERROR, "Tried to lock the audio system when it was already locked!\n");
        return;
    }
    audio_pause(true);
    SD_N64_IsLocked = true;
}

void SD_N64_Unlock()
{
    if (!SD_N64_IsLocked)
    {
        CK_Cross_LogMessage(CK_LOG_MSG_ERROR, "Tried to unlock the audio system when it was already unlocked!\n");
        return;
    }
    audio_pause(false);
    SD_N64_IsLocked = false;
}

SD_Backend sd_n64_backend = {
    .startup = SD_N64_Startup,
    .shutdown = SD_N64_Shutdown,
    .lock = SD_N64_Lock,
    .unlock = SD_N64_Unlock,
    .alOut = SD_N64_alOut,
    .pcSpkOn = SD_N64_PCSpkOn,
    .setTimer0 = SD_N64_SetTimer0};

SD_Backend *SD_Impl_GetBackend()
{
    return &sd_n64_backend;
}
