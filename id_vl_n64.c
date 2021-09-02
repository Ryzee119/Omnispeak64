
#include "id_sd.h"
#include "id_us.h"
#include "id_vl.h"
#include "id_vl_private.h"

#include "ck_cross.h"

#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <libdragon.h>

#ifndef USE_SW_RENDERER
#define USE_HW_RENDERER
#endif

typedef struct VL_N64_Surface
{
    VL_SurfaceUsage use;
    int width, height;
    uint8_t *pixels;
} VL_N64_Surface;

static display_context_t disp = 0;
uint32_t border_colour = 0xFFFFFFFF;

#ifdef USE_HW_RENDERER
#include "n64_rdp/rdl.h"
#include "n64_rdp/rdp_commands.h"
#define NUM_DISPLAY_LISTS 2
static RdpDisplayList *dls[NUM_DISPLAY_LISTS] = {NULL};
static RdpDisplayList *dl;
uint16_t *palette;
uint8_t pal_slot = 0;
#else
static const size_t FRAMEBUFFER_SIZE = (VL_EGAVGA_GFX_WIDTH * VL_EGAVGA_GFX_HEIGHT * 2);
sprite_t *staging_sprite;
#endif

static void VL_N64_SetVideoMode(int mode)
{
    if (mode == 0xD)
    {
        init_interrupts();
        display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);

        //Scale to EVGA 320x200 to 320x240. Original DOS version is 4:3
        uint32_t *vi_base = (uint32_t *)0xA4400000;
        vi_base[13] = 0x01000000 | (1024 * VL_EGAVGA_GFX_HEIGHT / 240);

#ifdef USE_HW_RENDERER
        rdp_init();
        for (int i = 0; i < NUM_DISPLAY_LISTS; i++)
        {
            dls[i] = rdl_heap_alloc(2048);
            rdl_reset(dls[i]);
        }
        dl = dls[0];
        palette = (uint16_t *)memalign(64, sizeof(uint16_t) * 16);
#else
        staging_sprite = malloc(sizeof(sprite_t) + FRAMEBUFFER_SIZE);
        staging_sprite->bitdepth = 2;
        staging_sprite->width = VL_EGAVGA_GFX_WIDTH;
        staging_sprite->height = VL_EGAVGA_GFX_HEIGHT;
        staging_sprite->hslices = 1;
        staging_sprite->vslices = 1;
#endif
    }
    else
    {
#ifdef USE_HW_RENDERER
        free(palette); //FIXME, free aligned memory?
        for (int i = 0; i < NUM_DISPLAY_LISTS; i++)
        {
            rdl_free(dls[i]);
        }
        rdp_detach_display();
#else
        free(staging_sprite);
        display_close();
#endif
    }
}

static void *VL_N64_CreateSurface(int w, int h, VL_SurfaceUsage usage)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)malloc(sizeof(VL_N64_Surface));
    surf->width = w;
    surf->height = h;
    surf->pixels = (uint8_t*)memalign(64, w * h);
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
    uint8_t r, g, b;
    r = VL_EGARGBColorTable[vl_emuegavgaadapter.bordercolor][0];
    g = VL_EGARGBColorTable[vl_emuegavgaadapter.bordercolor][1];
    b = VL_EGARGBColorTable[vl_emuegavgaadapter.bordercolor][2];
    border_colour = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 0x01; //rgba 5551
#ifdef USE_HW_RENDERER
    for (int i = 0; i < 16; i++)
    {
        r = VL_EGARGBColorTable[vl_emuegavgaadapter.palette[i]][0];
        g = VL_EGARGBColorTable[vl_emuegavgaadapter.palette[i]][1];
        b = VL_EGARGBColorTable[vl_emuegavgaadapter.palette[i]][2];
        uint16_t c = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 0x01; //rgba 5551
        palette[i] = c;
    }
    data_cache_hit_writeback_invalidate(palette, 16 * 2);
    //Load the palette into TMEM
    rdl_push(dl,
             RdpSyncTile(),
             MRdpLoadPalette16(2, (uint32_t)palette, RDP_AUTO_TMEM_SLOT(pal_slot)),
             RdpSyncTile()
    );
#endif
}

static int VL_N64_SurfacePGet(void *surface, int x, int y)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)surface;
    return ((uint8_t *)surf->pixels)[y * surf->width + x];
}

static void VL_N64_SurfaceRect(void *dst_surface, int x, int y, int w, int h, int colour)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
#if 0//#ifdef USE_HW_RENDERER
    //This mostly works but glitches out in the intro scenes?
    uint32_t _colour = (colour & 0xFF) << 24 | (colour & 0xFF) << 16 | (colour & 0xFF) << 8 | (colour & 0xFF);
    data_cache_hit_writeback_invalidate(surf->pixels, surf->width * surf->height);
    rdl_push(dl,
             RdpSetColorImage(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_8BIT, surf->width, (uint32_t)surf->pixels),
             RdpSetClippingI(0, 0, surf->width, surf->height),
             RdpSyncPipe(),
             RdpSetOtherModes(SOM_CYCLE_FILL),
             RdpSetFillColor(_colour), 
             RdpFillRectangleI(x, y, x + w, y + h)
    );
    rdl_flush(dl);
    rdl_exec(dl);
    rdl_reset(dl);
#else
    for (int _y = y; _y < y + h; ++_y)
    {
        memset(((uint8_t *)surf->pixels) + _y * surf->width + x, colour, w);
    }
#endif
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
            uint8_t *p = ((uint8_t *)surf->pixels) + _y * surf->width + _x;
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
        memcpy(((uint8_t *)dest->pixels) + (_y - sy + y) * dest->width + x, ((uint8_t *)surf->pixels) + _y * surf->width + sx, sw);
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
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_UnmaskedToPAL8(src, surf->pixels, x, y, surf->width, w, h);
}

static void VL_N64_UnmaskedToSurface_PM(void *src, void *dst_surface, int x, int y, int w, int h, int mapmask)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_UnmaskedToPAL8_PM(src, surf->pixels, x, y, surf->width, w, h, mapmask);
}

static void VL_N64_MaskedToSurface(void *src, void *dst_surface, int x, int y, int w, int h)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_MaskedToPAL8(src, surf->pixels, x, y, surf->width, w, h);
}

static void VL_N64_MaskedBlitToSurface(void *src, void *dst_surface, int x, int y, int w, int h)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_MaskedBlitClipToPAL8(src, surf->pixels, x, y, surf->width, w, h, surf->width, surf->height);
}

static void VL_N64_BitToSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppToPAL8(src, surf->pixels, x, y, surf->width, w, h, colour);
}

static void VL_N64_BitToSurface_PM(void *src, void *dst_surface, int x, int y, int w, int h, int colour, int mapmask)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppToPAL8_PM(src, surf->pixels, x, y, surf->width, w, h, colour, mapmask);
}

static void VL_N64_BitXorWithSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppXorWithPAL8(src, surf->pixels, x, y, surf->width, w, h, colour);
}

static void VL_N64_BitBlitToSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppBlitToPAL8(src, surf->pixels, x, y, surf->width, w, h, colour);
}

static void VL_N64_BitInvBlitToSurface(void *src, void *dst_surface, int x, int y, int w, int h, int colour)
{
    VL_N64_Surface *surf = (VL_N64_Surface *)dst_surface;
    VL_1bppInvBlitClipToPAL8(src, surf->pixels, x, y, surf->width, w, h, surf->width, surf->height, colour);
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
    VL_N64_Surface *src = (VL_N64_Surface *)surface;
    //debugf("VL_N64_Present w: %d h: %d scrlx: %d scrly: %d\n", src->width, src->height, scrlX, scrlY);
    while (!(disp = display_lock())) {}
#ifdef USE_HW_RENDERER
    data_cache_hit_writeback_invalidate(src->pixels,  src->width * src->height);
    //We draw the screen from top to bottom.
    //We can only draw 2048bytes per loop and for simplicity we want it to be a multiple
    //of the width.
    int x_per_loop = src->width;
    int y_per_loop = 2048 / src->width;
    int current_y = 0; //The texture is offset on the Yaxis by this many pixels
    int chunk_size = x_per_loop * y_per_loop;
    assert(chunk_size <= 2048);
    assert(y_per_loop > 0);

    rdp_attach_display(disp);
    rdp_set_clipping(0, 0, CK_Cross_min(VL_EGAVGA_GFX_WIDTH, src->width), CK_Cross_min(VL_EGAVGA_GFX_HEIGHT, src->height));

    /* Not sure if we really need to do this. Save some cycles
    rdl_push(dl,
        RdpSyncPipe(),
        RdpSetOtherModes(SOM_CYCLE_FILL),
        RdpSetFillColor16(border_colour),
        RdpFillRectangleI(0, 0, VL_EGAVGA_GFX_WIDTH, VL_EGAVGA_GFX_HEIGHT)
    );
    */

    rdl_push(dl,
        RdpSyncPipe(),
        RdpSetOtherModes(SOM_CYCLE_COPY | SOM_ALPHA_COMPARE | SOM_ENABLE_TLUT_RGB16)
    );

    uint8_t *ptr = src->pixels + scrlY * x_per_loop;
    while(current_y < src->height)
    {
        if (current_y >= VL_EGAVGA_GFX_HEIGHT)
        {
            break;
        }

        //Load y_per_loop * x_per_loop pixels into TMEM, and apply palette from pal_slot
        //Then draw to framebuffer
        rdl_push(dl,
            MRdpLoadTex8bpp(0, (uint32_t)ptr, x_per_loop, y_per_loop, x_per_loop, RDP_AUTO_TMEM_SLOT(0), RDP_AUTO_PITCH),
            MRdpSetTile8bpp(1, RDP_AUTO_TMEM_SLOT(0), RDP_AUTO_PITCH, RDP_AUTO_TMEM_SLOT(pal_slot), x_per_loop, y_per_loop),
            RdpTextureRectangle1I(1, 0, current_y, 0 + x_per_loop, current_y + y_per_loop),
            RdpTextureRectangle2I(scrlX, 0, 4, 1)
        );
        current_y += y_per_loop;
        ptr += chunk_size;
    }

    rdl_flush(dl);
    rdl_exec(dl);
    for (int i = 0; i < NUM_DISPLAY_LISTS; i++)
    {
        if (dl == dls[i])
        {
            dl = dls[(i + 1) % NUM_DISPLAY_LISTS];
            break;
        }
    }
    rdl_reset(dl);
    rdp_detach_display();
#else //Software Renderer
    uint16_t *dest = (uint16_t *)staging_sprite->data;
    for (int _y = scrlY; _y < src->height; _y++)
    {
        if (_y >= VL_EGAVGA_GFX_HEIGHT)
        {
            break;
        }
        for (int _x = scrlX; _x < src->width; _x++)
        {
            if (_x >= VL_EGAVGA_GFX_WIDTH)
            {
                break;
            }
            uint8_t ega = src->pixels[_y * src->width + _x];
            uint8_t r = VL_EGARGBColorTable[vl_emuegavgaadapter.palette[ega]][0];
            uint8_t g = VL_EGARGBColorTable[vl_emuegavgaadapter.palette[ega]][1];
            uint8_t b = VL_EGARGBColorTable[vl_emuegavgaadapter.palette[ega]][2];
            uint16_t c = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 0x01; //rgba 5551
            dest[(_y - scrlY) * VL_EGAVGA_GFX_WIDTH + _x - scrlX] = c;
        }
    }
    graphics_draw_sprite(disp, 0, 0, staging_sprite);
#endif
    display_show(disp);
}

static void VL_N64_FlushParams()
{
}

static void VL_N64_WaitVBLs(int vbls)
{
    //display_show already waits for vblank
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
    .flushParams = &VL_N64_FlushParams,
    .waitVBLs = &VL_N64_WaitVBLs
};

VL_Backend *VL_Impl_GetBackend()
{
    return &vl_n64_backend;
}
