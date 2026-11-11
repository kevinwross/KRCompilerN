#ifndef KRC_RUNTIME_UNWINDER_H
#define KRC_RUNTIME_UNWINDER_H

/* unwinder.h — Stack unwinder interface (plan.md §14.4).
 *
 * Provides frame-pointer-based stack walking for the C++ runtime
 * and debuggers. On non-KR targets these are stubs.
 */

#include <stddef.h>

#define KRC_MAX_FRAMES 64

typedef struct {
    void* pc;   /* program counter */
    void* fp;   /* frame pointer */
    void* sp;   /* stack pointer */
} krc_unwind_frame_t;

/* Walk the stack and fill in frames. Returns the number of frames filled. */
int krc_unwind_backtrace(krc_unwind_frame_t* frames, int max_frames);

/* Get information about a specific frame by index. Returns 0 on success,
 * -1 if the index is out of range. */
int krc_unwind_frame(int frame_index, krc_unwind_frame_t* frame);

#endif /* KRC_RUNTIME_UNWINDER_H */
