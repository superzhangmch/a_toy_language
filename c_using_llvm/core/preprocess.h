#ifndef PREPROCESS_H
#define PREPROCESS_H

#include <stddef.h>

typedef struct {
    char *combined_source;
    size_t mapping_count;
    struct LineMap {
        int start_combined_line;
        char *file;
        int start_file_line;
    } *mappings;
} PreprocessResult;

// Preprocess a source file expanding include/include_once directives.
// On success, fills result (caller must free combined_source and mapping strings).
// Returns 0 on success, non-zero on error.
int preprocess_file(const char *path, PreprocessResult *result);

// Map a combined line number to original file and line.
void map_line(const PreprocessResult *res, int combined_line, const char **file, int *line);

void free_preprocess_result(PreprocessResult *res);

#endif
