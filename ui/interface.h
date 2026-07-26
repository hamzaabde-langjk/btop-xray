#ifndef UI_INTERFACE_H
#define UI_INTERFACE_H

#include <stdbool.h>
#include <X11/Xlib.h>

typedef struct SimpleUI {
    Display *display;
    Window window;
    GC gc;
    int width;
    int height;
    void *shm_data;
    bool running;
    int selected_pid;
    int window_ready;
    int scroll_offset;
    int detail_mode;
    int header_height;
    int footer_height;
    int row_height;
    int padding;
    int corner_radius;
    XFontStruct *font;
    XFontStruct *font_bold;
    int font_height;
} SimpleUI;

typedef struct UIInterface {
    void (*update)(void *ui);
    void (*render)(void *ui);
    void (*destroy)(void *ui);
    void *data;
} UIInterface;

UIInterface* ui_create(void *renderer, void *shm_data, const char *filter);
void ui_destroy(UIInterface *ui);

#endif
