/*
Omnispeak: A Commander Keen Reimplementation
Copyright (C) 2020 Omnispeak Authors

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

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <libdragon.h>
#include <regsinternal.h>

#include "ck_cross.h"
#include "ck_ep.h"

#include "id_fs.h"
#include "id_mm.h"
#include "id_us.h"

#define IN_SRAM 0x8000
#define SRAM_NUMFILES 2
typedef struct sram_files_t
{
    const char *name;
    uint32_t size;
    uint32_t offset;
} sram_files_t;

#ifdef EP4
static sram_files_t sram_files[SRAM_NUMFILES + 1] = {
    {"STUB", 0, 0}, //So we dont get a 0 handle.
    {"CONFIG.CK4", 2048, 0},
    {"SAVEGAM0.CK4", 98304 - 2048, 0},
};
#elif EP5
static sram_files_t sram_files[SRAM_NUMFILES + 1] = {
    {"STUB", 0, 0}, //So we dont get a 0 handle.
    {"CONFIG.CK5", 2048, 0},
    {"SAVEGAM0.CK5", 98304 - 2048, 0},
};
#elif EP6
static sram_files_t sram_files[SRAM_NUMFILES + 1] = {
    {"STUB", 0, 0}, //So we dont get a 0 handle.
    {"CONFIG.CK6", 2048, 0},
    {"SAVEGAM0.CK6", 98304 - 2048, 0},
};
#endif

static int sram_get_handle_by_name(const char *name)
{
    FS_File handle = 0;
    int i;
    for (i = 0; i <= (SRAM_NUMFILES); i++)
    {
        if (strcasecmp(sram_files[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

static int sram_get_file_start_by_name(const char *name)
{
    int offset = 0;
    int i;
    for (i = 0; i < (SRAM_NUMFILES + 1); i++)
    {
        if (strcasecmp(sram_files[i].name, name) == 0)
            break;
        offset += sram_files[i].size;
    }
    if (i > SRAM_NUMFILES)
        return -1;

    return offset;
}

static int sram_get_file_start_by_handle(FS_File handle)
{
    handle &= ~IN_SRAM;
    assert(handle <= SRAM_NUMFILES);

    int offset = 0;
    for (int i = 0; i < handle; i++)
    {
        offset += sram_files[i].size;
    }
    return offset;
}

uint8_t __attribute__((aligned(16))) sector_cache[16];
static volatile struct PI_regs_s * const PI_regs = (struct PI_regs_s *)0xa4600000;
static void _dma_read(void * ram_address, unsigned long pi_address, unsigned long len) 
{
    assert(len > 0);
    disable_interrupts();
    while (dma_busy()) ;
    MEMORY_BARRIER();
    PI_regs->ram_address = ram_address;
    MEMORY_BARRIER();
    PI_regs->pi_address = pi_address;
    MEMORY_BARRIER();
    PI_regs->write_length = len-1;
    MEMORY_BARRIER();
    while (dma_busy()) ;
    enable_interrupts();
}

static void _dma_write(void * ram_address, unsigned long pi_address, unsigned long len) 
{
    assert(len > 0);
    disable_interrupts();
    while (dma_busy()) ;
    MEMORY_BARRIER();
    PI_regs->ram_address = (void*)ram_address;
    MEMORY_BARRIER();
    PI_regs->pi_address = pi_address;
    MEMORY_BARRIER();
    PI_regs->read_length = len-1;
    MEMORY_BARRIER();
    while (dma_busy());
    enable_interrupts();
}

//Reading and writing to SRAM via DMA must occur from a 16 bytes aligned buffer. For Omnispeak, the save file
//can be greater than 1 SRAM bank (32kB), so I need to be able to manage access multiple banks contiguously from non aligned buffers at random lengths.
//I read and write by 16 byte 'sectors' and manage the banking automatically. If a read crosses to a different bank this is handled here.
static int read_sram(uint8_t *dst, uint32_t offset, int len)
{
    assert(offset + len < 0x18000);

    uint32_t aligned_offset;
    uint32_t banked_offset;
    while (len > 0)
    {
        //Make sure we're on a 32bit boundary in a 16 byte sector
        aligned_offset = offset - (offset % 4);
        aligned_offset = aligned_offset - (aligned_offset % 16);

        //Select the right bank
        if (aligned_offset > 0xFFFF)
        {
            banked_offset = aligned_offset - (0xFFFF + 1);
            banked_offset |= (2 << 18);
        }
        else if (aligned_offset > 0x7FFF)
        {
            banked_offset = aligned_offset - (0x7FFF + 1);
            banked_offset |= (1 << 18);
        }
        else
        {
            banked_offset = aligned_offset;
        }

        //Read a sector of data from SRAM
        data_cache_hit_writeback_invalidate(sector_cache, 16);
        _dma_read(sector_cache, 0x08000000 + (banked_offset & 0x07FFFFFF), 16);

        //Copy only the required bytes into the dst buffer
        int br = CK_Cross_min(len, 16 - (offset - aligned_offset));
        memcpy(dst, sector_cache + (offset - aligned_offset), br);

        dst += br;
        offset += br;
        len -= br;
    }
}

static int write_sram(uint8_t *src, uint32_t offset, int len)
{
    assert(offset + len < 0x18000);

    uint32_t aligned_offset;
    uint32_t banked_offset;
    while (len > 0)
    {
        //Make sure we're on a 32bit boundary in a 16 byte sector
        aligned_offset = offset - (offset % 4);
        aligned_offset = aligned_offset - (aligned_offset % 16);

        //Select the right bank
        if (aligned_offset > 0xFFFF)
        {
            banked_offset = aligned_offset - (0xFFFF + 1);
            banked_offset |= (2 << 18);
        }
        else if (aligned_offset > 0x7FFF)
        {
            banked_offset = aligned_offset - (0x7FFF + 1);
            banked_offset |= (1 << 18);
        }
        else
        {
            banked_offset = aligned_offset;
        }

        //Read a sector of data from SRAM
        data_cache_hit_writeback_invalidate(sector_cache, 16);
        _dma_read(sector_cache, 0x08000000 + (banked_offset & 0x07FFFFFF), 16);
        int bw = CK_Cross_min(len, 16 - (offset - aligned_offset));
        memcpy(sector_cache + (offset - aligned_offset), src, bw);
        data_cache_hit_writeback_invalidate(sector_cache, 16);
        _dma_write(sector_cache, 0x08000000 + (banked_offset & 0x07FFFFFF), 16);

        src += bw;
        offset += bw;
        len -= bw;
    }
}

static char *FS_GetDataPath(const char *filename)
{
    static char path[32];
    snprintf(path, 32, "%s", filename);
    return path;
}

bool FS_IsFileValid(FS_File handle)
{
    return (handle > 0);
}

size_t FS_Read(void *ptr, size_t size, size_t nmemb, FS_File handle)
{
    int num_bytes = nmemb * size;
    if (handle & IN_SRAM)
    {
        handle &= ~IN_SRAM;
        assert(handle <= SRAM_NUMFILES);
        assert(handle != 0);
        int offset = sram_get_file_start_by_handle(handle) + sram_files[handle].offset;
        read_sram(ptr, offset, num_bytes);
        sram_files[handle].offset = (sram_files[handle].offset + num_bytes) % sram_files[handle].size;
        return nmemb;
    }

    int rb = dfs_read(ptr, 1, num_bytes, handle);
    if (rb != num_bytes)
    {
        debugf("Read byte mismatch %d %d\n", rb, num_bytes);
    }
    return rb / size;
}

size_t FS_Write(const void *ptr, size_t size, size_t nmemb, FS_File handle)
{
    int num_bytes = nmemb * size;
    if (handle & IN_SRAM)
    {
        handle &= ~IN_SRAM;
        assert(handle <= SRAM_NUMFILES);
        assert(handle != 0);
        int offset = sram_get_file_start_by_handle(handle) + sram_files[handle].offset;
        write_sram((uint8_t *)ptr, offset, num_bytes);
        sram_files[handle].offset = (sram_files[handle].offset + num_bytes) % sram_files[handle].size;
        return nmemb;
    }
    assert(0);
    //Can't write dfs. Shouldnt be needed.
    return 0;
}

size_t FS_SeekTo(FS_File handle, size_t offset)
{
    long int oldOff = 0;
    if (handle & IN_SRAM)
    {
        handle &= ~IN_SRAM;
        assert(handle <= SRAM_NUMFILES);
        assert(handle != 0);
        oldOff = sram_files[handle].offset;
        sram_files[handle].offset = offset;
    }
    else
    {
        oldOff = dfs_tell(handle);
        dfs_seek(handle, offset, SEEK_SET);
        return oldOff;
    }

    return oldOff;
}

void FS_CloseFile(FS_File handle)
{
    if (handle & IN_SRAM)
    {
        return;
    }
    dfs_close(handle);
}

size_t FS_GetFileSize(FS_File handle)
{
    if (handle & IN_SRAM)
    {
        handle &= ~IN_SRAM;
        assert(handle <= SRAM_NUMFILES);
        assert(handle != 0);
        return sram_files[handle].size;
    }
    return dfs_size(handle);
}

FS_File FS_OpenKeenFile(const char *filename)
{
    FS_File handle = dfs_open(FS_GetDataPath(filename));
    if (handle < 0)
    {
        debugf("Error opening %s\n", filename);
    }
    return handle;
}

FS_File FS_OpenOmniFile(const char *filename)
{
    return FS_OpenKeenFile(filename);
}

FS_File FS_OpenUserFile(const char *filename)
{
    FS_File handle = sram_get_handle_by_name(filename);
    if (handle < 0)
    {
        debugf("Could not open %s\n", filename);
        return 0;
    }
    sram_files[handle].offset = 0;
    return handle | IN_SRAM;
}

FS_File FS_CreateUserFile(const char *filename)
{
    FS_File handle = sram_get_handle_by_name(filename);
    if (handle < 0)
    {
        debugf("Could not create %s\n", filename);
        return 0;
    }
    sram_files[handle].offset = 0;
    return handle | IN_SRAM;
}

// Does a handle exist (and is it readable)
bool FS_IsKeenFilePresent(const char *filename)
{
    FS_File handle = dfs_open(FS_GetDataPath(filename));
    if (handle < 0)
    {
        debugf("%s is NOT present\n", filename);
        return false;
    }
    dfs_close(handle);
    return true;
}

// Does a handle exist (and is it readable)
bool FS_IsOmniFilePresent(const char *filename)
{
    FS_File handle = dfs_open(FS_GetDataPath(filename));
    if (handle < 0)
    {
        debugf("%s is NOT present\n", filename);
        return false;
    }
    dfs_close(handle);
    return true;
}

// Adjusts the extension on a filename to match the current episode.
// This function is NOT thread safe, and the string returned is only
// valid until the NEXT invocation of this function.
char *FS_AdjustExtension(const char *filename)
{
    static char newname[16];
    strcpy(newname, filename);
    size_t fnamelen = strlen(filename);
    newname[fnamelen - 3] = ck_currentEpisode->ext[0];
    newname[fnamelen - 2] = ck_currentEpisode->ext[1];
    newname[fnamelen - 1] = ck_currentEpisode->ext[2];
    return newname;
}

// Does a handle exist (and is it readable)
bool FS_IsUserFilePresent(const char *filename)
{
    if (sram_get_handle_by_name(filename) < 0)
    {
        debugf("FS_IsUserFilePresent %s is not present\n", filename);
        return false;
    }
    return true;
}

bool FSL_IsGoodOmniPath(const char *ext)
{
    if (!FS_IsOmniFilePresent("ACTION.CK4"))
    {
        debugf("Omnipath is NOT OK\n");
        return false;
    }
    return true;
}

bool FSL_IsGoodUserPath()
{
    return true;
}

void FS_Startup()
{
    debug_init(DEBUG_FEATURE_LOG_ISVIEWER);
    dfs_init(DFS_DEFAULT_LOCATION);
}

size_t FS_ReadInt8LE(void *ptr, size_t count, FS_File handle)
{
    return FS_Read(ptr, 1, count, handle);
}

size_t FS_ReadInt16LE(void *ptr, size_t count, FS_File handle)
{
    count = FS_Read(ptr, 2, count, handle);
#ifdef CK_CROSS_IS_BIGENDIAN
    uint16_t *uptr = (uint16_t *)ptr;
    for (size_t loopVar = 0; loopVar < count; loopVar++, uptr++)
        *uptr = CK_Cross_Swap16(*uptr);
#endif
    return count;
}

size_t FS_ReadInt32LE(void *ptr, size_t count, FS_File handle)
{
    count = FS_Read(ptr, 4, count, handle);
#ifdef CK_CROSS_IS_BIGENDIAN
    uint32_t *uptr = (uint32_t *)ptr;
    for (size_t loopVar = 0; loopVar < count; loopVar++, uptr++)
        *uptr = CK_Cross_Swap32(*uptr);
#endif
    return count;
}

size_t FS_WriteInt8LE(const void *ptr, size_t count, FS_File handle)
{
    return FS_Write(ptr, 1, count, handle);
}

size_t FS_WriteInt16LE(const void *ptr, size_t count, FS_File handle)
{
#ifndef CK_CROSS_IS_BIGENDIAN
    return FS_Write(ptr, 2, count, handle);
#else
    uint16_t val;
    size_t actualCount = 0;
    uint16_t *uptr = (uint16_t *)ptr;
    for (size_t loopVar = 0; loopVar < count; loopVar++, uptr++)
    {
        val = CK_Cross_Swap16(*uptr);
        actualCount += FS_Write(&val, 2, 1, handle);
    }
    return actualCount;
#endif
}

size_t FS_WriteInt32LE(const void *ptr, size_t count, FS_File handle)
{
#ifndef CK_CROSS_IS_BIGENDIAN
    return FS_Write(ptr, 4, count, handle);
#else
    uint32_t val;
    size_t actualCount = 0;
    uint32_t *uptr = (uint32_t *)ptr;
    for (size_t loopVar = 0; loopVar < count; loopVar++, uptr++)
    {
        val = CK_Cross_Swap32(*uptr);
        actualCount += FS_Write(&val, 4, 1, handle);
    }
    return actualCount;
#endif
}

size_t FS_ReadBoolFrom16LE(void *ptr, size_t count, FS_File handle)
{
    uint16_t val;
    size_t actualCount = 0;
    bool *currBoolPtr = (bool *)ptr; // No lvalue compilation error
    for (size_t loopVar = 0; loopVar < count; loopVar++, currBoolPtr++)
    {
        if (FS_Read(&val, 2, 1, handle)) // Should be either 0 or 1
        {
            *currBoolPtr = (val); // NOTE: No need to byte-swap
            actualCount++;
        }
    }
    return actualCount;
}

size_t FS_WriteBoolTo16LE(const void *ptr, size_t count, FS_File handle)
{
    uint16_t val;
    size_t actualCount = 0;
    bool *currBoolPtr = (bool *)ptr; // No lvalue compilation error
    for (size_t loopVar = 0; loopVar < count; loopVar++, currBoolPtr++)
    {
        val = CK_Cross_SwapLE16((*currBoolPtr) ? 1 : 0);
        actualCount += FS_Write(&val, 2, 1, handle);
    }
    return actualCount;
}

int FS_PrintF(FS_File handle, const char *fmt, ...)
{
    uint8_t buff[64]; //FIXME: Is this enough/ok on the stack?
    va_list args;
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    va_end(args);
    return FS_Write(buff, 1, strnlen(buff, sizeof(buff)), handle);
}

bool FS_LoadUserFile(const char *filename, mm_ptr_t *ptr, int *memsize)
{
    FS_File handle = FS_OpenUserFile(filename);

    if (!FS_IsFileValid(handle))
    {
        *ptr = 0;
        *memsize = 0;
        return false;
    }

    //Get length of handle
    int length = FS_GetFileSize(handle);

    MM_GetPtr(ptr, length);

    if (memsize)
        *memsize = length;
    int amountRead = FS_Read(*ptr, 1, length, handle);

    FS_CloseFile(handle);

    if (amountRead != length)
        return false;
    return true;
}
