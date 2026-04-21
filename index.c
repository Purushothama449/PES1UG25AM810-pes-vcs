// index.c — FINAL SAFE VERSION (no segfault)

#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

// forward declaration
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

// ─── PROVIDED ────────────────────────────────────────────────────────────────

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int remaining = index->count - i - 1;
            if (remaining > 0)
                memmove(&index->entries[i], &index->entries[i + 1],
                        remaining * sizeof(IndexEntry));
            index->count--;
            return index_save(index);
        }
    }
    return -1;
}

int index_status(const Index *index) {
    printf("Staged changes:\n");
    if (index->count == 0) printf("  (nothing to show)\n");

    for (int i = 0; i < index->count; i++) {
        printf("  staged:     %s\n", index->entries[i].path);
    }

    printf("\nUnstaged changes:\n");
    printf("  (nothing to show)\n");

    printf("\nUntracked files:\n");
    printf("  (nothing to show)\n\n");

    return 0;
}

// ─── IMPLEMENTATION ─────────────────────────────────────────────────────────

int index_load(Index *index) {
    index->count = 0;

    FILE *f = fopen(INDEX_FILE, "r");
    if (!f) return 0;

    while (1) {
        IndexEntry e = {0};
        char hash_hex[HASH_HEX_SIZE + 1];

        int rc = fscanf(f, "%o %64s %lu %u %s",
                        &e.mode, hash_hex,
                        &e.mtime_sec, &e.size,
                        e.path);

        if (rc != 5) break;

        if (hex_to_hash(hash_hex, &e.hash) != 0) {
            fclose(f);
            return -1;
        }

        if (index->count < MAX_INDEX_ENTRIES)
            index->entries[index->count++] = e;
    }

    fclose(f);
    return 0;
}

// 🔴 FIXED index_save (this was crashing)
int index_save(const Index *index) {
    if (!index) return -1;

    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", INDEX_FILE);

    FILE *f = fopen(tmp_path, "w");
    if (!f) return -1;

    for (int i = 0; i < index->count; i++) {

        if (i >= MAX_INDEX_ENTRIES) break;

        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&index->entries[i].hash, hex);

        if (fprintf(f, "%o %s %lu %u %s\n",
                    index->entries[i].mode,
                    hex,
                    index->entries[i].mtime_sec,
                    index->entries[i].size,
                    index->entries[i].path) < 0) {
            fclose(f);
            return -1;
        }
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    return rename(tmp_path, INDEX_FILE);
}

int index_add(Index *index, const char *path) {

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size < 0) {
        fclose(f);
        return -1;
    }

    void *data = NULL;

    if (size > 0) {
        data = malloc(size);
        if (!data) {
            fclose(f);
            return -1;
        }

        if (fread(data, 1, size, f) != (size_t)size) {
            free(data);
            fclose(f);
            return -1;
        }
    }

    fclose(f);

    ObjectID id;
    if (object_write(OBJ_BLOB, data, size, &id) != 0) {
        if (data) free(data);
        return -1;
    }

    if (data) free(data);

    struct stat st;
    if (stat(path, &st) != 0) return -1;

    if (index->count < 0 || index->count >= MAX_INDEX_ENTRIES)
        return -1;

    IndexEntry *e = index_find(index, path);
    if (!e) {
        e = &index->entries[index->count++];
    }

    e->mode = st.st_mode;
    e->mtime_sec = st.st_mtime;
    e->size = st.st_size;
    e->hash = id;

    snprintf(e->path, sizeof(e->path), "%s", path);

    return index_save(index);
}
// Phase 3: start index implementation
// Phase 3: implemented index_load
// Phase 3: implemented index_save
