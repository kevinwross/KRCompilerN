/* unwinder.c — Simple frame-pointer-based stack unwinder (plan.md §14.4).
 *
 * Provides krc_unwind_backtrace and krc_unwind_frame for use by the C++
 * runtime and debuggers. This is a minimal implementation that walks the
 * stack using the frame pointer register.
 *
 * KR16: R4 is the frame pointer, SP is the stack pointer.
 * KR32: R13 is the frame pointer, R15 is the stack pointer.
 *
 * On host builds (non-KR targets), these are stubs that return 0 frames.
 */

#include "hal.h"

#include <stddef.h>
#include <stdint.h>

#define KRC_MAX_FRAMES 64

typedef struct {
    void* pc;   /* program counter */
    void* fp;   /* frame pointer */
    void* sp;   /* stack pointer */
} krc_unwind_frame_t;

/* Only the KR targets have the frame-pointer-based unwinder.
 * On other platforms (e.g. host GCC for CI), these are stubs. */
#if defined(__KR16__) || defined(__KR32__)

#  if defined(__KR16__)
#    define KRC_FP_REG 4   /* R4 is FP on KR16 */
#  elif defined(__KR32__)
#    define KRC_FP_REG 13  /* R13 is FP on KR32 */
#  endif

/* Read the current frame pointer from the stack. */
static void* get_frame_pointer(void) {
    void* fp;
#if defined(__KR16__)
    __asm__ volatile ("mov %0, r4" : "=r"(fp));
#elif defined(__KR32__)
    __asm__ volatile ("mov %0, r13" : "=r"(fp));
#endif
    return fp;
}

/* Read the return address from the stack at fp. */
static void* read_return_address(void* fp) {
    void** frame = (void**)fp;
    return frame[0];
}

/* Get the caller's frame pointer from the current frame. */
static void* get_caller_fp(void* fp) {
    void** frame = (void**)fp;
    return frame[-1];
}

#endif /* defined(__KR16__) || defined(__KR32__) */

/* Walk the stack and fill in frames. Returns the number of frames filled. */
int krc_unwind_backtrace(krc_unwind_frame_t* frames, int max_frames) {
    if (!frames || max_frames <= 0) return 0;

#if defined(__KR16__) || defined(__KR32__)
    int count = 0;
    void* fp = get_frame_pointer();

    while (fp && count < max_frames) {
        void* ret = read_return_address(fp);
        frames[count].pc = ret;
        frames[count].fp = fp;
        frames[count].sp = (void*)((char*)fp + sizeof(void*));
        count++;

        fp = get_caller_fp(fp);
    }

    return count;
#else
    (void)max_frames;
    return 0;
#endif
}

/* Get information about a specific frame. */
int krc_unwind_frame(int frame_index, krc_unwind_frame_t* frame) {
    krc_unwind_frame_t frames[KRC_MAX_FRAMES];
    int count = krc_unwind_backtrace(frames, KRC_MAX_FRAMES);
    if (frame_index < 0 || frame_index >= count) return -1;
    *frame = frames[frame_index];
    return 0;
}
