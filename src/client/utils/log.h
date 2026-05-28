#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>

#define ASSERT(cond, ...) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "Assertion failed: %s\nFile: %s\nLine: %d\n", \
                    #cond, __FILE__, __LINE__); \
            if (*#__VA_ARGS__) { \
                fprintf(stderr, __VA_ARGS__); \
                fprintf(stderr, "\n"); \
            } \
            abort(); \
        } \
    } while (0)

#endif