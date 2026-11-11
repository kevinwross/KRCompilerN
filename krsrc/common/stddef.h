#ifndef KRC_STDDEF_H
#define KRC_STDDEF_H

/* Minimal stddef.h for krc runtime compilation. */

typedef unsigned int size_t;
typedef int ptrdiff_t;

#define offsetof(type, member) ((size_t)&((type *)0)->member)
#define NULL ((void *)0)

#endif /* KRC_STDDEF_H */
