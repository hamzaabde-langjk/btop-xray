/**
 * gfx/softpipe.c
 * عارض برمجي (بدون GPU) - يعمل على جميع الأنظمة
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "renderer.h"

/* ============================
   بنية العارض البرمجي
   ============================ */

typedef struct {
    Display *display;
    Window window;
    GC gc;
    int width;
    int height;
    uint32_t *framebuffer;
    size_t fb_size;
    bool running;
    float fps;
    uint64_t frame_count;
    struct timeval last_time;
    void *data;
    pthread_mutex_t lock;
} SoftRenderer;

/* ============================
   دوال الرسم الأساسية
   ============================ */

/* رسم بكسل */
static void draw_pixel(SoftRenderer *r, int x, int y, uint32_t color) {
    if (x < 0 || x >= r->width || y < 0 || y >= r->height) return;
    r->framebuffer[y * r->width + x] = color;
}

/* رسم دائرة */
static void draw_circle(SoftRenderer *r, int cx, int cy, int radius, uint32_t color) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                draw_pixel(r, cx + x, cy + y, color);
            }
        }
    }
}

/* رسم خط */
static void draw_line(SoftRenderer *r, int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        draw_pixel(r, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/* ============================
   تنفيذ دوال العارض
   ============================ */

Renderer* renderer_create(bool use_gpu, bool obsidian_only) {
    (void)use_gpu; /* تجاهل، نحن دائماً برمجي هنا */
    (void)obsidian_only;
    
    SoftRenderer *r = malloc(sizeof(SoftRenderer));
    if (!r) return NULL;
    
    memset(r, 0, sizeof(SoftRenderer));
    r->width = 1024;
    r->height = 768;
    r->running = true;
    pthread_mutex_init(&r->lock, NULL);
    
    /* تهيئة X11 */
    r->display = XOpenDisplay(NULL);
    if (!r->display) {
        fprintf(stderr, "❌ فشل فتح X11 Display\n");
        free(r);
        return NULL;
    }
    
    int screen = DefaultScreen(r->display);
    Window root = RootWindow(r->display, screen);
    
    XSetWindowAttributes attrs;
    attrs.background_pixel = BlackPixel(r->display, screen);
    attrs.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;
    
    r->window = XCreateWindow(r->display, root, 0, 0, r->width, r->height,
                              0, CopyFromParent, InputOutput, CopyFromParent,
                              CWBackPixel | CWEventMask, &attrs);
    
    XStoreName(r->display, r->window, "XRAY-SCOPE - Omniscient Lens");
    XMapWindow(r->display, r->window);
    
    r->gc = XCreateGC(r->display, r->window, 0, NULL);
    
    /* تخصيص ذاكرة الإطار */
    r->fb_size = r->width * r->height * sizeof(uint32_t);
    r->framebuffer = malloc(r->fb_size);
    if (!r->framebuffer) {
        XDestroyWindow(r->display, r->window);
        XCloseDisplay(r->display);
        free(r);
        return NULL;
    }
    
    memset(r->framebuffer, 0, r->fb_size);
    gettimeofday(&r->last_time, NULL);
    
    printf("🖥️  العارض البرمجي جاهز (%dx%d)\n", r->width, r->height);
    
    return (Renderer*)r;
}

void renderer_destroy(Renderer *renderer) {
    if (!renderer) return;
    SoftRenderer *r = (SoftRenderer*)renderer;
    
    r->running = false;
    
    if (r->framebuffer) free(r->framebuffer);
    if (r->gc) XFreeGC(r->display, r->gc);
    if (r->window) XDestroyWindow(r->display, r->window);
    if (r->display) XCloseDisplay(r->display);
    
    pthread_mutex_destroy(&r->lock);
    free(r);
}

void renderer_render(Renderer *renderer) {
    if (!renderer) return;
    SoftRenderer *r = (SoftRenderer*)renderer;
    
    pthread_mutex_lock(&r->lock);
    
    /* حساب FPS */
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = (now.tv_sec - r->last_time.tv_sec) +
                     (now.tv_usec - r->last_time.tv_usec) / 1000000.0;
    r->frame_count++;
    
    if (elapsed > 1.0) {
        r->fps = r->frame_count / elapsed;
        r->frame_count = 0;
        r->last_time = now;
    }
    
    /* تنظيف الإطار */
    memset(r->framebuffer, 0, r->fb_size);
    
    /* رسم خلفية داكنة (كوكب الأرض) */
    for (int y = 0; y < r->height; y++) {
        for (int x = 0; x < r->width; x++) {
            /* رسم تدرج يشبه الكرة الأرضية */
            int dx = x - r->width/2;
            int dy = y - r->height/2;
            int dist = sqrt(dx*dx + dy*dy);
            
            if (dist < r->height/3) {
                uint8_t blue = 200 - (dist * 200) / (r->height/3);
                uint8_t green = 100 - (dist * 100) / (r->height/3);
                uint8_t red = 50 - (dist * 50) / (r->height/3);
                r->framebuffer[y * r->width + x] = (red << 16) | (green << 8) | blue;
            } else {
                /* خلفية فضائية */
                uint8_t v = 10 + (rand() % 20);
                r->framebuffer[y * r->width + x] = (v << 16) | (v << 8) | v;
            }
        }
    }
    
    /* رسم نجوم (عمليات) */
    for (int i = 0; i < 50; i++) {
        int x = 100 + (rand() % (r->width - 200));
        int y = 100 + (rand() % (r->height - 200));
        uint32_t color = 0xFF0000 + (rand() % 0x00FFFF);
        int radius = 3 + (rand() % 8);
        draw_circle(r, x, y, radius, color);
        
        /* رسم اتصالات بين النجوم */
        if (i > 0 && i % 5 == 0) {
            int x2 = 100 + (rand() % (r->width - 200));
            int y2 = 100 + (rand() % (r->height - 200));
            draw_line(r, x, y, x2, y2, 0x00FF00);
        }
    }
    
    /* رسم معلومات */
    char info[256];
    snprintf(info, sizeof(info), "XRAY-SCOPE v1.0 | FPS: %.1f | Softpipe",
             r->fps);
    XDrawString(r->display, r->window, r->gc, 10, 20, info, strlen(info));
    
    snprintf(info, sizeof(info), "🔍 مراقبة النظام في الزمن الحقيقي");
    XDrawString(r->display, r->window, r->gc, 10, 40, info, strlen(info));
    
    /* عرض الإطار على الشاشة */
    XImage *image = XCreateImage(r->display, DefaultVisual(r->display, 0),
                                 24, ZPixmap, 0, (char*)r->framebuffer,
                                 r->width, r->height, 32, r->width * 4);
    if (image) {
        XPutImage(r->display, r->window, r->gc, image, 0, 0, 0, 0,
                  r->width, r->height);
        XDestroyImage(image);
    }
    
    XFlush(r->display);
    
    pthread_mutex_unlock(&r->lock);
}

void renderer_update(Renderer *renderer, const void *data) {
    if (!renderer) return;
    SoftRenderer *r = (SoftRenderer*)renderer;
    
    pthread_mutex_lock(&r->lock);
    r->data = (void*)data;
    pthread_mutex_unlock(&r->lock);
}

void renderer_resize(Renderer *renderer, int width, int height) {
    if (!renderer) return;
    SoftRenderer *r = (SoftRenderer*)renderer;
    
    pthread_mutex_lock(&r->lock);
    r->width = width;
    r->height = height;
    /* إعادة تخصيص الإطار */
    size_t new_size = width * height * sizeof(uint32_t);
    uint32_t *new_fb = realloc(r->framebuffer, new_size);
    if (new_fb) {
        r->framebuffer = new_fb;
        r->fb_size = new_size;
    }
    pthread_mutex_unlock(&r->lock);
}

bool renderer_is_ready(const Renderer *renderer) {
    if (!renderer) return false;
    SoftRenderer *r = (SoftRenderer*)renderer;
    return r->running && r->display != NULL;
}

float renderer_get_fps(const Renderer *renderer) {
    if (!renderer) return 0.0f;
    SoftRenderer *r = (SoftRenderer*)renderer;
    return r->fps;
}
