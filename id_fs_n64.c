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

#include "ck_cross.h"
#include "ck_ep.h"

#include "id_fs.h"
#include "id_mm.h"
#include "id_us.h"

static int active_files = 0;
#define IN_EEPROM 0x8000
#define EEPROM_FSIZE 2048
#define EEPROM_NUMFILES 4
static const eepfs_entry_t eeprom_16k_files[EEPROM_NUMFILES] = {
    {"OMNISPK.CFG", EEPROM_FSIZE},
    {"CONFIG.CK4", EEPROM_FSIZE},
    {"SAVEGAM0.CK4", EEPROM_FSIZE},
};

//Allow seek and stream type reading/writing to eeprom files by tracking file position
//Position 0 isnt used and is reserved in the backend for a "signature file" so we have '+ 1'
static int eeprom_16k_offsets[EEPROM_NUMFILES + 1];

static char *FS_GetDataPath(const char *filename)
{
    static char path[32];
    snprintf(path, 32, "%s", filename);
    return path;
}

bool FS_IsFileValid(FS_File file)
{
    //FIXME FS_File needs to be int in fs.h
    if (file <= 0)
    {
        //fprintf(stderr,"file %d not valid\n", file);
    }
    return (file > 0);
}

size_t FS_Read(void *ptr, size_t size, size_t nmemb, FS_File file)
{
    int num_bytes = nmemb * size;
    /*
    if (file & IN_EEPROM)
    {
        file &= ~IN_EEPROM;
        const eepfs_file_t *_file = eepfs_get_file(file);
        if (_file == NULL)
        {
            return 0;
        }
        const size_t start_bytes = _file->start_block * EEPROM_BLOCK_SIZE + eeprom_16k_offsets[file];
        if ((start_bytes + num_bytes) >= EEPROM_FSIZE)
        {
            num_bytes = (start_bytes + num_bytes) - EEPROM_FSIZE - 1;
        }
        eeprom_read_bytes(ptr, start_bytes, num_bytes);
        return num_bytes;
        
        return 0;
    }
    */
    int rb = dfs_read(ptr, 1, num_bytes, file);
    if (rb != num_bytes)
    {
        debugf("Read byte mismatch %d %d\n", rb, num_bytes);
    }
    return rb / size;
}

size_t FS_Write(const void *ptr, size_t size, size_t nmemb, FS_File file)
{
    int num_bytes = nmemb * size;
    /*
    if (file & IN_EEPROM)
    {
        file &= ~IN_EEPROM;
        const eepfs_file_t *_file = eepfs_get_file(file);
        if (_file == NULL)
        {
            return 0;
        }
        const size_t start_bytes = _file->start_block * EEPROM_BLOCK_SIZE + eeprom_16k_offsets[file];
        if ((start_bytes + num_bytes) >= EEPROM_FSIZE)
        {
            num_bytes = (start_bytes + num_bytes) - EEPROM_FSIZE - 1;
        }
        eeprom_write_bytes(ptr, start_bytes, num_bytes);
        eeprom_16k_offsets[file] += num_bytes;
        return num_bytes;
        return 0;
    }
    */
    //Can't write dfs. Shouldnt be needed.
    return 0;
}

size_t FS_SeekTo(FS_File file, size_t offset)
{
    long int oldOff = 0;
    /*
    if (file & IN_EEPROM)
    {
        
        file &= ~IN_EEPROM;
        const eepfs_file_t *_file = eepfs_get_file(file);
        if (_file != NULL)
        {
            oldOff = eeprom_16k_offsets[file];
            eeprom_16k_offsets[file] = offset;
        }
        
    }
    else
    */
    //{
        oldOff = dfs_tell(file);
		dfs_seek(file, offset, SEEK_SET);
        return oldOff;
    //}

    return oldOff;
}

void FS_CloseFile(FS_File file)
{
    /*if (file & IN_EEPROM)
    {
        //Don't need to close eeprom files
        return;
    }*/
    dfs_close(file);
    active_files--;
    //debugf("active files %d\n", active_files);
}

size_t FS_GetFileSize(FS_File file)
{
    /*
    if (file & IN_EEPROM)
    {
        return EEPROM_FSIZE;
    }
    */
    return dfs_size(file);
}

FS_File FS_OpenKeenFile(const char *filename)
{
    FS_File fp = dfs_open(FS_GetDataPath(filename));
    if (fp < 0)
    {
        debugf("Error opening %s\n", filename);
    }
    active_files++;
    //debugf("active files %d\n", active_files);
    return fp;
}

FS_File FS_OpenOmniFile(const char *filename)
{
    return FS_OpenKeenFile(filename);
}

FS_File FS_OpenUserFile(const char *filename)
{
    /*
    int handle = eepfs_find_handle(filename);
    if (handle < 0)
    {
        return 0;
    }
    eeprom_16k_offsets[handle] = 0;
    return handle | IN_EEPROM;
    */
    return 0;
}

FS_File FS_CreateUserFile(const char *filename)
{
    /*
    int handle = eepfs_find_handle(filename);
    if (handle < 0)
    {
        return 0;
    }
    eeprom_16k_offsets[handle] = 0;
    return handle | IN_EEPROM;
    */
    return 0;
}

// Does a file exist (and is it readable)
bool FS_IsKeenFilePresent(const char *filename)
{
    FS_File fp = dfs_open(FS_GetDataPath(filename));
    if (fp < 0)
    {
        debugf("%s is NOT present\n", filename);
        return false;
    }
    dfs_close(fp);
    return true;
}

// Does a file exist (and is it readable)
bool FS_IsOmniFilePresent(const char *filename)
{
    FS_File fp = dfs_open(FS_GetDataPath(filename));
    if (fp < 0)
    {
        debugf("%s is NOT present\n", filename);
        return false;
    }
    dfs_close(fp);
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

// Does a file exist (and is it readable)
bool FS_IsUserFilePresent(const char *filename)
{
    return 0; //eepfs_find_handle(filename) > 0;
}

bool FSL_IsGoodOmniPath(const char *ext)
{
    if (!FS_IsOmniFilePresent("ACTION.CK4"))
        return false;
    debugf("Omnipath is OK\n");
    return true;
}

bool FSL_IsGoodUserPath()
{
    //const eeprom_type_t eeprom_type = eeprom_present();
    //if (eeprom_type == EEPROM_16K)
    //{
    //    return true;
    //}
    return false;
}

void FS_Startup()
{
    debug_init(DEBUG_FEATURE_LOG_ISVIEWER);
    dfs_init(DFS_DEFAULT_LOCATION);
    //eepfs_init(eeprom_16k_files, EEPROM_NUMFILES); //FIXME: Warn if no eeprom
}

size_t FS_ReadInt8LE(void *ptr, size_t count, FS_File stream)
{
    return FS_Read(ptr, 1, count, stream);
}

size_t FS_ReadInt16LE(void *ptr, size_t count, FS_File stream)
{
    count = FS_Read(ptr, 2, count, stream);
#ifdef CK_CROSS_IS_BIGENDIAN
    uint16_t *uptr = (uint16_t *)ptr;
    for (size_t loopVar = 0; loopVar < count; loopVar++, uptr++)
        *uptr = CK_Cross_Swap16(*uptr);
#endif
    return count;
}

size_t FS_ReadInt32LE(void *ptr, size_t count, FS_File stream)
{
    count = FS_Read(ptr, 4, count, stream);
#ifdef CK_CROSS_IS_BIGENDIAN
    uint32_t *uptr = (uint32_t *)ptr;
    for (size_t loopVar = 0; loopVar < count; loopVar++, uptr++)
        *uptr = CK_Cross_Swap32(*uptr);
#endif
    return count;
}

size_t FS_WriteInt8LE(const void *ptr, size_t count, FS_File stream)
{
    return FS_Write(ptr, 1, count, stream);
}

size_t FS_WriteInt16LE(const void *ptr, size_t count, FS_File stream)
{
#ifndef CK_CROSS_IS_BIGENDIAN
    return FS_Write(ptr, 2, count, stream);
#else
    uint16_t val;
    size_t actualCount = 0;
    uint16_t *uptr = (uint16_t *)ptr;
    for (size_t loopVar = 0; loopVar < count; loopVar++, uptr++)
    {
        val = CK_Cross_Swap16(*uptr);
        actualCount += FS_Write(&val, 2, 1, stream);
    }
    return actualCount;
#endif
}

size_t FS_WriteInt32LE(const void *ptr, size_t count, FS_File stream)
{
#ifndef CK_CROSS_IS_BIGENDIAN
    return FS_Write(ptr, 4, count, stream);
#else
    uint32_t val;
    size_t actualCount = 0;
    uint32_t *uptr = (uint32_t *)ptr;
    for (size_t loopVar = 0; loopVar < count; loopVar++, uptr++)
    {
        val = CK_Cross_Swap32(*uptr);
        actualCount += FS_Write(&val, 4, 1, stream);
    }
    return actualCount;
#endif
}

size_t FS_ReadBoolFrom16LE(void *ptr, size_t count, FS_File stream)
{
    uint16_t val;
    size_t actualCount = 0;
    bool *currBoolPtr = (bool *)ptr; // No lvalue compilation error
    for (size_t loopVar = 0; loopVar < count; loopVar++, currBoolPtr++)
    {
        if (FS_Read(&val, 2, 1, stream)) // Should be either 0 or 1
        {
            *currBoolPtr = (val); // NOTE: No need to byte-swap
            actualCount++;
        }
    }
    return actualCount;
}

size_t FS_WriteBoolTo16LE(const void *ptr, size_t count, FS_File stream)
{
    uint16_t val;
    size_t actualCount = 0;
    bool *currBoolPtr = (bool *)ptr; // No lvalue compilation error
    for (size_t loopVar = 0; loopVar < count; loopVar++, currBoolPtr++)
    {
        val = CK_Cross_SwapLE16((*currBoolPtr) ? 1 : 0);
        actualCount += FS_Write(&val, 2, 1, stream);
    }
    return actualCount;
}

int FS_PrintF(FS_File stream, const char *fmt, ...)
{
    uint8_t buff[64]; //FIXME: Is this enough/ok on the stack?
    va_list args;
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    va_end(args);
    return FS_Write(buff, 1, strnlen(buff, sizeof(buff)), stream);
}

bool FS_LoadUserFile(const char *filename, mm_ptr_t *ptr, int *memsize)
{
    FS_File file = FS_OpenUserFile(filename);

    if (!FS_IsFileValid(file))
    {
        *ptr = 0;
        *memsize = 0;
        return false;
    }

    //Get length of file
    int length = FS_GetFileSize(file);

    MM_GetPtr(ptr, length);

    if (memsize)
        *memsize = length;

    int amountRead = FS_Read(*ptr, 1, length, file);

    FS_CloseFile(file);

    if (amountRead != length)
        return false;
    return true;
}
