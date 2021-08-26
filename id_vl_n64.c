
#include "id_sd.h"
#include "id_us.h"
#include "id_vl.h"
#include "id_vl_private.h"

#include "ck_cross.h"

#include "n64_rdp/RdpDisplayList.h"
#include "n64_rdp/RdpCommands.h"
extern void *__safe_buffer[];
#define __get_buffer( x ) (pixel_t *)__safe_buffer[(x)-1]
#define rdl_push(rdl) (rdl->cursor++)

#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <libdragon.h>


#define USE_HW_RENDERER

static display_context_t disp = 0;
static const size_t FRAMEBUFFER_SIZE = (VL_EGAVGA_GFX_WIDTH * VL_EGAVGA_GFX_HEIGHT * 2);

static int vl_n64_screenWidth;
static int vl_n64_screenHeight;

#ifdef USE_HW_RENDERER
RdpDisplayList * rdl = NULL;
Surface framebuffer;
Surface blitbuffer;
Rect rect1;
Rect rect2;
#else
sprite_t *staging_sprite;
#endif

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
        display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
#ifndef USE_HW_RENDERER
        staging_sprite = malloc(sizeof(sprite_t) + FRAMEBUFFER_SIZE);
        staging_sprite->bitdepth = 2;
        staging_sprite->width = VL_EGAVGA_GFX_WIDTH;
        staging_sprite->height = VL_EGAVGA_GFX_HEIGHT;
        staging_sprite->hslices = 1;
        staging_sprite->vslices = 1;
#else
        rdl_init();
        rdl = rdl_alloc(1024);
#endif
        while (!(disp = display_lock()));
        framebuffer.pixels = __get_buffer(disp);
        framebuffer.width = VL_EGAVGA_GFX_WIDTH;
	    framebuffer.height = VL_EGAVGA_GFX_HEIGHT;

        blitbuffer.width = VL_EGAVGA_GFX_WIDTH / 2;
        blitbuffer.height = VL_EGAVGA_GFX_HEIGHT / 2;
        blitbuffer.pixels = (pixel_t *)memalign(64, blitbuffer.width * blitbuffer.height * 2);

        rect1.left = 0;
        rect1.top = 0;
        rect1.right = framebuffer.width;
        rect1.bottom = framebuffer.height;

        rect2.left = 0;
        rect2.top = 0;
        rect2.right = blitbuffer.width;
        rect2.bottom = blitbuffer.height;

        memset(framebuffer.pixels, 0x00, FRAMEBUFFER_SIZE);
        memset(blitbuffer.pixels, 0x2, blitbuffer.width * blitbuffer.height * 2);
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
    //Set pallete to Tile ID 1, TLUT0.
    *rdl_push(rdl) = RdpSetOtherModes(SOM_CYCLE_COPY | SOM_ENABLE_TLUT_RGB16);
    *rdl_push(rdl) = MRdpLoadPalette16(2, (uint32_t)VL_EGARGBColorTable, RDP_AUTO_TMEM_SLOT(0));
    *rdl_push(rdl) = RdpSyncTile();
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

const pixel_t white = 0xFFFF;
const pixel_t black = 0x0001;

static void VL_N64_Present(void *surface, int scrlX, int scrlY, bool singleBuffered)
{
    VL_N64_Surface *src = (VL_N64_Surface *)surface;
    //debugf("VL_N64_Present w: %d h: %d scrlx: %d scrly: %d\n", src->w, src->h, scrlX, scrlY);
    //for (int i = 150; i < 300; i++)
    //{
    //    debugf("%02x ", src->data[i]);
    //}
    //debugf("\n");
#ifdef USE_HW_RENDERER
    

    int blit_x = (framebuffer.width / 2) - (blitbuffer.width / 2);
    int blit_y = (framebuffer.height / 2) - (blitbuffer.height / 2);

    /*
    rdl_attach_surface(rdl, &blitbuffer);
    rdl_fillrect(rdl, &rect2, black);

    rdl_attach_surface(rdl, &framebuffer);
    rdl_fillrect(rdl, &rect1, white);
    rdl_blit(rdl, &blitbuffer, NULL, blit_x, blit_y, true);
    rdl_finish(rdl);
    display_show(disp);
    */
    rdl_attach_surface(rdl, &framebuffer);

    *rdl_push(rdl) = RdpSetOtherModes(SOM_CYCLE_COPY | SOM_ENABLE_TLUT_RGB16 | SOM_ALPHA_COMPARE);
    *rdl_push(rdl) = RdpSyncPipe();
    *rdl_push(rdl) = MRdpLoadPalette16(0, (uint32_t)VL_EGARGBColorTable, RDP_AUTO_TMEM_SLOT(0));
    *rdl_push(rdl) = RdpSyncTile();

    *rdl_push(rdl) = MRdpLoadTex8bpp(0, (uint32_t)blitbuffer.pixels, 48, 48, 1, RDP_AUTO_TMEM_SLOT(1), 1);
    *rdl_push(rdl) = RdpTextureRectangle1I(1, 1, 1, 48, 48);
    *rdl_push(rdl) = RdpTextureRectangle2I(0, 0, 4, 1);
    rdl_finish(rdl);
    display_show(disp);

    while (!(disp = display_lock())) {}
	framebuffer.pixels = __get_buffer(disp);

#else
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
#endif
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
