#include "translator/printer/printer.h"

#ifdef F
# undef F
#endif
#ifdef F2
# undef F2
#endif
#ifdef FDEC
# undef FDEC
#endif
#ifdef FUNCTION_START
# undef FUNCTION_START
#endif
#ifdef RETURN
# undef RETURN
#endif
#ifdef FUNCTION_END
# undef FUNCTION_END
#endif
#ifdef ADD_CHAR
# undef ADD_CHAR
#endif
#ifdef ADD_STATIC_STRING
# undef ADD_STATIC_STRING
#endif
#ifdef ADD_STRING
# undef ADD_STRING
#endif
#ifdef ADD_STRING_LEN
# undef ADD_STRING_LEN
#endif
#ifdef MEMSET
# undef MEMSET
#endif
#ifdef SPACES
# undef SPACES
#endif
#ifdef INDENT
# undef INDENT
#endif
#ifdef OPERATOR_SPACES
# undef OPERATOR_SPACES
#endif

#ifdef DEFINE_LENGTH
# define F(f, ...) \
    len += f##_len(po, __VA_ARGS__)
# define F2(f, ...) \
    len += 2 * f##_len(po, __VA_ARGS__)
# define FDEC(f, ...) \
    size_t f##_len(const PrinterOptions *const po, __VA_ARGS__)
# define FUNCTION_START \
    size_t len = 0
# define RETURN \
    return len
# define FUNCTION_END \
    RETURN
# define ADD_CHAR(c) \
    len++
# define ADD_STATIC_STRING(s) \
    len += sizeof(s) - 1
# define ADD_STRING(s) \
    len += STRLEN(s);
# define ADD_STRING_LEN(s, length) \
    len += length;
# define MEMSET(c, length) \
    len += length
# define INDENT(length) \
    len += length * STRLEN(po->command->indent);
#else
# define F(f, ...) \
    f(po, __VA_ARGS__, &p)
# define F2(f, ...) \
    f(po, __VA_ARGS__, &p)
# define FDEC(f, ...) \
    void f(const PrinterOptions *const po, __VA_ARGS__, char **pp)
# define FUNCTION_START \
    char *p = *pp
# define RETURN \
    return
# define FUNCTION_END \
    *pp = p
# define ADD_CHAR(c) \
    *p++ = c
# define ADD_STATIC_STRING(s) \
    memcpy(p, s, sizeof(s) - 1); \
    p += sizeof(s) - 1
# define ADD_STRING(s) \
    { \
      size_t len = STRLEN(s); \
      if (len) {\
        memcpy(p, s, len); \
        p += len; \
      } \
    }
# define ADD_STRING_LEN(s, length) \
    { \
      if (length) {\
        memcpy(p, s, length); \
        p += length; \
      } \
    }
# define MEMSET(c, length) \
    memset(p, c, length); \
    p += length
# define INDENT(length) \
    { \
      size_t i = 0; \
      size_t len = STRLEN(po->command->indent); \
      while (i < length) \
        ADD_STRING_LEN(po->command->indent, len) \
    }
#endif

#define SPACES(length) \
    if (length) { \
      MEMSET(' ', length); \
    }

#define OPERATOR_SPACES(op) \
    _OPERATOR_SPACES(po, op)
