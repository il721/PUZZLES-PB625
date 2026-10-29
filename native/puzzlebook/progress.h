/* Progress file: Settings > "Save progress to a file" / "Load progress from a file" (port of the source's
 * "Transfer progress"). One JSON file, /mnt/ext1/puzzlebook-progress.json, in the app's bundle format:
 *   {"bundleVersion": 1, "exportedAt": "...", "namespaces": {"module_rebus": {"puzzles": {"p01": {"cells": {...},
 *    "solved": true, "updatedAt": 1730000000000, ...}}}, ...}}
 * Export writes every touched puzzle of every module that has a namespace. Import merges, never loses a better
 * result: solved beats unsolved; between two unsolved copies the later updatedAt wins; ties keep the local copy,
 * so loading the same file twice changes nothing. Unknown namespaces and puzzles are ignored. */
#ifndef PROGRESS_H
#define PROGRESS_H

enum { PR_OK, PR_NOFILE, PR_BAD, PR_NEWER, PR_WRITE };

int progress_export(void);                          /* PR_OK or PR_WRITE */
int progress_open(char *exported_at, int n);        /* parse the file: PR_OK, PR_NOFILE, PR_BAD or PR_NEWER; date "YYYY-MM-DD HH:MM" */
void progress_apply(int *added, int *updated);      /* merge the opened file into the modules (callers save them) */
void progress_discard(void);

#endif
