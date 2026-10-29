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
