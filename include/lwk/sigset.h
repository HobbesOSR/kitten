#ifndef _LWK_SIGSET_H
#define _LWK_SIGSET_H

#include <arch/signal.h>

// API for manipulating sigsets
extern void sigset_add(sigset_t *sigset, int signum);
extern void sigset_del(sigset_t *sigset, int signum);
extern bool sigset_test(const sigset_t *sigset, int signum);
extern bool sigset_isempty(const sigset_t *sigset);
extern void sigset_zero(sigset_t *sigset);
extern void sigset_fill(sigset_t *sigset);
extern void sigset_copy(sigset_t *dst, const sigset_t *src);
extern void sigset_or(sigset_t *result, const sigset_t *a, const sigset_t *b);
extern void sigset_and(sigset_t *result, const sigset_t *a, const sigset_t *b);
extern void sigset_nand(sigset_t *result, const sigset_t *a, const sigset_t *b);
extern void sigset_complement(sigset_t *sigset);
extern bool sigset_haspending(sigset_t *sigset, sigset_t *blocked);
extern int  sigset_getnext(sigset_t *sigset, sigset_t *blocked);

#endif
