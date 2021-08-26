#include "RdpDisplayList.h"

#include <malloc.h>
#include <stdbool.h>

#include "RdpCommands.h"

#define MIN(a,b) ((a)<(b)?(a):(b))

// RDP Register Pointers
#define RDP_REG_START   (((volatile uint32_t *)0xA4100000)[0])
#define RDP_REG_END     (((volatile uint32_t *)0xA4100000)[1])
#define RDP_REG_CURRENT (((volatile uint32_t *)0xA4100000)[2])
#define RDP_REG_STATUS  (((volatile uint32_t *)0xA4100000)[3])
// RDP Status Register Read Bits
#define RDP_STATUS_COMMAND_BUSY        0x040
#define RDP_STATUS_COMMAND_BUFFER_BUSY 0x080
#define RDP_STATUS_DMA_BUSY            0x100
#define RDP_STATUS_END_VALID           0x200
#define RDP_STATUS_START_VALID         0x400
// RDP Status Register Write Bits
#define RDP_STATUS_CLEAR_DMEM_DMA_MODE 0x001
#define RDP_STATUS_CLEAR_FREEZE        0x004
#define RDP_STATUS_CLEAR_FLUSH         0x010

#define UNCACHED_ADDR(addr) ((uint32_t)(addr) | 0xA0000000);

static const int TEXEL_SIZE = RDP_TEXEL_SIZE_16BIT;
static const int BLOCK_W = 48;
static const int BLOCK_H = 48;
static const int TILE_LINE_SIZE = BLOCK_W * TEXEL_SIZE / sizeof(uint64_t);
static const int DSDX = 4;
static const int DSDY = 1;

static pixel_t * attached = NULL;

#define RDL_FLAGS_STARTED (1<<0)

#define rdl_push(rdl) (rdl->cursor++)

static volatile uint32_t rdl_interrupt_count = 0;
static uint32_t rdl_interrupts_expected = 0;

static void __rdl_interrupt()
{
    rdl_interrupt_count++;
}

void rdl_init( void )
{
    register_DP_handler( __rdl_interrupt );
    set_DP_interrupt( 1 );
}

void rdl_close ( void )
{
    set_DP_interrupt( 0 );
    unregister_DP_handler( __rdl_interrupt );
}

RdpDisplayList * rdl_alloc(size_t capacity)
{
    RdpDisplayList * rdl = (RdpDisplayList *)malloc(sizeof(RdpDisplayList));
    if (rdl == NULL)
    {
        return NULL;
    }

    const size_t commands_size = (capacity * sizeof(uint64_t));
    rdl->commands = (uint64_t *)memalign(64, commands_size);
    if (rdl->commands == NULL)
    {
        free(rdl);
        return NULL;
    }

    rdl->flags = 0;
    rdl->flushed = rdl->cursor = &rdl->commands[0];
    rdl->end = &rdl->commands[capacity];

    return rdl;
}

void rdl_free(RdpDisplayList * rdl)
{
    if (rdl)
    {
        free(rdl->commands);
        free(rdl);
    }
}

void rdl_reset(RdpDisplayList * rdl)
{
    rdl->flags = 0;
    rdl->flushed = rdl->cursor = &rdl->commands[0];
}

static void rdl_flush_cache(RdpDisplayList * rdl)
{
    if (rdl->cursor == rdl->flushed) return;

    uint32_t length = (rdl->cursor - rdl->flushed) * sizeof(uint64_t);
    /* Ensure that the length is a multiple of 16 */
    uint32_t remainder = length % 16;
    if (remainder > 0) length = length + 16 - remainder;
    data_cache_hit_writeback(rdl->flushed, length);
    rdl->flushed = rdl->cursor;
}

void rdl_exec(RdpDisplayList *rdl)
{
    if (rdl->cursor == &rdl->commands[0]) return;

    rdl_flush_cache(rdl);

    // NOTE: THIS DOESN'T ACTUALLY WORK FOR REASONS NOT YET DETERMINED
    if ( rdl->flags & RDL_FLAGS_STARTED )
    {
        MEMORY_BARRIER();
        RDP_REG_END = UNCACHED_ADDR(rdl->flushed);
        MEMORY_BARRIER();
        return;
    }

    /* Best effort to be sure we can write once we disable interrupts */
    while (RDP_REG_STATUS & (RDP_STATUS_START_VALID | RDP_STATUS_END_VALID)) {}

    /* Make sure another thread doesn't attempt to render */
    disable_interrupts();

    /* Clear XBUS/Flush/Freeze */
    RDP_REG_STATUS = (
        RDP_STATUS_CLEAR_DMEM_DMA_MODE |
        RDP_STATUS_CLEAR_FLUSH |
        RDP_STATUS_CLEAR_FREEZE
    );
    MEMORY_BARRIER();

    /* Don't saturate the RDP command buffer.  Another command could have been written
     * since we checked before disabling interrupts, but it is unlikely, so we probably
     * won't stall in this critical section long. */
    while (RDP_REG_STATUS & (RDP_STATUS_START_VALID | RDP_STATUS_END_VALID)) {}

    /* Send start and end of buffer location to kick off the command transfer */
    MEMORY_BARRIER();
    RDP_REG_START = UNCACHED_ADDR(&rdl->commands[0]);
    MEMORY_BARRIER();
    RDP_REG_END = UNCACHED_ADDR(rdl->flushed);
    MEMORY_BARRIER();

    /* We are good now */
    enable_interrupts();

    rdl->flags |= RDL_FLAGS_STARTED;
}

void rdl_finish(RdpDisplayList * rdl)
{
    if (rdl->cursor != &rdl->commands[0])
    {
        *rdl_push(rdl) = RdpSyncFull();
        rdl_interrupts_expected++;

        rdl_exec(rdl);

        /* Wait for the RDP to finish what it's doing */
        while (rdl_interrupt_count < rdl_interrupts_expected) {}
    }

    rdl_reset(rdl);
    attached = NULL;
    rdl_interrupt_count = rdl_interrupts_expected = 0;
}

void rdl_attach_surface(RdpDisplayList * rdl, Surface * dst)
{
    if (attached == dst->pixels) return;

    if (rdl->cursor != &rdl->commands[0]) *rdl_push(rdl) = RdpSyncFull();
    *rdl_push(rdl) = RdpSetColorImage(RDP_COLOR_FORMAT_RGBA, TEXEL_SIZE, dst->width, (uint32_t)dst->pixels);
    *rdl_push(rdl) = RdpSetScissorI(0, 0, dst->width, dst->height);

    attached = dst->pixels;

    /* Disabled incremental rendering because it doesn't work */
    // rdl_exec(rdl);
}

void rdl_blit(
	RdpDisplayList * rdl,
	Surface * src,
	Rect * rect,
	int dest_x, int dest_y,
	bool color_key
)
{
    int s0 = 0;
    int t0 = 0;
    int width = src->width;
    int height = src->height;
    if (rect)
    {
        s0 = rect->left;
        t0 = rect->top;
        width = MIN(width - rect->left, rect->right - rect->left);
        height = MIN(height - rect->top, rect->bottom = rect->top);
    }

    *rdl_push(rdl) = RdpSyncPipe();
    *rdl_push(rdl) = RdpSetOtherModes(SOM_CYCLE_COPY | (color_key ? SOM_ALPHA_COMPARE : 0));
    *rdl_push(rdl) = RdpSetTexImage(RDP_COLOR_FORMAT_RGBA, TEXEL_SIZE, (uint32_t)src->pixels, src->width);
    *rdl_push(rdl) = RdpSetTile(RDP_COLOR_FORMAT_RGBA, TEXEL_SIZE, TILE_LINE_SIZE, 0, 0);
    if (color_key)
    {
        *rdl_push(rdl) = RdpSetBlendColor(COLOR_KEY);
        debugf( "RDP Set Other Modes: 0x%016llx\n", *(rdl->cursor - 4));
        debugf( "RDP Set Blend Color: 0x%016llx\n", *(rdl->cursor - 1));
    }

    for (int y = 0; y < height; y += BLOCK_H) {
        int bh = MIN(BLOCK_H, height - y);
        int yc = y + dest_y;
        int t = y + t0;

        if (yc + bh <= 0) continue;
        if (yc < 0)
        {
            t -= yc;
            bh += yc;
            yc = 0;
        }

        for (int x = 0; x < width; x += BLOCK_W) {
            int bw = MIN(BLOCK_W, width - x);
            int xc = x + dest_x;
            int s = x + s0;

            if (xc + bw <= 0) continue;
            if (xc < 0)
            {
                s -= xc;
                bw += xc;
                xc = 0;
            }

            *rdl_push(rdl) = RdpLoadTileI(0, s, t, (s+bw-1), (t+bh-1));
            *rdl_push(rdl) = RdpTextureRectangle1I(0, xc, yc, (xc+bw-1), (yc+bh-1));
            *rdl_push(rdl) = RdpTextureRectangle2I(s, t, DSDX, DSDY);
            *rdl_push(rdl) = RdpSyncTile();
        }
    }

    /* Disabled incremental rendering because it doesn't work */
    // rdl_exec(rdl);
}


void rdl_fillrect(RdpDisplayList * rdl, Rect * rect, pixel_t color)
{
    uint32_t rdp_color = ((uint32_t)color) | ((uint32_t)color << 16);

    *rdl_push(rdl) = RdpSyncPipe();
    *rdl_push(rdl) = RdpSetOtherModes(SOM_CYCLE_FILL);
    *rdl_push(rdl) = RdpSetFillColor(rdp_color);
    *rdl_push(rdl) = RdpFillRectangleI(rect->left, rect->top, rect->right, rect->bottom);

    /* Disabled incremental rendering because it doesn't work */
    // rdl_exec(rdl);
}

void rdl_fillrect_texture(RdpDisplayList * rdl, Rect * rect, pixel_t color, void *pal, void *tex)
{
   /* uint32_t rdp_color = ((uint32_t)color) | ((uint32_t)color << 16);
    *rdl_push(rdl) = RdpSetOtherModes(SOM_CYCLE_COPY | SOM_ENABLE_TLUT_RGB16);
    *rdl_push(rdl) = MRdpLoadPalette16(2, (uint32_t)pal, RDP_AUTO_TMEM_SLOT(0));
    *rdl_push(rdl) = RdpSyncTile();

    *rdl_push(rdl) = MRdpLoadTex4bpp(1, (uint32_t)tex, 48, 48, RDP_AUTO_PITCH, RDP_AUTO_TMEM_SLOT(1), RDP_AUTO_PITCH),
    *rdl_push(rdl) = RdpSyncTile();
    *rdl_push(rdl) = RdpTextureRectangle1I(0, xc, yc, (xc+bw-1), (yc+bh-1));
    *rdl_push(rdl) = RdpTextureRectangle2I(s, t, DSDX, DSDY);
    *rdl_push(rdl) = RdpSyncTile();*/
    /* Disabled incremental rendering because it doesn't work */
    // rdl_exec(rdl);
}
