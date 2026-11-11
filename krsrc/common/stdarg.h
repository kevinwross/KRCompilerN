#ifndef KRC_STDARG_H
#define KRC_STDARG_H

/* Minimal stdarg.h for krc runtime compilation. */

typedef char* va_list;

#define va_start(ap, param) ((ap) = (char*)&(param) + sizeof(param))
#define va_arg(ap, type)    (*(type *)((ap) += sizeof(type)) - sizeof(type))
#define va_end(ap)          ((ap) = (char*)0)
#define va_copy(dest, src)  ((dest) = (src))

#endif /* KRC_STDARG_H */
