#ifndef KRC_INTRIN_H
#define KRC_INTRIN_H

/* Minimal intrin.h for krc runtime compilation (KR32 target).
 *
 * Declares the compiler intrinsic functions used by the KR32 runtime.
 */

#include "stdint.h"

/* InterlockedCompareExchange: atomic compare-and-swap.
 * Returns the original value of *dest. */
long _InterlockedCompareExchange(long volatile* dest, long exchange, long comparand);

/* InterlockedExchange: atomic exchange.
 * Returns the original value of *dest. */
long _InterlockedExchange(long volatile* dest, long value);

#endif /* KRC_INTRIN_H */
