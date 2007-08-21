/*
 *  linux/init/version.c
 *
 *  Copyright (C) 1992  Theodore Ts'o
 *
 *  May be freely distributed as part of Linux.
 */

#include <lwk/compile.h>
#include <lwk/version.h>

const char lwk_banner[] =
	"LWK version " UTS_RELEASE " (" LWK_COMPILE_BY "@"
	LWK_COMPILE_HOST ") (" LWK_COMPILER ") " UTS_VERSION
	"\n";

