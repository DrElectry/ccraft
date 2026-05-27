#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

extern uint64_t __servseed;

#define WIDTH 1280
#define HEIGHT 720

extern int __onserv;
extern int __servport;
extern char __servip[256];

#endif