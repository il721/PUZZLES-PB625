/* Minimal stand-in for the PocketBook inkview.h: only what apps/tatham uses (see ../fake_inkview.c). */
#ifndef TAT_FAKE_INKVIEW_H
#define TAT_FAKE_INKVIEW_H

#include <stdbool.h>

#define EVT_INIT 21
#define EVT_EXIT 22
#define EVT_SHOW 23
#define EVT_KEYPRESS 25
#define EVT_POINTERUP 29
#define EVT_POINTERDOWN 30
#define EVT_POINTERMOVE 31
#define EVT_ORIENTATION 32
#define EVT_POINTERLONG 34
#define EVT_POINTERDRAG 44

#define IV_KEY_OK 0x0a
#define IV_KEY_MENU 0x17
#define IV_KEY_PREV 0x18
#define IV_KEY_NEXT 0x19
#define IV_KEY_HOME 0x1a
#define IV_KEY_BACK 0x1b

#define BLACK 0x000000
#define DGRAY 0x555555
#define LGRAY 0xaaaaaa
#define WHITE 0xffffff

#define ALIGN_LEFT 1
#define ALIGN_CENTER 2
#define ALIGN_RIGHT 4
#define VALIGN_TOP 16
#define VALIGN_MIDDLE 32
#define VALIGN_BOTTOM 64
#define DOTS 512

#define KBD_NORMAL 0
#define PANEL_DISABLED 0

typedef struct ibitmap_s {
    unsigned short width, height, depth, scanline;
    unsigned char data[];
} ibitmap;

typedef struct ifont_s {
    char *name;
    int size;
    int height;
    int baseline;
    void *fdata;
} ifont;

typedef int (*iv_handler)(int type, int par1, int par2);
typedef void (*iv_keyboardhandler)(char *text);

void InkViewMain(iv_handler h);
void CloseApp(void);
int ScreenWidth(void);
int ScreenHeight(void);
void FullUpdate(void);
void PartialUpdate(int x, int y, int w, int h);
void SetClip(int x, int y, int w, int h);
void DrawLine(int x1, int y1, int x2, int y2, int color);
void FillArea(int x, int y, int w, int h, int color);
ifont *OpenFont(const char *name, int size, int aa);
void CloseFont(ifont *f);
void SetFont(const ifont *font, int color);
int StringWidth(const char *s);
int TextRectHeight(int width, const char *s, int flags);
char *DrawTextRect(int x, int y, int w, int h, const char *s, int flags);
ibitmap *BitmapFromScreen(int x, int y, int w, int h);
void DrawBitmap(int x, int y, const ibitmap *b);
#define ICON_INFORMATION 1
#define ICON_ERROR 4
void Message(int icon, const char *title, const char *text, int timeout);
void OpenKeyboard(const char *title, char *buffer, int maxlen, int flags, iv_keyboardhandler hproc);
void SetPanelType(int type);
void SetOrientation(int n);

#endif
