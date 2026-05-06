#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
 
#ifndef FBIOPAN_DISPLAY
#define FBIOPAN_DISPLAY 0x4606
#endif
 
#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC _IOW('F', 0x20, unsigned int)
#endif
 
static volatile int running = 1;
 
void handle_signal(int sig) {
    (void)sig;
    running = 0;
}
 
/*
 * Efficient BGR8888 -> RGB8888 in-place conversion.
 * Each pixel is 0xAARRGGBB in memory (little-endian):
 *   BGR layout: byte0=B byte1=G byte2=R byte3=A
 *   RGB layout: byte0=R byte1=G byte2=B byte3=A
 * We swap R and B by rotating the lower 24 bits of each 32-bit word.
 * Uses 32-bit ops to avoid per-byte overhead.
 */
static void bgr_to_rgb(uint32_t *pixels, size_t count) {
    size_t i = 0;
 
    /* Process 4 pixels at a time to help auto-vectorization */
    for (; i + 4 <= count; i += 4) {
        uint32_t p0 = pixels[i+0];
        uint32_t p1 = pixels[i+1];
        uint32_t p2 = pixels[i+2];
        uint32_t p3 = pixels[i+3];
 
        /* swap R (bits 23:16) and B (bits 7:0), keep G and A in place */
        pixels[i+0] = (p0 & 0xFF00FF00u) | ((p0 & 0x00FF0000u) >> 16) | ((p0 & 0x000000FFu) << 16);
        pixels[i+1] = (p1 & 0xFF00FF00u) | ((p1 & 0x00FF0000u) >> 16) | ((p1 & 0x000000FFu) << 16);
        pixels[i+2] = (p2 & 0xFF00FF00u) | ((p2 & 0x00FF0000u) >> 16) | ((p2 & 0x000000FFu) << 16);
        pixels[i+3] = (p3 & 0xFF00FF00u) | ((p3 & 0x00FF0000u) >> 16) | ((p3 & 0x000000FFu) << 16);
    }
 
    /* Handle remaining pixels */
    for (; i < count; i++) {
        uint32_t p = pixels[i];
        pixels[i] = (p & 0xFF00FF00u) | ((p & 0x00FF0000u) >> 16) | ((p & 0x000000FFu) << 16);
    }
}
 
int main(int argc, char *argv[]) {
    const char *fbdev = "/dev/graphics/fb0";
    int fps = 30;
 
    if (argc >= 2) fbdev = argv[1];
    if (argc >= 3) fps = atoi(argv[2]);
    if (fps <= 0 || fps > 120) fps = 30;
 
    int fd = open(fbdev, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Cannot open %s: %s\n", fbdev, strerror(errno));
        return 1;
    }
 
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
 
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        fprintf(stderr, "FBIOGET_VSCREENINFO failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        fprintf(stderr, "FBIOGET_FSCREENINFO failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
 
    /* mmap the full framebuffer for in-place BGR->RGB conversion */
    size_t fb_size = finfo.line_length * vinfo.yres_virtual;
    uint32_t *fb = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
 
    /* number of 32-bit pixels per frame (only visible rows) */
    size_t pixels_per_frame = (finfo.line_length / 4) * vinfo.yres;
 
    printf("fb_refresh starting\n");
    printf("  Device     : %s\n", fbdev);
    printf("  Resolution : %dx%d (virtual %dx%d)\n",
           vinfo.xres, vinfo.yres, vinfo.xres_virtual, vinfo.yres_virtual);
    printf("  BPP        : %d\n", vinfo.bits_per_pixel);
    printf("  Line length: %d bytes\n", finfo.line_length);
    printf("  FB size    : %zu bytes\n", fb_size);
    printf("  FPS target : %d\n", fps);
    printf("  BGR->RGB   : enabled\n");
    fflush(stdout);
 
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);
 
    long interval_us = 1000000L / fps;
    unsigned int frame = 0;
    int pan_ok = 1;
    int vsync_ok = 1;
 
    while (running) {
        /* BGR8888 -> RGB8888 in-place conversion */
        bgr_to_rgb(fb, pixels_per_frame);
 
        /* Method 1: FBIOPAN_DISPLAY — most reliable way to trigger a hw flush */
        if (pan_ok) {
            vinfo.xoffset = 0;
            vinfo.yoffset = 0;
            if (ioctl(fd, FBIOPAN_DISPLAY, &vinfo) < 0) {
                if (errno != EINVAL) {
                    fprintf(stderr, "FBIOPAN_DISPLAY failed: %s — disabling\n", strerror(errno));
                }
                pan_ok = 0;
            }
        }
 
        /* Method 2: FBIO_WAITFORVSYNC — syncs to display vsync */
        if (vsync_ok) {
            unsigned int zero = 0;
            if (ioctl(fd, FBIO_WAITFORVSYNC, &zero) < 0) {
                /* Not fatal, many Android kernels don't support this */
                vsync_ok = 0;
            }
        }
 
        /* Method 3: Fallback — write a dummy byte to wake the driver */
        if (!pan_ok) {
            if (ioctl(fd, FBIOBLANK, FB_BLANK_UNBLANK) < 0) {
                /* ignore */
            }
        }
 
        frame++;
        if (frame % (fps * 5) == 0) {
            printf("  Refreshed %u frames (pan=%s vsync=%s)\n",
                   frame, pan_ok ? "yes" : "no", vsync_ok ? "yes" : "no");
            fflush(stdout);
        }
 
        usleep(interval_us);
    }
 
    munmap(fb, fb_size);
    printf("\nStopped after %u frames.\n", frame);
    close(fd);
    return 0;
}