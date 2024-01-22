// SPDX-License-Identifier: GPL-2.0

#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <libdragon.h>
#include "rdp.h"

#include "id_vl.h"
#include "id_vl_private.h"
#include "ck_cross.h"

typedef struct VL_N64_Surface
{
    VL_SurfaceUsage use;
    int width, height;
    uint8_t *pixels;
} VL_N64_Surface;

static surface_t *disp;
static uint32_t display_width;
static uint32_t display_height;
static uint32_t border_colour = 0xFFFFFFFF;

uint16_t *palette;
uint8_t pal_slot = 1;
static bool palette_dirty = false;

static void _do_audio_update()
{
    if (audio_can_write())
    {
        short *buf = audio_write_begin();
        mixer_poll(buf, audio_get_buffer_length());
        audio_write_end();
    }
}

static void VL_N64_SetVideoMode(int mode)
{
    if (mode == 0xD)
    {
        resolution_t res = {
            .height = 200,
            .width = 320,
            .interlaced = 0
        };
        display_init(res, DEPTH_16_BPP, 1, GAMMA_NONE, ANTIALIAS_RESAMPLE_FETCH_ALWAYS);
        rdpq_init();
        rdpq_set_fill_color(RGBA32(0,0,0,255));

        display_width = 320;
        display_height = 200;

        palette = (uint16_t *)memalign(64, sizeof(uint16_t) * 16);
        assert(palette != NULL);
    }
    else
    {
        rdpq_close();
        free(palette);
    }
}

static void *VL_N64_CreateSurface(int w, int h, VL_SurfaceUsage usage)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)malloc(sizeof(VL_N64_Surface));
    assert(surf != NULL);
    surf->width = w;
    surf->height = h;
    surf->pixels = (uint8_t*)memalign(64, w * h);
    assert(surf->pixels != NULL);
    return surf;
}

static void VL_N64_DestroySurface(void *surface)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)surface;
    if (surf->pixels)
        free(surf->pixels);
    free(surf);
}

static long VL_N64_GetSurfaceMemUse(void *surface)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)surface;
    return surf->width * surf->height;
}

static void VL_N64_GetSurfaceDimensions(void *surface, int *w, int *h)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)surface;
    if (w)
        *w = surf->width;
    if (h)
        *h = surf->height;
}

static void VL_N64_RefreshPaletteAndBorderColor(void *screen)
{
    _do_audio_update();
    uint8_t r, g, b;
    r = VL_EGARGBColorTable[vl_emuegavgaadapter.bordercolor][0];
    g = VL_EGARGBColorTable[vl_emuegavgaadapter.bordercolor][1];
    b = VL_EGARGBColorTable[vl_emuegavgaadapter.bordercolor][2];
    border_colour = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 0x01; //rgba 5551

    for (int i = 0; i < 16; i++)
    {
        r = VL_EGARGBColorTable[vl_emuegavgaadapter.palette[i]][0];
        g = VL_EGARGBColorTable[vl_emuegavgaadapter.palette[i]][1];
        b = VL_EGARGBColorTable[vl_emuegavgaadapter.palette[i]][2];
        uint16_t c = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 0x01; //rgba 5551
        palette[i] = c;
    }
    data_cache_hit_writeback_invalidate(palette, 16 * 2);
    palette_dirty = true;
}

static int VL_N64_SurfacePGet(void *surface, int x, int y)
{
    _do_audio_update();
    VL_N64_Surface *surf = (VL_N64_Surface *)surface;
    return ((uint8_t *)surf->pixels)[y * surf->width + x];
}

static void VL_N64_SurfaceRect(void *dst_surface, int x, int y, int w, int h, int colour)
{
    _do_audio_update();
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    for (int _y = y; _y < y + h; ++_y)
    {
        memset(((uint8_t *)surf->pixels) + (_y * surf->width) + x, colour, CK_Cross_min(w, surf->width - x));
    }
}

static void VL_N64_SurfaceRect_PM(void *dst_surface, int x, int y, int w, int h, int colour, int mapmask)
{
    _do_audio_update();
    mapmask &= 0xF;
    colour &= mapmask;

    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    for (int _y = y; _y < y + h; ++_y)
    {
        for (int _x = x; _x < x + w; ++_x)
        {
            uint8_t *p = ((uint8_t *)surf->pixels) + _y * surf->width + _x;
            *p &= ~mapmask;
            *p |= colour;
        }
    }
}

static void VL_N64_SurfaceToSurface(void *src_surface, void *dst_surface, int x, int y, int sx, int sy, int sw, int sh)
{
    _do_audio_update();
    VL_N64_Surface *surf = (VL_N64_Surface *)src_surface;
    VL_N64_Surface *dest = (VL_N64_Surface *)dst_surface;
    for (int _y = sy; _y < sy + sh; ++_y)
    {
        memcpy(((uint8_t *)dest->pixels) + (_y - sy + y) * dest->width + x, ((uint8_t *)surf->pixels) + _y * surf->width + sx, sw);
    }
}

static void VL_N64_SurfaceToSelf(void *surface, int x, int y, int sx, int sy, int sw, int sh)
{
    _do_audio_update();
    VL_N64_Surface *srf = (VL_N64_Surface *)surface;
    bool directionX = sx > x;
    (void) directionX;
    bool directionY = sy > y;

    if (directionY)
    {
        for (int yi = 0; yi < sh; ++yi)
        {
            memmove(((uint8_t *)srf->pixels) + ((yi + y) * srf->width + x), ((uint8_t *)srf->pixels) + ((sy + yi) * srf->width + sx), sw);
        }
    }
    else
    {
        for (int yi = sh - 1; yi >= 0; --yi)
        {
            memmove(((uint8_t *)srf->pixels) + ((yi + y) * srf->width + x), ((uint8_t *)srf->pixels) + ((sy + yi) * srf->width + sx), sw);
        }
    }
}

static void VL_N64_UnmaskedToSurface(void *src, void *dst_surface, int x, int y, int w, int h)
{
    _do_audio_update();
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_UnmaskedToPAL8(src, surf->pixels, x, y, surf->width, w, h);
}

static void VL_N64_UnmaskedToSurface_PM(void *src, void *dst_surface, int x, int y, int w, int h, int mapmask)
{
    _do_audio_update();
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_UnmaskedToPAL8_PM(src, surf->pixels, x, y, surf->width, w, h, mapmask);
}

static void VL_N64_MaskedToSurface(void *src, void *dst_surface, int x, int y, int w, int h)
{
    _do_audio_update();
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_MaskedToPAL8(src, surf->pixels, x, y, surf->width, w, h);
}

static void VL_N64_MaskedBlitToSurface(void *src, void *dst_surface, int x, int y, int w, int h)
{
    _do_audio_update();
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_MaskedBlitClipToPAL8(src, surf->pixels, x, y, surf->width, w, h, surf->width, surf->height);
}

static void VL_N64_BitToSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    _do_audio_update();
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppToPAL8(src, surf->pixels, x, y, surf->width, w, h, colour);
}

static void VL_N64_BitToSurface_PM(void *src, void *dst_surface, int x, int y, int w, int h, int colour, int mapmask)
{
    _do_audio_update();
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppToPAL8_PM(src, surf->pixels, x, y, surf->width, w, h, colour, mapmask);
}

static void VL_N64_BitXorWithSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    _do_audio_update();
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppXorWithPAL8(src, surf->pixels, x, y, surf->width, w, h, colour);
}

static void VL_N64_BitBlitToSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    _do_audio_update();
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppBlitToPAL8(src, surf->pixels, x, y, surf->width, w, h, colour);
}

static void VL_N64_BitInvBlitToSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    _do_audio_update();
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppInvBlitClipToPAL8(src, surf->pixels, x, y, surf->width, w, h, surf->width, surf->height, colour);
}

static int VL_N64_GetActiveBufferId(void *surface)
{
    return 0;
}

static int VL_N64_GetNumBuffers(void *surface)
{
    (void)surface;
    return 1;
}

static void VL_N64_ScrollSurface(void *surface, int x, int y)
{
    _do_audio_update();
    VL_N64_Surface *surf = (VL_N64_Surface *)surface;
    int dx = 0, dy = 0, sx = 0, sy = 0;
    int w = surf->width - CK_Cross_max(x, -x), h = surf->height - CK_Cross_max(y, -y);
    if (x > 0)
    {
        dx = 0;
        sx = x;
    }
    else
    {
        dx = -x;
        sx = 0;
    }
    if (y > 0)
    {
        dy = 0;
        sy = y;
    }
    else
    {
        dy = -y;
        sy = 0;
    }
    VL_N64_SurfaceToSelf(surface, dx, dy, sx, sy, w, h);
}

static void VL_N64_Present(void *surface, int scrlX, int scrlY, bool singleBuffered)
{
    _do_audio_update();

    VL_N64_Surface *src = (VL_N64_Surface *)surface;

    src->width = CK_Cross_min(src->width, 1024);

    disp = display_get();
    if (!disp)
    {
        return;
    }

    data_cache_hit_writeback_invalidate(src->pixels, src->width * src->height);

    rdpq_attach(disp, NULL);
    rdpq_set_scissor(0, 0, display_width, display_height);
    rdpq_set_mode_standard();
    rdpq_mode_tlut(TLUT_RGBA16);

    if (palette_dirty)
    {
        rdpq_tex_upload_tlut(palette, 0, 16);
        palette_dirty = false;
    }

    surface_t tex = {
            .buffer = src->pixels,
            .height = src->height,
            .width = src->width,
            .stride = src->width,
            .flags = FMT_CI8
        };

    rdpq_tex_blit(&tex, -scrlX, -scrlY, NULL);
    rdpq_detach_show();
}

static void VL_N64_FlushParams()
{
    _do_audio_update();
}

static void VL_N64_WaitVBLs(int vbls)
{
    long long micros = timer_ticks() + TIMER_TICKS_LL(1000000 * vbls / 60);
    do
    {
        _do_audio_update();
    } while (timer_ticks() < micros);
}

static void VL_N64_SyncBuffers(void *surface)
{
	(void)surface;
}

static void VL_N64_UpdateRect(void *surface, int x, int y, int w, int h)
{
	(void)surface;
	(void)x;
	(void)y;
	(void)w;
	(void)h;
}

VL_Backend vl_n64_backend =
{
    .setVideoMode = &VL_N64_SetVideoMode,
    .createSurface = &VL_N64_CreateSurface,
    .destroySurface = &VL_N64_DestroySurface,
    .getSurfaceMemUse = &VL_N64_GetSurfaceMemUse,
    .getSurfaceDimensions = &VL_N64_GetSurfaceDimensions,
    .refreshPaletteAndBorderColor = &VL_N64_RefreshPaletteAndBorderColor,
    .surfacePGet = &VL_N64_SurfacePGet,
    .surfaceRect = &VL_N64_SurfaceRect,
    .surfaceRect_PM = &VL_N64_SurfaceRect_PM,
    .surfaceToSurface = &VL_N64_SurfaceToSurface,
    .surfaceToSelf = &VL_N64_SurfaceToSelf,
    .unmaskedToSurface = &VL_N64_UnmaskedToSurface,
    .unmaskedToSurface_PM = &VL_N64_UnmaskedToSurface_PM,
    .maskedToSurface = &VL_N64_MaskedToSurface,
    .maskedBlitToSurface = &VL_N64_MaskedBlitToSurface,
    .bitToSurface = &VL_N64_BitToSurface,
    .bitToSurface_PM = &VL_N64_BitToSurface_PM,
    .bitXorWithSurface = &VL_N64_BitXorWithSurface,
    .bitBlitToSurface = &VL_N64_BitBlitToSurface,
    .bitInvBlitToSurface = &VL_N64_BitInvBlitToSurface,
    .scrollSurface = &VL_N64_ScrollSurface,
    .present = &VL_N64_Present,
    .getActiveBufferId = &VL_N64_GetActiveBufferId,
    .getNumBuffers = &VL_N64_GetNumBuffers,
    .syncBuffers = &VL_N64_SyncBuffers,
    .updateRect = &VL_N64_UpdateRect,
    .flushParams = &VL_N64_FlushParams,
    .waitVBLs = &VL_N64_WaitVBLs
};

VL_Backend *VL_Impl_GetBackend()
{
    return &vl_n64_backend;
}
