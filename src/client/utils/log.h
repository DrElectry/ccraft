#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86) // i have to move this out lmao
static const char* get_cpu_model(void) {
    static char cpu_brand[49] = "Unknown CPU";
    int cpu_info[4] = {0};
    __asm__ __volatile__ (
        "cpuid"
        : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]), "=d"(cpu_info[3])
        : "a"(0x80000002), "c"(0)
    );
    memcpy(cpu_brand, cpu_info, sizeof(cpu_info));
    __asm__ __volatile__ (
        "cpuid"
        : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]), "=d"(cpu_info[3])
        : "a"(0x80000003), "c"(0)
    );
    memcpy(cpu_brand + 16, cpu_info, sizeof(cpu_info));
    __asm__ __volatile__ (
        "cpuid"
        : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]), "=d"(cpu_info[3])
        : "a"(0x80000004), "c"(0)
    );
    memcpy(cpu_brand + 32, cpu_info, sizeof(cpu_info));
    cpu_brand[48] = '\0';
    char* start = cpu_brand;
    while (*start == ' ') start++;
    return start;
}
#else
static const char* get_cpu_model(void) {
    return "Unknown CPU";
}
#endif

#endif