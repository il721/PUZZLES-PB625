#include <stdio.h>
#include <time.h>

#include "shell.h"

#ifndef LOGF
#define LOGF "/mnt/ext1/puzzlebook.txt"
#endif

ifont *ui_f_btn, *ui_f_small, *ui_f_title, *ui_f_tiny;
static int fonts_size;

void ui_log(const char *fmt, int a, int b, int c) {
  FILE *f = fopen(LOGF, "a");
  if (!f) return;
  fprintf(f, fmt, a, b, c);
  fputc('\n', f);
  fclose(f);
}

long long ui_now_ms(void) { return (long long)time(NULL) * 1000LL; }

ifont *ui_open_font(const char *name, int size) {
  ifont *f = OpenFont(name, size, 1);
  if (!f) f = OpenFont("LiberationSans", size, 1);
  return f;
}

void ui_fonts(int btn_size) {
  if (ui_f_btn && fonts_size == btn_size) return;
  if (ui_f_btn) CloseFont(ui_f_btn);
  if (ui_f_small) CloseFont(ui_f_small);
  if (ui_f_title) CloseFont(ui_f_title);
  if (ui_f_tiny) CloseFont(ui_f_tiny);
  ui_f_btn = ui_open_font("LiberationSans-Bold", btn_size);
  ui_f_small = ui_open_font("LiberationSans", btn_size * 8 / 10);
  ui_f_title = ui_open_font("LiberationSans-Bold", btn_size * 9 / 10);
  ui_f_tiny = ui_open_font("LiberationSans-Bold", btn_size * 66 / 100);
  fonts_size = btn_size;
}

void ui_text_center(ifont *f, int color, int x, int y, int w, int h, const char *s) {
  int sw;
  if (!f) return;
  SetFont(f, color);
  sw = StringWidth(s);
  DrawString(x + (w - sw) / 2, y + (h - f->height) / 2, s);
}

void ui_box(int x, int y, int w, int h, int thick, int fill) {
  int i;
  FillArea(x, y, w, h, fill);
  for (i = 0; i < thick; i++) DrawRect(x + i, y + i, w - 2 * i, h - 2 * i, BLACK);
}

void ui_draw_btn(const UiBtn *b) {
  ifont *f = ui_f_btn;
  ui_box(b->x, b->y, b->w, b->h, 2, WHITE);
  SetFont(f, BLACK);
  if (StringWidth(b->label) > b->w - 16) { f = ui_f_small; SetFont(f, BLACK); }
  if (StringWidth(b->label) > b->w - 16) f = ui_f_tiny;
  ui_text_center(f, BLACK, b->x, b->y, b->w, b->h, b->label);
}

int ui_hit(const UiBtn *b, int x, int y) { return x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h; }

/* Code point <-> UTF-8 (letters of the Cyrillic puzzles, the progress file) */
void ui_utf8(unsigned cp, char *buf) {
  if (cp < 0x80) { buf[0] = (char)cp; buf[1] = 0; }
  else if (cp < 0x800) { buf[0] = (char)(0xC0 | (cp >> 6)); buf[1] = (char)(0x80 | (cp & 0x3F)); buf[2] = 0; }
  else { buf[0] = (char)(0xE0 | (cp >> 12)); buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); buf[2] = (char)(0x80 | (cp & 0x3F)); buf[3] = 0; }
}

unsigned ui_cp(const char *s) {
  const unsigned char *u = (const unsigned char *)s;
  if (u[0] < 0x80) return u[0];
  if ((u[0] & 0xE0) == 0xC0 && (u[1] & 0xC0) == 0x80) return ((unsigned)(u[0] & 0x1F) << 6) | (u[1] & 0x3F);
  if ((u[0] & 0xF0) == 0xE0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80)
    return ((unsigned)(u[0] & 0x0F) << 12) | ((unsigned)(u[1] & 0x3F) << 6) | (u[2] & 0x3F);
  return 0;
}
