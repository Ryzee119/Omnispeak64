/* RDP Display List support */

#pragma once

#include <libdragon.h>

#define FRAMEBUFFER_DEPTH DEPTH_16_BPP

#define COLOR_KEY (0x000000FF)

typedef uint16_t pixel_t; // 16-bit RGBA (5:5:5:1)

typedef struct Surface
{
	pixel_t *pixels;
	size_t width;
	size_t height;
} Surface;

typedef struct Rect
{
	long left;
	long top;
	long right;
	long bottom;
} Rect;

typedef struct RdpDisplayList
{
    uint32_t flags;
	uint64_t * cursor;
    uint64_t * flushed;
	uint64_t * end;
	uint64_t * commands;
} RdpDisplayList;

void rdl_init( void );
void rdl_close( void );

RdpDisplayList * rdl_alloc(size_t capacity);
void rdl_free(RdpDisplayList * rdl);

void rdl_exec(RdpDisplayList *rdl);
void rdl_finish(RdpDisplayList * rdl);

void rdl_attach_surface(
	RdpDisplayList * rdl,
	Surface * dst
);

void rdl_blit(
	RdpDisplayList * rdl,
	Surface * src,
	Rect * rect,
	int dest_x, int dest_y,
	bool color_key
);

void rdl_fillrect(
	RdpDisplayList * rdl,
	Rect * rect,
	pixel_t color
);
