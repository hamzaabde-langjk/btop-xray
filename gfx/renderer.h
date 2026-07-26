#ifndef GFX_RENDERER_H
#define GFX_RENDERER_H
#include <stdbool.h>
typedef struct Renderer Renderer;
Renderer* renderer_create(bool use_gpu, bool obsidian_only);
void renderer_destroy(Renderer *renderer);
void renderer_render(Renderer *renderer);
void renderer_update(Renderer *renderer, const void *data);
void renderer_resize(Renderer *renderer, int width, int height);
bool renderer_is_ready(const Renderer *renderer);
float renderer_get_fps(const Renderer *renderer);
#endif
