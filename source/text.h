// Thin wrapper over libogc's built-in console for HUD/menu text. GX draws
// every shape and line; text is layered on top each frame using the
// console's ANSI cursor-positioning and color escape codes, so we get a
// known-good bitmap font without hand-rolling glyph data.
#ifndef VECTREK_TEXT_H
#define VECTREK_TEXT_H

void text_init(void *xfb, void *rmode);

/** Move the console cursor to a 0-indexed character row/col and print. */
void text_at(int row, int col, const char *fmt, ...);

// Basic ANSI foreground colors (\x1b[3Nm).
#define TXT_BLACK   0
#define TXT_RED     1
#define TXT_GREEN   2
#define TXT_YELLOW  3
#define TXT_BLUE    4
#define TXT_MAGENTA 5
#define TXT_CYAN    6
#define TXT_WHITE   7

void text_color(int ansiColor);
void text_reset(void);
void text_clear(void);

/**
 * Deferred text: console printf writes straight into the framebuffer, so
 * anything drawn while a GX scene is still queued gets wiped out by the
 * GX_CopyDisp that copies the rendered scene into that same buffer. Game
 * HUD/world labels queue their text with text_queue() during drawing;
 * main.c calls text_flush() right after GX_CopyDisp so labels land on top
 * of the finished frame instead of underneath it. Menu screens have no GX
 * draws in a frame, so they can keep using text_at() directly.
 */
void text_queue(int row, int col, int ansiColor, const char *fmt, ...);
void text_flush(void);

#endif
