#ifndef FM_H
#define FM_H

typedef struct {
    unsigned int id;
    char *src;
    char *data;
    long size;
} File;

File file_open(const char *src);
int file_exists(const char* src);
File file_create(const char* src);

#endif