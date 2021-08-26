#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

#include "RdpDisplayList.h"

// Libdragon display helpers
static display_context_t now_drawing = 0;
extern void *__safe_buffer[];
#define __get_buffer( x ) (pixel_t *)__safe_buffer[(x)-1]

static const size_t FRAMEBUFFER_WIDTH = 320;
static const size_t FRAMEBUFFER_HEIGHT = 240;
static const size_t FRAMEBUFFER_COUNT = 2;
static const size_t SURFACE_PITCH = sizeof(pixel_t);
static const size_t FRAMEBUFFER_SIZE = FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * SURFACE_PITCH;
static const uint8_t OPAQUE = 0xFF;

int main(void)
{
    init_interrupts();
    controller_init();
    debug_init_isviewer();
	timer_init();
    display_init(
        RESOLUTION_320x240,
        DEPTH_16_BPP,
        FRAMEBUFFER_COUNT,
        GAMMA_NONE,
        ANTIALIAS_RESAMPLE
    );
    rdl_init();

    Surface framebuffer;
    framebuffer.width = FRAMEBUFFER_WIDTH;
	framebuffer.height = FRAMEBUFFER_HEIGHT;

    Surface blitbuffer;
    blitbuffer.width = FRAMEBUFFER_WIDTH / 2;
    blitbuffer.height = FRAMEBUFFER_HEIGHT / 2;
    const size_t blitbuffer_size = blitbuffer.width * blitbuffer.height * SURFACE_PITCH;
    blitbuffer.pixels = (pixel_t *)memalign(64, blitbuffer_size);

    RdpDisplayList * rdl = NULL;
    rdl = rdl_alloc(1024);

    while (!(now_drawing = display_lock())) {}
	framebuffer.pixels = __get_buffer(now_drawing);

    const pixel_t white = 0xFFFF;
    const pixel_t black = 0x0001;

    Rect rect1;
    rect1.left = 0;
    rect1.top = 0;
    rect1.right = framebuffer.width;
    rect1.bottom = framebuffer.height;

    Rect rect2;
    rect2.left = 0;
    rect2.top = 0;
    rect2.right = blitbuffer.width;
    rect2.bottom = blitbuffer.height;

    int blit_x = (framebuffer.width / 2) - (blitbuffer.width / 2);
    int blit_y = (framebuffer.height / 2) - (blitbuffer.height / 2);

    while(1)
    {
        // Clear the buffers before filling
        memset( blitbuffer.pixels, 0x00, blitbuffer_size );
        memset( framebuffer.pixels, 0x00, FRAMEBUFFER_SIZE );

        rdl_attach_surface(rdl, &blitbuffer);
        rdl_fillrect(rdl, &rect2, black);

	    rdl_attach_surface(rdl, &framebuffer);
        rdl_fillrect(rdl, &rect1, white);
        rdl_blit(rdl, &blitbuffer, NULL, blit_x, blit_y, true);

        rdl_finish(rdl);
		display_show(now_drawing);
        debugf( "blitbuffer[0]  = 0x%04x\n", blitbuffer.pixels[0]);
        debugf( "framebuffer[0] = 0x%04x\n", framebuffer.pixels[(framebuffer.width * blit_y) + blit_x]);

        // Only draw one frame so the logs don't go crazy
        while (1) {}

        while (!(now_drawing = display_lock())) {}
	    framebuffer.pixels = __get_buffer(now_drawing);
    }
}
