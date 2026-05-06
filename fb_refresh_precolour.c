#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
 
/* FBIOPAN_DISPLAY forces the display hardware to re-read the framebuffer */
#ifndef FBIOPAN_DISPLAY
#define FBIOPAN_DISPLAY 0x4606
#endif
 
/* Samsung/Exynos custom ioctl to trigger a vsync/refresh */
#define S3CFB_VSYNC_OFF       _IO('F', 32)
#define S3CFB_VSYNC_ON        _IO('F', 33)
#define FBIO_WAITFORVSYNC     _IOW('F', 0x20, unsigned int)
 
static volatile int running = 1;
 
void handle_signal(int sig) {
    (void)sig;
    running = 0;
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
 
    printf("fb_refresh starting\n");
    printf("  Device     : %s\n", fbdev);
    printf("  Resolution : %dx%d (virtual %dx%d)\n",
           vinfo.xres, vinfo.yres, vinfo.xres_virtual, vinfo.yres_virtual);
    printf("  BPP        : %d\n", vinfo.bits_per_pixel);
    printf("  Line length: %d bytes\n", finfo.line_length);
    printf("  FPS target : %d\n", fps);
    fflush(stdout);
 
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);
 
    long interval_us = 1000000L / fps;
    unsigned int frame = 0;
    int pan_ok = 1;
    int vsync_ok = 1;
 
    while (running) {
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
 
    printf("\nStopped after %u frames.\n", frame);
    close(fd);
    return 0;
}