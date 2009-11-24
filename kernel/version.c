/*
 *  linux/init/version.c
 *
 *  Copyright (C) 1992  Theodore Ts'o
 *
 *  May be freely distributed as part of Linux.
 */

#include <lwk/compile.h>
#include <lwk/version.h>
#include <lwk/uts.h>
#include <lwk/utsname.h>

const char lwk_banner[] =
	"LWK version " UTS_RELEASE " (" LWK_COMPILE_BY "@"
	LWK_COMPILE_HOST ") (" LWK_COMPILER ") " UTS_VERSION
	"\n";

/**
 * User-level apps call the uname() system call to figure out basic
 * information about the system they are running on, as indicated
 * by this structure.  We report back that we are Linux since that
 * is what standard Linux executables expect (UTS_LINUX_SYSNAME and
 * UTS_LINUX_RELEASE).
 */
struct utsname linux_utsname = {
	.sysname	=	UTS_LINUX_SYSNAME,
	.nodename	=	UTS_NODENAME,
	.release	=	UTS_LINUX_RELEASE,
	.version	=	UTS_VERSION,
	.machine	=	UTS_MACHINE,
	.domainname	=	UTS_DOMAINNAME
};

