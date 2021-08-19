#include "id_sd.h"
#include "id_us.h"
#include "id_vl.h"
#include "id_vl_private.h"

#include "ck_cross.h"

#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

static display_context_t disp = 0;
sprite_t *staging_sprite;
static int vl_n64_screenWidth;
static int vl_n64_screenHeight;

/* TODO (Overscan border):
 * - If a texture is used for offscreen rendering with scaling applied later,
 * it's better to have the borders within the texture itself.
 */

typedef struct VL_N64_Surface
{
    VL_SurfaceUsage use;
    int w, h;
    uint8_t *data;
} VL_N64_Surface;

static void VL_N64_SetVideoMode(int mode)
{
    if (mode == 0xD)
    {
        vl_n64_screenWidth = VL_EGAVGA_GFX_WIDTH;
        vl_n64_screenHeight = VL_EGAVGA_GFX_HEIGHT;
        init_interrupts();
        display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
        staging_sprite = malloc(sizeof(sprite_t) + (VL_EGAVGA_GFX_WIDTH * VL_EGAVGA_GFX_HEIGHT * 2));
        staging_sprite->bitdepth = 2;
        staging_sprite->width = VL_EGAVGA_GFX_WIDTH;
        staging_sprite->height = VL_EGAVGA_GFX_HEIGHT;
        staging_sprite->hslices = 1;
        staging_sprite->vslices = 1;
        rdp_init();
        while (!(disp = display_lock()))
            ;
    }
    else
    {
        display_close();
    }
}

static void VL_N64_SurfaceRect(void *dst_surface, int x, int y, int w, int h, int colour);
static void *VL_N64_CreateSurface(int w, int h, VL_SurfaceUsage usage)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)malloc(sizeof(VL_N64_Surface));
    surf->w = w;
    surf->h = h;
    surf->data = (uint8_t *)malloc(w * h); // 8-bit pal for now
    return surf;
}

static void VL_N64_DestroySurface(void *surface)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)surface;
    if (surf->data)
        free(surf->data);
    free(surf);
}

static long VL_N64_GetSurfaceMemUse(void *surface)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)surface;
    return surf->w * surf->h;
}

static void VL_N64_GetSurfaceDimensions(void *surface, int *w, int *h)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)surface;
    if (w)
        *w = surf->w;
    if (h)
        *h = surf->h;
}

static void VL_N64_RefreshPaletteAndBorderColor(void *screen)
{
}

static int VL_N64_SurfacePGet(void *surface, int x, int y)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)surface;
    return ((uint8_t *)surf->data)[y * surf->w + x];
}

static void VL_N64_SurfaceRect(void *dst_surface, int x, int y, int w, int h, int colour)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    for (int _y = y; _y < y + h; ++_y)
    {
        memset(((uint8_t *)surf->data) + _y * surf->w + x, colour, w);
    }
}

static void VL_N64_SurfaceRect_PM(void *dst_surface, int x, int y, int w, int h, int colour, int mapmask)
{
    mapmask &= 0xF;
    colour &= mapmask;

    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    for (int _y = y; _y < y + h; ++_y)
    {
        for (int _x = x; _x < x + w; ++_x)
        {
            uint8_t *p = ((uint8_t *)surf->data) + _y * surf->w + _x;
            *p &= ~mapmask;
            *p |= colour;
        }
    }
}

static void VL_N64_SurfaceToSurface(void *src_surface, void *dst_surface, int x, int y, int sx, int sy, int sw, int sh)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)src_surface;
    VL_N64_Surface *dest = (VL_N64_Surface *)dst_surface;
    for (int _y = sy; _y < sy + sh; ++_y)
    {
        memcpy(((uint8_t *)dest->data) + (_y - sy + y) * dest->w + x, ((uint8_t *)surf->data) + _y * surf->w + sx, sw);
    }
}

static void VL_N64_SurfaceToSelf(void *surface, int x, int y, int sx, int sy, int sw, int sh)
{
    VL_N64_Surface *srf = (VL_N64_Surface *)surface;
    bool directionX = sx > x;
    bool directionY = sy > y;

    if (directionY)
    {
        for (int yi = 0; yi < sh; ++yi)
        {
            memmove(((uint8_t *)srf->data) + ((yi + y) * srf->w + x), ((uint8_t *)srf->data) + ((sy + yi) * srf->w + sx), sw);
        }
    }
    else
    {
        for (int yi = sh - 1; yi >= 0; --yi)
        {
            memmove(((uint8_t *)srf->data) + ((yi + y) * srf->w + x), ((uint8_t *)srf->data) + ((sy + yi) * srf->w + sx), sw);
        }
    }
}

static void VL_N64_UnmaskedToSurface(void *src, void *dst_surface, int x, int y, int w, int h)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_UnmaskedToPAL8(src, surf->data, x, y, surf->w, w, h);
}

static void VL_N64_UnmaskedToSurface_PM(void *src, void *dst_surface, int x, int y, int w, int h, int mapmask)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_UnmaskedToPAL8_PM(src, surf->data, x, y, surf->w, w, h, mapmask);
}

static void VL_N64_MaskedToSurface(void *src, void *dst_surface, int x, int y, int w, int h)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_MaskedToPAL8(src, surf->data, x, y, surf->w, w, h);
}

static void VL_N64_MaskedBlitToSurface(void *src, void *dst_surface, int x, int y, int w, int h)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_MaskedBlitClipToPAL8(src, surf->data, x, y, surf->w, w, h, surf->w, surf->h);
}

static void VL_N64_BitToSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppToPAL8(src, surf->data, x, y, surf->w, w, h, colour);
}

static void VL_N64_BitToSurface_PM(void *src, void *dst_surface, int x, int y, int w, int h, int colour, int mapmask)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppToPAL8_PM(src, surf->data, x, y, surf->w, w, h, colour, mapmask);
}

static void VL_N64_BitXorWithSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppXorWithPAL8(src, surf->data, x, y, surf->w, w, h, colour);
}

static void VL_N64_BitBlitToSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppBlitToPAL8(src, surf->data, x, y, surf->w, w, h, colour);
}

static void VL_N64_BitInvBlitToSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppInvBlitClipToPAL8(src, surf->data, x, y, surf->w, w, h, surf->w, surf->h, colour);
}

static int VL_N64_GetActiveBufferId(void *surface)
{
    (void)surface;
    return disp - 1;
}

static int VL_N64_GetNumBuffers(void *surface)
{
    (void)surface;
    return 2;
}

static void VL_N64_ScrollSurface(void *surface, int x, int y)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)surface;
    int dx = 0, dy = 0, sx = 0, sy = 0;
    int w = surf->w - CK_Cross_max(x, -x), h = surf->h - CK_Cross_max(y, -y);
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
    VL_N64_Surface *src = (VL_N64_Surface *)surface;
    //debugf("VL_N64_Present w: %d h: %d scrlx: %d scrly: %d\n", src->w, src->h, scrlX, scrlY);
    //for (int i = 150; i < 300; i++)
    //{
    //    debugf("%02x ", src->data[i]);
    //}
    //debugf("\n");

    uint16_t *dest = (uint16_t *)staging_sprite->data;
    for (int _y = scrlY; _y < scrlY + src->h; _y++)
    {
        if (_y >= VL_EGAVGA_GFX_HEIGHT)
        {
            break;
        }
        for (int _x = scrlX; _x < scrlX + src->w; _x++)
        {
            if (_x >= VL_EGAVGA_GFX_WIDTH)
            {
                break;
            }
            uint8_t ega = src->data[_y * src->w + _x];
            uint8_t r = VL_EGARGBColorTable[vl_emuegavgaadapter.palette[ega]][0];
            uint8_t g = VL_EGARGBColorTable[vl_emuegavgaadapter.palette[ega]][1];
            uint8_t b = VL_EGARGBColorTable[vl_emuegavgaadapter.palette[ega]][2];
            uint16_t c = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 0x01; //rgba 5551
            //debugf("%02x %04x\n", ega, c);
            dest[_y * VL_EGAVGA_GFX_WIDTH + _x] = c;
        }
    }
    graphics_draw_sprite(disp, 0, 0, staging_sprite);
    display_show(disp);
    while (!(disp = display_lock()))
        ;
}

void VL_N64_FlushParams()
{
}

void VL_N64_WaitVBLs(int vbls)
{
    //graphics_draw_sprite already waits for vblank
    //SD_SetTimeCount(SD_GetTimeCount() + CK_Cross_max(vbls,1));
}

// Unfortunately, we can't take advantage of designated initializers in C++.
VL_Backend vl_n64_backend =
    {
        /*.setVideoMode =*/&VL_N64_SetVideoMode,
        /*.createSurface =*/&VL_N64_CreateSurface,
        /*.destroySurface =*/&VL_N64_DestroySurface,
        /*.getSurfaceMemUse =*/&VL_N64_GetSurfaceMemUse,
        /*.getSurfaceDimensions =*/&VL_N64_GetSurfaceDimensions,
        /*.refreshPaletteAndBorderColor =*/&VL_N64_RefreshPaletteAndBorderColor,
        /*.surfacePGet =*/&VL_N64_SurfacePGet,
        /*.surfaceRect =*/&VL_N64_SurfaceRect,
        /*.surfaceRect_PM =*/&VL_N64_SurfaceRect_PM,
        /*.surfaceToSurface =*/&VL_N64_SurfaceToSurface,
        /*.surfaceToSelf =*/&VL_N64_SurfaceToSelf,
        /*.unmaskedToSurface =*/&VL_N64_UnmaskedToSurface,
        /*.unmaskedToSurface_PM =*/&VL_N64_UnmaskedToSurface_PM,
        /*.maskedToSurface =*/&VL_N64_MaskedToSurface,
        /*.maskedBlitToSurface =*/&VL_N64_MaskedBlitToSurface,
        /*.bitToSurface =*/&VL_N64_BitToSurface,
        /*.bitToSurface_PM =*/&VL_N64_BitToSurface_PM,
        /*.bitXorWithSurface =*/&VL_N64_BitXorWithSurface,
        /*.bitBlitToSurface =*/&VL_N64_BitBlitToSurface,
        /*.bitInvBlitToSurface =*/&VL_N64_BitInvBlitToSurface,
        /*.scrollSurface =*/&VL_N64_ScrollSurface,
        /*.present =*/&VL_N64_Present,
        /*.getActiveBufferId =*/&VL_N64_GetActiveBufferId,
        /*.getNumBuffers =*/&VL_N64_GetNumBuffers,
        /*.flushParams =*/&VL_N64_FlushParams,
        /*.waitVBLs =*/&VL_N64_WaitVBLs};

VL_Backend *VL_Impl_GetBackend()
{
    return &vl_n64_backend;
}
