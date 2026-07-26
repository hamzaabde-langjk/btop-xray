#include <stdlib.h>
#include "renderer.h"
struct Renderer { int dummy; };
Renderer* renderer_create(bool a, bool b) { (void)a;(void)b; return malloc(sizeof(Renderer)); }
void renderer_destroy(Renderer *r) { if(r) free(r); }
void renderer_render(Renderer *r) { (void)r; }
void renderer_update(Renderer *r, const void *d) { (void)r;(void)d; }
void renderer_resize(Renderer *r, int w, int h) { (void)r;(void)w;(void)h; }
bool renderer_is_ready(const Renderer *r) { (void)r; return true; }
float renderer_get_fps(const Renderer *r) { (void)r; return 60.0f; }
