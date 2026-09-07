#include "text.h"
#include <stdio.h>
#include <stdarg.h>
#include <gccore.h>

void text_init(void *xfb, void *rmodeVoid) {
    GXRModeObj *rmode = (GXRModeObj *)rmodeVoid;
    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
}

void text_at(int row, int col, const char *fmt, ...) {
    printf("\x1b[%d;%dH", row + 1, col + 1);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void text_color(int c) {
    printf("\x1b[3%dm", c);
}

void text_reset(void) {
    printf("\x1b[37m");
}

void text_clear(void) {
    printf("\x1b[2J");
}

#define TEXT_QUEUE_MAX 64

typedef struct {
    int row, col, color;
    char str[48];
} TextQueueItem;

static TextQueueItem s_queue[TEXT_QUEUE_MAX];
static int s_queueCount = 0;

void text_queue(int row, int col, int color, const char *fmt, ...) {
    if (s_queueCount >= TEXT_QUEUE_MAX) return;
    TextQueueItem *it = &s_queue[s_queueCount++];
    it->row = row;
    it->col = col;
    it->color = color;
    va_list args;
    va_start(args, fmt);
    vsnprintf(it->str, sizeof(it->str), fmt, args);
    va_end(args);
}

void text_flush(void) {
    for (int i = 0; i < s_queueCount; i++) {
        text_color(s_queue[i].color);
        text_at(s_queue[i].row, s_queue[i].col, "%s", s_queue[i].str);
    }
    s_queueCount = 0;
}
