#include "preprocess.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

typedef struct {
    char **data;
    int size;
    int cap;
} StringVec;

static void sv_init(StringVec *v) { v->data = NULL; v->size = v->cap = 0; }
static void sv_free(StringVec *v) {
    for (int i = 0; i < v->size; i++) free(v->data[i]);
    free(v->data);
}
static int sv_contains(StringVec *v, const char *s) {
    for (int i = 0; i < v->size; i++) {
        if (strcmp(v->data[i], s) == 0) return 1;
    }
    return 0;
}
static void sv_push(StringVec *v, const char *s) {
    if (v->size >= v->cap) {
        v->cap = v->cap == 0 ? 8 : v->cap * 2;
        v->data = realloc(v->data, v->cap * sizeof(char*));
    }
    v->data[v->size++] = strdup(s);
}

static char* resolve_path(const char *base_file, const char *target) {
    char buf[PATH_MAX];
    if (target[0] == '/') {
        strncpy(buf, target, sizeof(buf));
        buf[sizeof(buf)-1] = '\0';
    } else {
        const char *slash = strrchr(base_file, '/');
        if (slash) {
            size_t dir_len = slash - base_file + 1;
            if (dir_len >= sizeof(buf)) dir_len = sizeof(buf) - 1;
            strncpy(buf, base_file, dir_len);
            buf[dir_len] = '\0';
            strncat(buf, target, sizeof(buf) - dir_len - 1);
        } else {
            strncpy(buf, target, sizeof(buf));
            buf[sizeof(buf)-1] = '\0';
        }
    }
    // Try realpath to canonicalize
    char *resolved = realpath(buf, NULL);
    if (resolved) return resolved;
    return strdup(buf);
}

static void add_mapping(PreprocessResult *res, int combined_start, const char *file, int file_start) {
    res->mappings = realloc(res->mappings, (res->mapping_count + 1) * sizeof(*res->mappings));
    res->mappings[res->mapping_count].start_combined_line = combined_start;
    res->mappings[res->mapping_count].file = strdup(file);
    res->mappings[res->mapping_count].start_file_line = file_start;
    res->mapping_count++;
}

static void append_line(char **buf, size_t *cap, size_t *len, const char *line) {
    size_t l = strlen(line);
    if (*len + l + 2 >= *cap) {
        *cap = (*cap + l + 1024);
        *buf = realloc(*buf, *cap);
    }
    memcpy(*buf + *len, line, l);
    *len += l;
    (*buf)[*len] = '\n';
    (*buf)[*len + 1] = '\0';
    *len += 1;
}

static int preprocess_internal(const char *path, PreprocessResult *res, StringVec *once_set, StringVec *stack, char **buf, size_t *cap, size_t *len, int *combined_line) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Failed to open include file: %s\n", path);
        return -1;
    }

    if (sv_contains(stack, path)) {
        fprintf(stderr, "Include cycle detected at %s\n", path);
        fclose(f);
        return -1;
    }
    sv_push(stack, path);
    add_mapping(res, *combined_line, path, 1);

    char linebuf[4096];
    while (fgets(linebuf, sizeof(linebuf), f) != NULL) {
        // strip trailing newline for processing
        size_t ll = strlen(linebuf);
        if (ll > 0 && linebuf[ll-1] == '\n') linebuf[ll-1] = '\0';

        char *p = linebuf;
        while (*p && isspace((unsigned char)*p)) p++;

        int is_include = 0, is_once = 0;
        if (strncmp(p, "include_once", 12) == 0 && isspace((unsigned char)p[12])) { is_include = 1; is_once = 1; p += 12; }
        else if (strncmp(p, "include", 7) == 0 && isspace((unsigned char)p[7])) { is_include = 1; p += 7; }
        if (is_include) {
            while (*p && isspace((unsigned char)*p)) p++;
            char fname[1024]; int idx = 0;
            if (*p == '"' || *p == '\'') {
                char quote = *p++;
                while (*p && *p != quote && idx < 1023) fname[idx++] = *p++;
                fname[idx] = '\0';
                if (*p != quote) {
                    fprintf(stderr, "Invalid include path near line %d in %s\n", *combined_line, path);
                    fclose(f);
                    return -1;
                }
            } else {
                while (*p && !isspace((unsigned char)*p) && *p != '#' && idx < 1023) {
                    fname[idx++] = *p++;
                }
                fname[idx] = '\0';
                if (idx == 0) {
                    fprintf(stderr, "Invalid include path near line %d in %s\n", *combined_line, path);
                    fclose(f);
                    return -1;
                }
            }
            char *full = resolve_path(path, fname);
            if (!(is_once && sv_contains(once_set, full))) {
                if (is_once) sv_push(once_set, full);
                if (preprocess_internal(full, res, once_set, stack, buf, cap, len, combined_line) != 0) {
                    free(full);
                    fclose(f);
                    return -1;
                }
            }
            free(full);
            continue; // do not count this line itself
        }

        append_line(buf, cap, len, linebuf);
        (*combined_line)++;
    }

    stack->size--;
    fclose(f);
    return 0;
}

int preprocess_file(const char *path, PreprocessResult *result) {
    memset(result, 0, sizeof(*result));
    StringVec once_set, stack;
    sv_init(&once_set);
    sv_init(&stack);
    size_t cap = 1024, len = 0;
    result->combined_source = malloc(cap);
    result->combined_source[0] = '\0';
    int combined_line = 1;
    int ret = preprocess_internal(path, result, &once_set, &stack, &result->combined_source, &cap, &len, &combined_line);
    sv_free(&once_set);
    sv_free(&stack);
    if (ret != 0) {
        free_preprocess_result(result);
    }
    return ret;
}

void map_line(const PreprocessResult *res, int combined_line, const char **file, int *line) {
    const char *f = "<unknown>";
    int l = combined_line;
    for (size_t i = 0; i < res->mapping_count; i++) {
        if (combined_line >= res->mappings[i].start_combined_line) {
            f = res->mappings[i].file;
            l = res->mappings[i].start_file_line + (combined_line - res->mappings[i].start_combined_line);
        } else {
            break;
        }
    }
    *file = f;
    *line = l;
}

void free_preprocess_result(PreprocessResult *res) {
    if (!res) return;
    free(res->combined_source);
    for (size_t i = 0; i < res->mapping_count; i++) {
        free(res->mappings[i].file);
    }
    free(res->mappings);
    res->combined_source = NULL;
    res->mappings = NULL;
    res->mapping_count = 0;
}
