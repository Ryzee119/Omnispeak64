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

opl3_chip nuked_oplChip;

#define PC_PIT_RATE 1193182
#define SD_SFX_PART_RATE 140
/* In the original exe, upon setting a rate of 140Hz or 560Hz for some
 * interrupt handler, the value 1192030 divided by the desired rate is
 * calculated, to be programmed for timer 0 as a consequence.
 * For THIS value, it is rather 1193182 that should be divided by it, in order
 * to obtain a better approximation of the actual rate.
 */
#define SD_SOUND_PART_RATE_BASE 1192030

// Sort of replacements for x86 behaviors and assembly code

static bool SD_N64_AudioSubsystem_Up;

timer_link_t *t0_timer;
void SDL_t0Service(void);

static void _t0service(int ovfl)
{
	SDL_t0Service();
}

/* NEVER call this from the SDL callback!!! (Or you want a deadlock?) */
void SD_N64_SetTimer0(int16_t int_8_divisor)
{
    uint16_t ints_per_sec = SD_SOUND_PART_RATE_BASE / int_8_divisor;
    delete_timer(t0_timer);
    t0_timer = new_timer(TIMER_TICKS(1000000/ints_per_sec), TF_CONTINUOUS, _t0service);
    if (!t0_timer)
    {
        debugf("Error creating timer\n");
    }
}

int16_t *stream;
bool generate_stream = false;
int pc_spkr_freq = 0;
/* FIXME: The SDL prefix may conflict with SDL functions in the future(???)
 * Best (but hackish) solution, if it happens: Add our own custom prefix.
 */

static void audio_callback(short *buffer, size_t numsamples)
{
    memcpy(buffer, stream, numsamples);
}

void SD_N64_alOut(uint8_t reg, uint8_t val)
{
    //OPL3_WriteReg(&nuked_oplChip, reg, val);
    //if (audio_can_write() && generate_stream)
    //{
        //OPL3_GenerateStream(&nuked_oplChip, stream, audio_get_buffer_length() / 2);
        //generate_stream = false;
    //}
}

static void create_sine(int sample_rate, int freq, int num_samples, uint16_t *stream)
{
    for (int i = 0; i < num_samples; i += 2)
    {
        // amplitude = 0.8 * max range; max range = 0x8000 = 32768 ( max value for 16 Bit signed int )
        int sinus = .5 * 0x8000 * sin((2 * 3.1415f * freq) * i / sample_rate / 2);
        stream[i + 0] = CK_Cross_SwapLE16(sinus & 0xFFFF);
        stream[i + 1] = CK_Cross_SwapLE16(sinus & 0xFFFF);
    }
}

void SD_N64_PCSpkOn(bool on, int freq)
{
    //if (on)
    //    pc_spkr_freq = freq;
    //else
    //    pc_spkr_freq = 0;
    static int i = 0;
    if (!i)
        create_sine(9600, 1000, audio_get_buffer_length(), stream);
    
    if (i ==0)
        i = 1;
    if (i == 1)
    {
        for (int j = 0; j < 64; j++)
        {
            fprintf(stderr, "%d\n", stream[j]);
        }
        fprintf(stderr, "\r\n");
    }
    i = 2;
}

void SD_N64_Startup(void)
{
    SD_N64_AudioSubsystem_Up = true;
    //OPL3_Reset(&nuked_oplChip, 9600);
    audio_init(9600, 2);
    stream = malloc(audio_get_buffer_length() * 2);
    memset(stream, 0x00, audio_get_buffer_length() * 2);
    audio_write_silence();
    audio_write_silence();
    audio_set_buffer_callback(&audio_callback);
    init_interrupts();
    timer_init();
    t0_timer = NULL;
}

void SD_N64_Shutdown(void)
{
    if (SD_N64_AudioSubsystem_Up)
    {
        SD_N64_AudioSubsystem_Up = false;
    }
}

bool SD_N64_IsLocked = false;

void SD_N64_Lock()
{
    if (SD_N64_IsLocked)
        CK_Cross_LogMessage(CK_LOG_MSG_ERROR, "Tried to lock the audio system when it was already locked!\n");
    SD_N64_IsLocked = true;
}

void SD_N64_Unlock()
{
    if (!SD_N64_IsLocked)
        CK_Cross_LogMessage(CK_LOG_MSG_ERROR, "Tried to unlock the audio system when it was already unlocked!\n");
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
