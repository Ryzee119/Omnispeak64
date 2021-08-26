/*
Omnispeak: A Commander Keen Reimplementation
Copyright (C) 2018 Omnispeak Authors

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

#define PC_PIT_RATE 1193182
#define SD_SFX_PART_RATE 140
#define SD_SOUND_PART_RATE_BASE 1192030

static int16_t *stream = NULL;
static const int BITRATE = 9600;
static bool SD_N64_IsLocked = false;

//Timing backend for the gamelogic uses the sound system
static timer_link_t *t0_timer;
void SDL_t0Service(void);

//OPL (for Adlib backend)
static opl3_chip nuked_oplChip;
static bool SD_N64_AudioSubsystem_Up = false;
static bool generate_stream = false;

//Audio interrupts
static void _t0service(int ovfl)
{
    SDL_t0Service();
}

static void _audio_callback(short *buffer, size_t numsamples)
{
    memcpy(buffer, stream, numsamples * 2);
}

void SD_N64_SetTimer0(int16_t int_8_divisor)
{
    //Create an interrupt that occurs at a certain frequency.
    uint16_t ints_per_sec = SD_SOUND_PART_RATE_BASE / int_8_divisor;
    delete_timer(t0_timer);
    t0_timer = new_timer(TIMER_TICKS(1000000/ints_per_sec), TF_CONTINUOUS, _t0service);
}

void SD_N64_alOut(uint8_t reg, uint8_t val)
{
    OPL3_WriteReg(&nuked_oplChip, reg, val);
    if (audio_can_write() && generate_stream)
    {
        OPL3_GenerateStream(&nuked_oplChip, stream, audio_get_buffer_length() / 2);
        generate_stream = false;
    }
}

void SD_N64_PCSpkOn(bool on, int freq)
{
    //Fill the stream with a 16 bit interlaced stereo PCM waveform at the required frequency
    for (int i = 0; i < (audio_get_buffer_length * 2); i += 2)
    {
        int sinus = 0.8 * 0x8000 * sin((2 * 3.1415f * freq) * i / BITRATE / 2);
        stream[i + 0] = CK_Cross_SwapLE16(sinus & 0xFFFF);
        stream[i + 1] = CK_Cross_SwapLE16(sinus & 0xFFFF);
    }
}

void SD_N64_Startup(void)
{
    if (SD_N64_AudioSubsystem_Up)
    {
        return;     
    }
    audio_init(BITRATE, 2);
    OPL3_Reset(&nuked_oplChip, BITRATE);
    stream = malloc(2  * sizeof(short) * audio_get_buffer_length());
    memset(stream, 0x00, audio_get_buffer_length() * 2);
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
