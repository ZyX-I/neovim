#define NEOVIM_SRC_VIML_PRINTER_CH_MACROS_H

#ifdef F
# undef F
#endif
#ifdef F_ESCAPED
# undef F_ESCAPED
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
#ifdef FILL
# undef FILL
#endif
#ifdef INDENT
# undef INDENT
#endif

#ifdef CH_MACROS_DEFINE_LENGTH
# define F(f, ...) \
    len += CALL_LEN(f, __VA_ARGS__)
# define F_ESCAPED(f, e, ...) \
    len += 2 * CALL_LEN(f, __VA_ARGS__)
# define FDEC(f, ...) \
    size_t s##f##_len(const CH_MACROS_OPTIONS_TYPE *const o, __VA_ARGS__)
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
# define FILL(c, length) \
    len += length
# define INDENT(length) \
    len += length * STRLEN(o->command.indent);
#else
# ifdef CH_MACROS_DEFINE_FWRITE
#  define _WRITE(s, length) \
    do { \
      if (write_string_len((const char *) s, length, write, cookie) == FAIL) \
        return FAIL; \
    } while (0)
#  define F(f, ...) \
    do { \
      if (f(o, __VA_ARGS__, write, cookie) == FAIL) \
        return FAIL; \
    } while (0)
#  define FDEC(f, ...) \
     int f(const CH_MACROS_OPTIONS_TYPE *const o, __VA_ARGS__, \
           Writer write, void *cookie)
#  define FUNCTION_START
#  define RETURN \
    return OK
#  define FUNCTION_END \
    return OK
#  define ADD_CHAR(c) \
    do { \
      char s[] = {c}; \
      _WRITE(s, 1); \
    } while (0)
#  define ADD_STATIC_STRING(s) \
    _WRITE(s, sizeof(s) - 1)
#  define ADD_STRING(s) \
    _WRITE(s, STRLEN(s))
#  define ADD_STRING_LEN(s, length) \
    _WRITE(s, length)
#  define FILL(c, length) \
    do { \
      char s[length]; \
      memset(s, c, length); \
      _WRITE(s, length); \
    } while (0)
#  define F_ESCAPED(f, e, ...) \
    do { \
      const EscapedCookie new_cookie = { \
        .echars = e, \
        .write = write, \
        .cookie = cookie \
      }; \
      { \
        Writer write = &write_escaped_string_len; \
        void *cookie = (void *) &new_cookie; \
        F(f, __VA_ARGS__); \
      } \
    } while (0)
# else
#  define F(f, ...) \
    s##f(o, __VA_ARGS__, &p)
#  define FDEC(f, ...) \
    void s##f(const CH_MACROS_OPTIONS_TYPE *const o, __VA_ARGS__, char **pp)
#  define FUNCTION_START \
    char *p = *pp
#  define RETURN \
    return
#  define FUNCTION_END \
    *pp = p
#  define ADD_CHAR(c) \
    *p++ = c
#  define ADD_STATIC_STRING(s) \
    memcpy(p, s, sizeof(s) - 1); \
    p += sizeof(s) - 1
#  define ADD_STRING(s) \
    do { \
      size_t len = STRLEN(s); \
      if (len) {\
        memcpy(p, s, len); \
        p += len; \
      } \
    } while (0)
#  define ADD_STRING_LEN(s, length) \
    do { \
      if (length) {\
        memcpy(p, s, length); \
        p += length; \
      } \
    } while (0)
#  define FILL(c, length) \
    memset(p, c, length); \
    p += length
#  define F_ESCAPED(f, e, ...) \
    do { \
      char *arg_start = p; \
      const char *const echars = e; \
      F(f, __VA_ARGS__); \
      while (arg_start < p) { \
        if (strchr(echars, *arg_start) != NULL) { \
          STRMOVE(arg_start + 1, arg_start); \
          *arg_start = '\\'; \
          arg_start += 2; \
          p++; \
        } else { \
          arg_start++; \
        } \
      } \
    } while (0)
# endif
# define INDENT(length) \
    do { \
      const size_t len = STRLEN(o->command.indent); \
      const size_t mlen = length; \
      for (size_t i = 0; i < mlen; i++) \
        ADD_STRING_LEN(o->command.indent, len); \
    } while (0)
#endif

#ifndef SPACES
# define SPACES(length) \
    do { \
      if (length) { \
        FILL(' ', length); \
      } \
    } while (0)
# define OPERATOR_SPACES(op) \
     _OPERATOR_SPACES(o, op)
# define CALL_LEN(f, ...) s##f##_len(o, __VA_ARGS__)

# define SPACES_BEFORE_SUBSCRIPT2(a1, a2) SPACES(o->a1.a2.before_subscript)
# define SPACES_BEFORE_TEXT2(a1, a2)      SPACES(o->a1.a2.before_text)
# define SPACES_BEFORE_ATTRIBUTE2(a1, a2) SPACES(o->a1.a2.before_attribute)
# define SPACES_BEFORE_END2(a1, a2)       SPACES(o->a1.a2.before_end)
# define SPACES_AFTER_START2(a1, a2)      SPACES(o->a1.a2.after_start)

# define SPACES_BEFORE_END3(a1, a2, a3)   SPACES(o->a1.a2.a3.before_end)
# define SPACES_BEFORE3(a1, a2, a3)       SPACES(o->a1.a2.a3.before)
# define SPACES_AFTER_START3(a1, a2, a3)  SPACES(o->a1.a2.a3.after_start)
# define SPACES_AFTER3(a1, a2, a3)        SPACES(o->a1.a2.a3.after)

# define SPACES_BEFORE4(a1, a2, a3, a4)   SPACES(o->a1.a2.a3.a4.before)
# define SPACES_AFTER4(a1, a2, a3, a4)    SPACES(o->a1.a2.a3.a4.after)

# define ADD_TRAILING_COMMA2(a1, a2)      o->a1.a2.trailing_comma
#endif
