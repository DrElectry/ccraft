#include "utils/file.h"
#include "utils/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/stat.h>

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


int file_addwrite(File *file, const char *data, size_t size)
{
    if (!file || !file->src || !data) return 0;

    FILE *fp = fopen(file->src, "ab"); // append binary mode
    if (!fp) return 0;

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);

    if (written != size) return 0;

    file->size += written;

    return 1;
}

int file_write(File *file, const char *data, size_t size) {
    ASSERT(file != NULL, "file_write: file is NULL");
    ASSERT(file->src != NULL, "file_write: file src is NULL");

    FILE *fp = fopen(file->src, "wb");
    ASSERT(fp != NULL, "Failed to open file for writing: %s", file->src);

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);

    ASSERT(written == size, "Failed to write full file: %s (written %zu / %zu)",
           file->src, written, size);

    free(file->data);
    file->data = (char *)malloc(size + 1);
    ASSERT(file->data != NULL, "Failed to allocate memory for file buffer");

    memcpy(file->data, data, size);
    file->data[size] = '\0';
    file->size = (long)size;

    return 1;
}

int file_exists(const char* src) {
    FILE *fp = fopen(src, "rb");
    if (fp) {
        fclose(fp);
        return 1;
    }
    return 0;
}

File file_create(const char* src) {
    File file;
    file.id = next_id++;
    file.src = strdup(src);
    file.data = NULL;
    file.size = 0;

    ASSERT(file.src != NULL, "Failed to allocate memory for file src: %s", src);

    FILE *fp = fopen(src, "wb");
    ASSERT(fp != NULL, "Failed to create file: %s", src);

    fclose(fp);

    file.data = (char *)malloc(1);
    ASSERT(file.data != NULL, "Failed to allocate memory for empty file buffer");

    file.data[0] = '\0';
    file.size = 0;

    return file;
}