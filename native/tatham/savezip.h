/* Export / import of all saved puzzles as one zip file (see savezip.c). The same layout is used by the
 * Android port of the collection (il721/meizu-m6-note-m721-h), so progress can be moved between devices. */
#ifndef TAT_SAVEZIP_H
#define TAT_SAVEZIP_H

#ifndef TAT_ZIP
#define TAT_ZIP "/mnt/ext1/tatham-saves.zip"
#endif

/* Writes TAT_ZIP with <slug>.sav and <slug>.st of every puzzle that has a save. Returns the number of puzzles
 * written, or -1 if the file cannot be written. */
int savezip_export(void);
/* Restores saves from TAT_ZIP into the data folder. Returns the number of puzzles restored, -1 if there is no
 * readable zip, -2 if it only holds compressed entries (only uncompressed "stored" ones are supported). */
int savezip_import(void);

#endif
