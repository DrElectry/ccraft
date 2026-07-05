#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

#include "gl/fbo.h"

extern uint64_t __servseed;

extern int WIDTH;
extern int HEIGHT;

extern int __onserv;
extern int __servport;
extern char __servip[256];
extern char __nickname[32];

#endif