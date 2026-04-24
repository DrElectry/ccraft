#include "fm.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int next_id = 1;

File file_open(const char *src) {
    File file;
    file.id = next_id++;
    file.src = strdup(src);
    file.data = NULL;
    file.size = 0;

    ASSERT(file.src != NULL, "Failed to allocate memory for file src: %s", src);

    FILE *fp = fopen(src, "rb");
    ASSERT(fp != NULL, "Failed to open file: %s", src);

    fseek(fp, 0, SEEK_END);
    file.size = ftell(fp);
    rewind(fp);

    ASSERT(file.size >= 0, "Invalid file size for: %s", src);

    file.data = (char *)malloc(file.size + 1);
    ASSERT(file.data != NULL, "Failed to allocate memory for file: %s", src);

    size_t read = fread(file.data, 1, file.size, fp);
    fclose(fp);

    ASSERT(read == file.size, "Failed to read full file: %s (read %zu / %ld)",
           src, read, file.size);

    file.data[file.size] = '\0';

    return file;
}

void file_free(File *file) {
    ASSERT(file != NULL, "file_free called with NULL pointer");

    if (file->data) free(file->data);
    if (file->src) free(file->src);

    file->data = NULL;
    file->src = NULL;
    file->size = 0;
}