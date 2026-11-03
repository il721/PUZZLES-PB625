/* Minimal zip writer and reader for the puzzle saves. Entries are stored (not compressed): <slug>.sav is the
 * engine's save file, <slug>.st one byte '1' (solved) or '0'. Both ends of the transfer write stored entries. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tat.h"
#include "savezip.h"

static uint32_t crc_table[256];

static uint32_t crc32_of(const unsigned char *p, size_t n)
{
    uint32_t c = 0xffffffffu;
    size_t i;
    int k;
    if (!crc_table[1]) {
        uint32_t v;
        for (i = 0; i < 256; i++) {
            v = (uint32_t)i;
            for (k = 0; k < 8; k++) v = (v & 1) ? 0xedb88320u ^ (v >> 1) : v >> 1;
            crc_table[i] = v;
        }
    }
    for (i = 0; i < n; i++) c = crc_table[(c ^ p[i]) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void put32(FILE *f, uint32_t v) { put16(f, v & 0xffff); put16(f, v >> 16); }
static unsigned get16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static uint32_t get32(const unsigned char *p) { return get16(p) | ((uint32_t)get16(p + 2) << 16); }

/* whole file into memory (NULL if missing or empty) */
static unsigned char *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    unsigned char *buf;
    long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 64L * 1024 * 1024) { fclose(f); return NULL; }
    buf = malloc((size_t)n);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    *len = buf ? (size_t)n : 0;
    return buf;
}

struct cdent { char name[64]; uint32_t crc, size, off; };

static void write_entry(FILE *f, struct cdent *e, const char *name, const unsigned char *data, size_t n)
{
    snprintf(e->name, sizeof e->name, "%s", name);
    e->crc = crc32_of(data, n);
    e->size = (uint32_t)n;
    e->off = (uint32_t)ftell(f);
    put32(f, 0x04034b50u); put16(f, 20); put16(f, 0); put16(f, 0);      /* sig, version, flags, method = stored */
    put16(f, 0); put16(f, 0x21);                                        /* time 00:00, date 1980-01-01 */
    put32(f, e->crc); put32(f, e->size); put32(f, e->size);
    put16(f, (unsigned)strlen(name)); put16(f, 0);
    fwrite(name, 1, strlen(name), f);
    fwrite(data, 1, n, f);
}

int savezip_export(void)
{
    struct cdent ents[2 * 64];
    int ne = 0, np = 0, i;
    uint32_t cd_off, cd_size;
    char path[300], name[80], tmp[320];
    FILE *f;
    snprintf(tmp, sizeof tmp, "%s.part", TAT_ZIP);
    f = fopen(tmp, "wb");
    if (!f) return -1;
    for (i = 0; i < gamecount && ne + 2 <= 2 * 64; i++) {
        const char *slug = gamelist[i]->htmlhelp_topic;
        size_t n;
        unsigned char *d;
        unsigned char st = '0';
        snprintf(path, sizeof path, "%s/%s.sav", TAT_DIR, slug);
        d = slurp(path, &n);
        if (!d) continue;
        snprintf(name, sizeof name, "%s.sav", slug);
        write_entry(f, &ents[ne++], name, d, n);
        free(d);
        snprintf(path, sizeof path, "%s/%s.st", TAT_DIR, slug);
        d = slurp(path, &n);
        if (d) { st = d[0] == '1' ? '1' : '0'; free(d); }
        snprintf(name, sizeof name, "%s.st", slug);
        write_entry(f, &ents[ne++], name, &st, 1);
        np++;
    }
    cd_off = (uint32_t)ftell(f);
    for (i = 0; i < ne; i++) {
        struct cdent *e = &ents[i];
        put32(f, 0x02014b50u); put16(f, 20); put16(f, 20); put16(f, 0); put16(f, 0);
        put16(f, 0); put16(f, 0x21);
        put32(f, e->crc); put32(f, e->size); put32(f, e->size);
        put16(f, (unsigned)strlen(e->name)); put16(f, 0); put16(f, 0);   /* name, extra, comment lengths */
        put16(f, 0); put16(f, 0); put32(f, 0); put32(f, e->off);         /* disk, internal attr, external attr, offset */
        fwrite(e->name, 1, strlen(e->name), f);
    }
    cd_size = (uint32_t)ftell(f) - cd_off;
    put32(f, 0x06054b50u); put16(f, 0); put16(f, 0); put16(f, (unsigned)ne); put16(f, (unsigned)ne);
    put32(f, cd_size); put32(f, cd_off); put16(f, 0);
    if (fclose(f) != 0) { remove(tmp); return -1; }
    remove(TAT_ZIP);
    if (rename(tmp, TAT_ZIP) != 0) return -1;
    return np;
}

static int known_slug(const char *s, size_t n)
{
    int i;
    for (i = 0; i < gamecount; i++)
        if (strlen(gamelist[i]->htmlhelp_topic) == n && memcmp(gamelist[i]->htmlhelp_topic, s, n) == 0) return 1;
    return 0;
}

static int write_file(const char *path, const unsigned char *d, size_t n)
{
    char tmp[320];
    FILE *f;
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    f = fopen(tmp, "wb");
    if (!f) return 0;
    if (fwrite(d, 1, n, f) != n) { fclose(f); remove(tmp); return 0; }
    if (fclose(f) != 0) { remove(tmp); return 0; }
    return rename(tmp, path) == 0;
}

int savezip_import(void)
{
    size_t len, pos = 0;
    unsigned char *z = slurp(TAT_ZIP, &len);
    int restored = 0, compressed = 0;
    if (!z) return -1;
    while (pos + 30 <= len && get32(z + pos) == 0x04034b50u) {
        unsigned flags = get16(z + pos + 6), method = get16(z + pos + 8);
        uint32_t crc = get32(z + pos + 14), csize = get32(z + pos + 18), usize = get32(z + pos + 22);
        unsigned nl = get16(z + pos + 26), xl = get16(z + pos + 28);
        size_t data = pos + 30 + nl + xl;
        const char *name = (const char *)z + pos + 30;
        if (data > len || (flags & 8)) break;                           /* sizes after the data: not written by us */
        if (data + csize > len) break;
        pos = data + csize;
        if (nl > 4 && nl < 60 && memcmp(name + nl - 4, ".sav", 4) == 0 && known_slug(name, nl - 4)) {
            char slug[64], path[300];
            if (method != 0) { compressed++; continue; }
            if (usize != csize || csize == 0 || crc32_of(z + data, csize) != crc) continue;     /* damaged */
            memcpy(slug, name, nl - 4);
            slug[nl - 4] = 0;
            snprintf(path, sizeof path, "%s/%s.sav", TAT_DIR, slug);
            if (!write_file(path, z + data, csize)) continue;
            snprintf(path, sizeof path, "%s/%s.st", TAT_DIR, slug);
            remove(path);                                              /* replaced below if the zip has one */
            restored++;
        }
    }
    /* second pass for the .st flags (they come after their .sav) */
    pos = 0;
    while (pos + 30 <= len && get32(z + pos) == 0x04034b50u) {
        unsigned flags = get16(z + pos + 6), method = get16(z + pos + 8);
        uint32_t csize = get32(z + pos + 18);
        unsigned nl = get16(z + pos + 26), xl = get16(z + pos + 28);
        size_t data = pos + 30 + nl + xl;
        const char *name = (const char *)z + pos + 30;
        if (data > len || (flags & 8) || data + csize > len) break;
        pos = data + csize;
        if (method == 0 && csize == 1 && nl > 3 && nl < 60 && memcmp(name + nl - 3, ".st", 3) == 0 && known_slug(name, nl - 3)) {
            char slug[64], path[300];
            FILE *f2;
            memcpy(slug, name, nl - 3);
            slug[nl - 3] = 0;
            snprintf(path, sizeof path, "%s/%s.sav", TAT_DIR, slug);
            f2 = fopen(path, "rb");
            if (f2) {                                                  /* only for a puzzle that has a save */
                fclose(f2);
                snprintf(path, sizeof path, "%s/%s.st", TAT_DIR, slug);
                write_file(path, z + data, 1);
            }
        }
    }
    free(z);
    if (restored == 0 && compressed > 0) return -2;
    return restored;
}
