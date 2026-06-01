#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

extern uint64_t __servseed;

#define WIDTH 800
#define HEIGHT 600

extern int __onserv;
extern int __servport;
extern char __servip[256];
extern char __nickname[32];

#endif