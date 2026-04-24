#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>

#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "Assertion failed: %s\nFile: %s\nLine: %d\n", \
                    #cond, __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)

#endif