#ifndef KEVINC_PP_INCLUDE_H
#define KEVINC_PP_INCLUDE_H

#define ID(x) x
#define CAT(a, b) a##b
#define STR(x) #x
#define VALUE 5

#if defined(VALUE) && (VALUE > 3)
#define SELECTED 1
#else
#define SELECTED 0
#endif

#endif
