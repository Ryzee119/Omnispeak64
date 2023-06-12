// SPDX-License-Identifier: GPL-2.0

#include <libdragon.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <system.h>
#include "id_fs.h"

static const uint32_t SRAM_MAGIC = 0x64646464;
#define SRAMFS_MIN(a,b) (((a)<(b))?(a):(b))
#define SRAMFS_MAX(a,b) (((a)>(b))?(a):(b))
#define SRAM_BASE (0x08000000)

FS_File FSL_OpenFileInDirCaseInsensitive(const char *dirPath, const char *fileName, bool forWrite)
{
    char fullFileName[32];
    sprintf(fullFileName, "%s/%s", dirPath, fileName);
    FS_File fp = fopen(fullFileName, forWrite ? "wb" : "rb");
    return fp;
}

FS_File FSL_CreateFileInDir(const char *dirPath, const char *fileName)
{
    char fullFileName[32];
    sprintf(fullFileName, "%s/%s", dirPath, fileName);
    FS_File fp = fopen(fullFileName, "wb");
    return fp;
}

bool FSL_IsDirWritable(const char *dirPath)
{
    return true;
}

size_t FS_GetFileSize(FS_File file)
{
    fseek(file, 0, SEEK_END);
    uint32_t file_length = (uint32_t)ftell(file);
    fseek(file, 0, SEEK_SET);
    return file_length;
}

typedef struct sram_files_t
{
    const char *name;
    uint32_t size;
    uint32_t offset;
} sram_files_t;

sram_files_t *sram_files = NULL;
int sram_num_files = 0;

static int sram_get_handle_by_name(const char *name)
{
    for (int i = 1; i <= sram_num_files; i++)
    {
        if (strcasecmp(sram_files[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

static int sram_get_file_start_by_handle(int handle)
{
    int offset = 0;
    for (int i = 1; i < handle; i++)
    {
        offset += sram_files[i].size;
    }
    return offset;
}

uint8_t *sector_cache;

//Reading and writing to SRAM via DMA must occur from a 16 bytes aligned buffer. For Omnispeak, the save file
//can be greater than 1 SRAM bank (32kB), so I need to be able to manage access multiple banks contiguously from non aligned buffers at random lengths.
//I read and write by 16 byte 'sectors' and manage the banking automatically. If a read crosses to a different bank this is handled here.
static void read_sram(uint8_t *dst, uint32_t offset, int len)
{
    assert(offset + len < 0x18000);

    uint32_t aligned_offset;
    while (len > 0)
    {
        //Make sure we're on a 32bit boundary in a 16 byte sector
        aligned_offset = offset - (offset % 4);
        aligned_offset = aligned_offset - (aligned_offset % 16);

        //Read a sector of data from SRAM
        dma_read_raw_async(sector_cache, SRAM_BASE + aligned_offset, 16); 

        //Copy only the required bytes into the dst buffer
        int br = SRAMFS_MIN(len, 16 - (offset - aligned_offset));
        memcpy(dst, sector_cache + (offset - aligned_offset), br);

        dst += br;
        offset += br;
        len -= br;
    }
}

static void write_sram(uint8_t *src, uint32_t offset, int len)
{
    assert(offset + len < 0x18000);

    uint32_t aligned_offset;
    while (len > 0)
    {
        //Make sure we're on a 32bit boundary in a 16 byte sector
        aligned_offset = offset - (offset % 4);
        aligned_offset = aligned_offset - (aligned_offset % 16);

        //Read a sector of data from SRAM if writing to an unaligned sector
        //Can skip this read/modify/write if we're aligned and atleast 16 byte.
        if (len < 16 || (offset - aligned_offset) != 0)
        {
            dma_read_raw_async(sector_cache, SRAM_BASE + aligned_offset, 16); 
        }

        int bw = SRAMFS_MIN(len, 16 - (offset - aligned_offset));
        memcpy(sector_cache + (offset - aligned_offset), src, bw);
        dma_write_raw_async(sector_cache, SRAM_BASE + aligned_offset, 16); 

        src += bw;
        offset += bw;
        len -= bw;
    }
}

static void *__open(char *name, int flags)
{
    name++;
    int handle = sram_get_handle_by_name(name);
    if (handle <= 0)
    {
        return NULL;
    }

    int offset = sram_get_file_start_by_handle(handle);
    int magic;
    read_sram((uint8_t *)&magic, offset, sizeof(SRAM_MAGIC));

    //File is meant to be ready only, see if it exists by reading the first few bytes
    //to see if the magic number is present.
    if (flags == O_RDONLY && magic != SRAM_MAGIC)
    {
        return NULL;
    }

    //We should 'create' the file
    if (magic != SRAM_MAGIC)
    {
        //Write the magic number then return handle to the file, then zero the remainder
        magic = SRAM_MAGIC;
        write_sram((uint8_t *)&magic, offset, sizeof(SRAM_MAGIC));
        magic = 0;
        read_sram((uint8_t *)&magic, offset, sizeof(SRAM_MAGIC));
        assert(magic == SRAM_MAGIC);
        uint8_t zero[16] = {0};
        int remaining = sram_files[handle].size - sizeof(SRAM_MAGIC);
        int pos = 0;
        while(remaining)
        {
            int chunk = SRAMFS_MIN(sizeof(zero), remaining);
            write_sram(zero, sizeof(SRAM_MAGIC) + offset + pos, chunk);
            remaining -= chunk;
            pos += chunk;
        }
    }
    sram_files[handle].offset = 0;
    return (void *)handle;
}

static int __fstat( void *file, struct stat *st )
{
    int handle = (uint32_t)file;
    st->st_dev = 0;
    st->st_ino = 0;
    st->st_mode = S_IFREG;
    st->st_nlink = 1;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = 0;
    st->st_size = sram_files[handle].size;
    st->st_atime = 0;
    st->st_mtime = 0;
    st->st_ctime = 0;
    st->st_blksize = 0;
    st->st_blocks = 0;
    return 0;
}

static int __lseek(void *file, int ptr, int dir)
{
    int handle = (uint32_t)file;
    int new_offset = sram_files[handle].offset;

    if (dir == SEEK_SET)
    {
        new_offset = ptr;
    }
    else if (dir == SEEK_CUR)
    {
        new_offset += ptr;
    }
    else if (dir == SEEK_END)
    {
        new_offset = sram_files[handle].size;
    }

    if (new_offset < 0)
    {
        new_offset = 0;
    }
    else if (new_offset > sram_files[handle].size)
    {
        new_offset = sram_files[handle].size;
    }

    sram_files[handle].offset = new_offset;

    return new_offset;
}

static int __read( void *file, uint8_t *ptr, int len )
{
    int handle = (uint32_t)file;
    int offset = sram_get_file_start_by_handle(handle) + sram_files[handle].offset + sizeof(SRAM_MAGIC);
    int max_len = SRAMFS_MIN(len, sram_files[handle].size - offset);
    read_sram(ptr, offset, max_len);
    sram_files[handle].offset += max_len;
    return max_len;
}

static int __write( void *file, uint8_t *ptr, int len )
{
    int handle = (uint32_t)file;
    int offset = sram_get_file_start_by_handle(handle) + sram_files[handle].offset + sizeof(SRAM_MAGIC);
    write_sram(ptr, offset, len);
    sram_files[handle].offset += len;
    return len;
}

static int __close( void *file )
{
    return 0;
}

static filesystem_t sram_fs = {
    __open,
    __fstat,
    __lseek,
    __read,
    __write,
    __close,
    0,
    0,
    0
};

int sramfs_init(sram_files_t *files, int num_files)
{
    assert(files != NULL);
    assert(num_files > 0);

    sram_files = malloc(sizeof(sram_files_t) * (num_files + 1));
    sector_cache = malloc_uncached_aligned(16, 16);
    assert(sram_files != NULL);
    memcpy(&sram_files[1], files, sizeof(sram_files_t) * num_files);
    sram_num_files = num_files;
    int res = attach_filesystem("sram:/", &sram_fs);
    return res;
}