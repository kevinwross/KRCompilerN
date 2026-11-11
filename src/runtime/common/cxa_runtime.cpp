/* cxa_runtime.cpp — Itanium C++ ABI __cxa_* runtime functions (plan.md §14.4).
 *
 * Minimal implementation for v1.0. Exception handling is not yet supported;
 * these functions abort when called. Provided so that C++ code referencing
 * them links successfully.
 */

#include <cstdlib>
#include <cstdio>

#ifdef _MSC_VER
#pragma warning(disable: 4702)  // unreachable code after abort()
#endif

extern "C" {

/* __cxa_allocate_exception — allocate exception object storage. */
void* __cxa_allocate_exception(unsigned int thrown_size) {
    (void)thrown_size;
    std::fprintf(stderr, "__cxa_allocate_exception: exceptions not supported\n");
    std::abort();
    return nullptr;
}

/* __cxa_free_exception — free exception object storage. */
void __cxa_free_exception(void* thrown_exception) {
    (void)thrown_exception;
    std::fprintf(stderr, "__cxa_free_exception: exceptions not supported\n");
    std::abort();
}

/* __cxa_throw — throw an exception. Never returns. */
[[noreturn]] void __cxa_throw(void* thrown_exception, void* tinfo,
                              void (*dest)(void*)) {
    (void)thrown_exception;
    (void)tinfo;
    (void)dest;
    std::fprintf(stderr, "__cxa_throw: exceptions not supported\n");
    std::abort();
}

/* __cxa_begin_catch — begin handling a caught exception. */
void* __cxa_begin_catch(void* thrown_exception) {
    (void)thrown_exception;
    std::fprintf(stderr, "__cxa_begin_catch: exceptions not supported\n");
    std::abort();
    return nullptr;
}

/* __cxa_end_catch — end handling a caught exception. */
void __cxa_end_catch(void) {
    std::fprintf(stderr, "__cxa_end_catch: exceptions not supported\n");
    std::abort();
}

/* __cxa_rethrow — rethrow the current exception. Never returns. */
[[noreturn]] void __cxa_rethrow(void) {
    std::fprintf(stderr, "__cxa_rethrow: exceptions not supported\n");
    std::abort();
}

/* __cxa_uncaught_exception — return true if an exception is uncaught. */
int __cxa_uncaught_exception(void) {
    return 0;
}

/* __cxa_deleted_virtual — called for deleted virtual function calls. */
[[noreturn]] void __cxa_deleted_virtual(void) {
    std::fprintf(stderr, "__cxa_deleted_virtual: deleted virtual called\n");
    std::abort();
}

/* __cxa_pure_virtual — called for pure virtual function calls. */
[[noreturn]] void __cxa_pure_virtual(void) {
    std::fprintf(stderr, "__cxa_pure_virtual: pure virtual called\n");
    std::abort();
}

/* __cxa_bad_cast — thrown for failed dynamic_cast. */
[[noreturn]] void __cxa_bad_cast(void) {
    std::fprintf(stderr, "__cxa_bad_cast: bad cast\n");
    std::abort();
}

/* __cxa_bad_typeid — thrown for bad typeid. */
[[noreturn]] void __cxa_bad_typeid(void) {
    std::fprintf(stderr, "__cxa_bad_typeid: bad typeid\n");
    std::abort();
}

} /* extern "C" */
