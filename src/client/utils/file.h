#ifndef FILE_H
#define FILE_H

#include <stddef.h>

typedef struct {
    unsigned int id;
    char *src;
    char *data;
    long size;
} File;

File file_open(const char *src);
int file_exists(const char* src);
File file_create(const char* src);
int file_write(File *file, const char *data, size_t size);
int file_addwrite(File *file, const char *data, size_t size);

#endif