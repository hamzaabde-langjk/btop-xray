#ifndef UI_INTERFACE_H
#define UI_INTERFACE_H

#include <stdbool.h>
#include <X11/Xlib.h>

typedef struct {
    Display *dpy;
    Window win;
    GC gc;
    int width;
    int height;
    void *data;
    int selected_pid;
    int scroll_offset;
    int header_height;
    int footer_height;
    int row_height;
    int padding;
} UIState;

typedef struct {
    void (*render)(void *state);
    void (*destroy)(void *state);
    UIState *state;
} UIInterface;

UIInterface* ui_create(void *shm_data);
void ui_destroy(UIInterface *ui);

#endif
