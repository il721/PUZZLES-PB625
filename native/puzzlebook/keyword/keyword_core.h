/* Keyword Rebuses: puzzle model and rules (no inkview, builds on any host).
 *
 * Port of packages/module_keyword_rebus (il721/puzzles): a 3x3 grid of words made of ten different Cyrillic letters;
 * every letter stands for one digit (same letter = same digit, different letters = different digits), a word of
 * two or more letters never starts with 0. Row r is the equation `w[r][0] op w[r][1] = w[r][2]` with the ops
 * x, +, + for rows 0..2; column c is `w[0][c] op w[1][c] = w[2][c]` with the ops +, x, - for columns 0..2. The ten
 * letters ordered by their digits 0..9 spell the book's keyword (shown on a win; never matched against).
 * The player places digits on letters; a digit may be given to two letters at once (both are flagged, never
 * refused). Win = all ten letters have a digit, all digits differ, no leading zero, all six equations hold (any such
 * assignment wins; every puzzle has exactly one).
 */
#ifndef KEYWORD_CORE_H
#define KEYWORD_CORE_H

#include <stdint.h>

#define KW_LETTERS 10

typedef struct {
  const char *id;
  int tutorial;
  uint16_t let[KW_LETTERS];  /* the ten letters (Unicode code points) in order of first appearance, row-major */
  uint8_t len[3][3];         /* letters in word (row, col): 1..3 */
  uint8_t w[3][3][3];        /* word (row, col): letter indices into let[], left to right */
  uint8_t ans[KW_LETTERS];   /* the book's digit of each letter (tests only: the board never looks at it) */
} KwPuzzle;

typedef struct {
  const KwPuzzle *p;
  int8_t val[KW_LETTERS];    /* digit of each letter, -1 = none */
  int8_t sel;                /* selected letter, -1 = none */
} KwBoard;

enum { KW_INCOMPLETE, KW_OK, KW_WRONG };

typedef struct {
  int set;                       /* letters that hold a digit */
  uint8_t eq[6];                 /* KW_INCOMPLETE / KW_OK / KW_WRONG for rows 0..2, then columns 0..2 */
  uint8_t conflict[KW_LETTERS];  /* 1 = the letter shares its digit with another letter */
  uint8_t holders[10];           /* how many letters hold digit d */
  int8_t holder[10];             /* the letter holding digit d when it is exactly one, else -1 (the keyword strip) */
  uint8_t lead0[3][3];           /* 1 = a word of 2+ letters whose first letter holds 0 */
  int nconflict, nlead0, nwrong; /* letters in conflict, words with a leading zero, equations that fail */
  int solved;
} KwStatus;

void kw_init(KwBoard *b, const KwPuzzle *p);
void kw_reset(KwBoard *b);                          /* no digits, no selection */
/* Gives letter (0..9) the digit (0..9), or removes its digit for digit = -1. 0 (nothing changes) for bad arguments. */
int kw_set(KwBoard *b, int letter, int digit);
void kw_status(const KwBoard *b, KwStatus *st);
int kw_wins(const KwBoard *b);
/* The value of word (row, col), or -1 while one of its letters has no digit. */
int kw_word_value(const KwBoard *b, int row, int col);
/* Operator of row r / column c: '*', '+' or '-'. */
int kw_row_op(int r);
int kw_col_op(int c);
/* The keyword the board spells: the letters (code points) ordered by their digit; only meaningful for a solved board.
 * Returns 1 if every digit 0..9 is held by exactly one letter (out[d] = the letter), else 0 and out is zeroed. */
int kw_keyword(const KwBoard *b, uint16_t out[10]);

/* Save format: 10 chars, letter i = '0'..'9' or '-' for none. Buffer needs KW_SAVE_LEN + 1 bytes. */
#define KW_SAVE_LEN KW_LETTERS
void kw_save(const KwBoard *b, char *out);
void kw_load(KwBoard *b, const char *s); /* tolerant: bad chars leave the letter empty */

#endif
