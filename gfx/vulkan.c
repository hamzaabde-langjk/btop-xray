/**
 * gfx/vulkan.c
 * عارض Vulkan (محاكاة)
 */

#include <stdio.h>
#include <stdlib.h>

#include "renderer.h"

typedef struct {
    int dummy;
} VulkanRenderer;

Renderer* renderer_create(bool use_gpu, bool obsidian_only) {
    (void)use_gpu;
    (void)obsidian_only;
    
    printf("⚠️  Vulkan غير مدعوم، استخدام العارض البرمجي\n");
    return NULL;
}

void renderer_destroy(Renderer *renderer) { (void)renderer; }
void renderer_render(Renderer *renderer) { (void)renderer; }
void renderer_update(Renderer *renderer, const void *data) { (void)renderer; (void)data; }
void renderer_resize(Renderer *renderer, int width, int height) { (void)renderer; (void)width; (void)height; }
bool renderer_is_ready(const Renderer *renderer) { (void)renderer; return false; }
float renderer_get_fps(const Renderer *renderer) { (void)renderer; return 0.0f; }
