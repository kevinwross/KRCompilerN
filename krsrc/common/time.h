#ifndef KRC_TIME_H
#define KRC_TIME_H

#include "stddef.h"

/* Minimal time.h declarations for krc runtime compilation. */

typedef long time_t;

long time(long* t);
long clock(void);

#endif /* KRC_TIME_H */
