// SPDX-License-Identifier: GPL-2.0

#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <libdragon.h>
#include "rdp.h"

#include "id_vl.h"
#include "id_vl_private.h"
#include "ck_cross.h"

typedef surface_t VL_N64_Surface;

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
        const resolution_t RESOLUTION_320x200 = {320, 200, false};
        display_init(RESOLUTION_320x200, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE_FETCH_ALWAYS);
        rdpq_init();
        rdpq_debug_start();

        rdpq_set_fill_color(RGBA32(0,0,0,255));
        palette = (uint16_t *)malloc_uncached_aligned(64, sizeof(uint16_t) * 16);
        assert(palette != NULL);
    }
    else
    {
        rdpq_close();
        free_uncached(palette);
    }
}

static void *VL_N64_CreateSurface(int w, int h, VL_SurfaceUsage usage)
{
    surface_t surf = surface_alloc(FMT_CI8, w, h);
    surface_t *psurf = malloc(sizeof(surface_t));
    memcpy(psurf, &surf, sizeof(surface_t));
    return psurf;
}

static void VL_N64_DestroySurface(void *surface)
{
    surface_t *psurf = surface;
    surface_free(psurf);
    free(psurf);
}

static long VL_N64_GetSurfaceMemUse(void *surface)
{
    surface_t *surf = surface;
    return surf->stride * surf->height;
}

static void VL_N64_GetSurfaceDimensions(void *surface, int *w, int *h)
{
    surface_t *surf = surface;
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
    palette_dirty = true;
}

static int VL_N64_SurfacePGet(void *surface, int x, int y)
{
    _do_audio_update();
    surface_t *surf = surface;
    return ((uint8_t *)surf->buffer)[y * surf->width + x];
}

static void VL_N64_SurfaceRect(void *dst_surface, int x, int y, int w, int h, int colour)
{
    _do_audio_update();
    surface_t *surf = dst_surface;
    for (int _y = y; _y < y + h; ++_y)
    {
        memset(((uint8_t *)surf->buffer) + (_y * surf->width) + x, colour, CK_Cross_min(w, surf->width - x));
    }
}

static void VL_N64_SurfaceRect_PM(void *dst_surface, int x, int y, int w, int h, int colour, int mapmask)
{
    _do_audio_update();
    mapmask &= 0xF;
    colour &= mapmask;

    surface_t *surf = dst_surface;
    for (int _y = y; _y < y + h; ++_y)
    {
        for (int _x = x; _x < x + w; ++_x)
        {
            uint8_t *p = ((uint8_t *)surf->buffer) + _y * surf->width + _x;
            *p &= ~mapmask;
            *p |= colour;
        }
    }
}

static void VL_N64_SurfaceToSurface(void *src_surface, void *dst_surface, int x, int y, int sx, int sy, int sw, int sh)
{
    _do_audio_update();
    surface_t *surf = src_surface;
    surface_t *dest = dst_surface;
    for (int _y = sy; _y < sy + sh; ++_y)
    {
        memcpy(((uint8_t *)dest->buffer) + (_y - sy + y) * dest->width + x, ((uint8_t *)surf->buffer) + _y * surf->width + sx, sw);
    }
}

static void VL_N64_SurfaceToSelf(void *surface, int x, int y, int sx, int sy, int sw, int sh)
{
    _do_audio_update();
    surface_t *srf = surface;
    bool directionX = sx > x;
    bool directionY = sy > y;

    if (directionY)
    {
        for (int yi = 0; yi < sh; ++yi)
        {
            memmove(((uint8_t *)srf->buffer) + ((yi + y) * srf->width + x), ((uint8_t *)srf->buffer) + ((sy + yi) * srf->width + sx), sw);
        }
    }
    else
    {
        for (int yi = sh - 1; yi >= 0; --yi)
        {
            memmove(((uint8_t *)srf->buffer) + ((yi + y) * srf->width + x), ((uint8_t *)srf->buffer) + ((sy + yi) * srf->width + sx), sw);
        }
    }
}

static void VL_N64_UnmaskedToSurface(void *src, void *dst_surface, int x, int y, int w, int h)
{
    _do_audio_update();
    surface_t *surf = dst_surface;
    VL_UnmaskedToPAL8(src, surf->buffer, x, y, surf->width, w, h);
}

static void VL_N64_UnmaskedToSurface_PM(void *src, void *dst_surface, int x, int y, int w, int h, int mapmask)
{
    _do_audio_update();
    surface_t *surf = dst_surface;
    VL_UnmaskedToPAL8_PM(src, surf->buffer, x, y, surf->width, w, h, mapmask);
}

static void VL_N64_MaskedToSurface(void *src, void *dst_surface, int x, int y, int w, int h)
{
    _do_audio_update();
    surface_t *surf = dst_surface;
    VL_MaskedToPAL8(src, surf->buffer, x, y, surf->width, w, h);
}

static void VL_N64_MaskedBlitToSurface(void *src, void *dst_surface, int x, int y, int w, int h)
{
    _do_audio_update();
    surface_t *surf = dst_surface;
    VL_MaskedBlitClipToPAL8(src, surf->buffer, x, y, surf->width, w, h, surf->width, surf->height);
}

static void VL_N64_BitToSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    _do_audio_update();
    surface_t *surf = dst_surface;
    VL_1bppToPAL8(src, surf->buffer, x, y, surf->width, w, h, colour);
}

static void VL_N64_BitToSurface_PM(void *src, void *dst_surface, int x, int y, int w, int h, int colour, int mapmask)
{
    _do_audio_update();
    surface_t *surf = dst_surface;
    VL_1bppToPAL8_PM(src, surf->buffer, x, y, surf->width, w, h, colour, mapmask);
}

static void VL_N64_BitXorWithSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    _do_audio_update();
    surface_t *surf = dst_surface;
    VL_1bppXorWithPAL8(src, surf->buffer, x, y, surf->width, w, h, colour);
}

static void VL_N64_BitBlitToSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    _do_audio_update();
    surface_t *surf = dst_surface;
    VL_1bppBlitToPAL8(src, surf->buffer, x, y, surf->width, w, h, colour);
}

static void VL_N64_BitInvBlitToSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    _do_audio_update();
    surface_t *surf = dst_surface;
    VL_1bppInvBlitClipToPAL8(src, surf->buffer, x, y, surf->width, w, h, surf->width, surf->height, colour);
}

static int VL_N64_GetActiveBufferId(void *surface)
{
    static int d = 0;
    (void)surface;
    return d^=1;
}

static int VL_N64_GetNumBuffers(void *surface)
{
    (void)surface;
    return 2;
}

static void VL_N64_ScrollSurface(void *surface, int x, int y)
{
    _do_audio_update();
    surface_t *surf = surface;
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

    surface_t *omni_fb = surface;

    if (omni_fb->width > 1024)
    {
        return;
    }

    surface_t *disp = display_get();
    rdpq_attach(disp, NULL);
    rdpq_set_mode_standard();

    // We draw the screen from top to bottom.
    // We can only draw 2048bytes per loop and for simplicity we want it to be a multiple
    // of the displays width.
    int x_per_loop = disp->width;
    int y_per_loop = 5;
    int current_y = 0;
    assert((x_per_loop * y_per_loop) <= 2048);

    // Load palette into PALETTE_TILE
    if (palette_dirty)
    {
        rdpq_tex_upload_tlut(palette, 0, 16);
        palette_dirty = false;
    }

    uint8_t *old_buffer = omni_fb->buffer;
    intptr_t offset_buffer = (intptr_t)omni_fb->buffer + (scrlY * omni_fb->width);
    omni_fb->buffer = (void *)offset_buffer;

    while (current_y < disp->height)
    {
        rdpq_mode_tlut(TLUT_RGBA16);
        rdpq_tex_upload_sub(TILE0, omni_fb, NULL, scrlX, current_y, x_per_loop + scrlX, current_y + y_per_loop);
        rdpq_texture_rectangle(TILE0, 0, current_y, x_per_loop, current_y + y_per_loop, scrlX, current_y);
        current_y += y_per_loop;
    }
    omni_fb->buffer = old_buffer;

    rdpq_detach_show();
    _do_audio_update();
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

}

static void VL_N64_UpdateRect(void *surface, int x, int y, int w, int h)
{

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
    .syncBuffers = VL_N64_SyncBuffers,
    .updateRect = VL_N64_UpdateRect,
    .flushParams = &VL_N64_FlushParams,
    .waitVBLs = &VL_N64_WaitVBLs
};

VL_Backend *VL_Impl_GetBackend()
{
    return &vl_n64_backend;
}
