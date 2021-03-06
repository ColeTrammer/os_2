#ifndef _STDARG_H
#define _STDARG_H 1

#include <bits/va_list.h>

#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v)      __builtin_va_end(v)
#define va_arg(v, l)   __builtin_va_arg(v, l)

#endif /* _STDARG_H */
