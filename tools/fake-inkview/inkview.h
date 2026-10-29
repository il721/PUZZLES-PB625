/* Minimal stand-in for the PocketBook inkview.h, only what native/rebus/main.c uses.
 * Lets the UI run on the build host and render to SVG (see inkview_svg.c, tools/test-rebus-ui.sh). */
#ifndef FAKE_INKVIEW_H
#define FAKE_INKVIEW_H

#define EVT_INIT 21
#define EVT_EXIT 22
#define EVT_SHOW 23
#define EVT_KEYPRESS 25
#define EVT_POINTERUP 29
#define EVT_ORIENTATION 32
#define EVT_SAVESTATE 155

#define KEY_MENU 0x17
#define KEY_PREV 0x18
#define KEY_NEXT 0x19
#define KEY_HOME 0x1a
#define KEY_BACK 0x1b

#define BLACK 0x000000
#define WHITE 0xffffff
#define ALIGN_LEFT 1
#define ALIGN_CENTER 2
#define VALIGN_MIDDLE 32

typedef struct ifont_s {
  char *name; char *family; int size;
  unsigned char aa, isbold, isitalic, _r1;
  unsigned short charset, _r2;
  int color, height, linespacing, baseline;
  void *fdata;
} ifont;

typedef int (*iv_handler)(int type, int par1, int par2);

void InkViewMain(iv_handler h);
void CloseApp(void);
int ScreenWidth(void);
int ScreenHeight(void);
void SetOrientation(int n);
void ClearScreen(void);
void DrawLine(int x1, int y1, int x2, int y2, int color);
void DrawRect(int x, int y, int w, int h, int color);
void FillArea(int x, int y, int w, int h, int color);
ifont *OpenFont(const char *name, int size, int aa);
void CloseFont(ifont *f);
void SetFont(ifont *font, int color);
void DrawString(int x, int y, const char *s);
char *DrawTextRect(int x, int y, int w, int h, const char *s, int flags);
int StringWidth(const char *s);
void FullUpdate(void);
void PartialUpdate(int x, int y, int w, int h);

#endif
