#ifndef NVIM_API_PRIVATE_OBJ_STREAM_H
#define NVIM_API_PRIVATE_OBJ_STREAM_H

#include <stdbool.h>
#include <stddefs.h>

#include <mpack.h>

#include "nvim/api/private/defs.h"
#include "nvim/rbuffer.h"

typedef void *ObjStreamOut;

/// Common header for all “out” streams
#define OSTREAM_OUT_HEADER \
  bool (*push_dictionary_start)(ObjStreamOut out, size_t len); \
  bool (*push_array_start)(ObjStreamOut out, size_t len); \
  bool (*push_string_start)(ObjStreamOut out, size_t len); \
  bool (*push_string_part)(ObjStreamOut out, const char *s, size_t len); \
  bool (*push_string)(ObjStreamOut out, String s); \
  bool (*push_nil)(ObjStreamOut out); \
  bool (*push_integer)(ObjStreamOut out, Integer n); \
  bool (*push_float)(ObjStreamOut out, Float f); \
  bool (*push_boolean)(ObjStreamOut out, Boolean b); \
  bool (*push_reference)(ObjStreamOut out, Reference r); \
  bool (*push_object)(ObjStreamOut out, Object obj); \
  bool (*push_window)(ObjStreamOut out, win_T *win); \
  bool (*push_tabpage)(ObjStreamOut out, tabpage_T *tab); \
  bool (*push_buffer)(ObjStreamOut out, buf_T *buf); \
  bool (*error_out)(ObjStreamOut out, Error err); \
  void (*everything_ok)(ObjStreamOut out); \
  Error err

typedef struct {
  OSTREAM_OUT_HEADER;
} ObjStreamOutBase;

typedef bool (*ObjStreamWriter)(void *wcookie, const char *buf, size_t buf_len);

typedef struct {
  OSTREAM_OUT_HEADER;
  ObjStreamWriter write;
  void *wcookie;
} ObjStreamOutMpack;

typedef struct {
  OSTREAM_OUT_HEADER;
} ObjStreamOutVimL;

typedef void *ObjStreamIn;

/// Common header for all “in” streams
///
/// Some notes about string functions:
///
/// `pull_string_start` just checks whether next thing to pull is a string and
///                     returns its length. Actually, all functions check
///                     whether they are going to work with a stream.
/// `pull_string_buf` copies string into a preallocated data, up to ret_len
///                   bytes. Returns string length and length of the remainder,
///                   if any. `*ret_len` is expected to contain buffer length,
///                   after running function it will contain actual length. If
///                   `*ret_len` is less then or equal to actual string length
///                   then string will be received completely.
/// `pull_string` returns pointer to some internal memory in `*ret_buf` used for
///               reading. This memory is valid only as long as other function
///               from OSTREAM_IN_HEADER is not run.
/// `pull_string_alloc` allocates just enough bytes to hold received string with
///                     NUL byte and saves pointer to the allocated memory to
///                     `*ret_buf`. It always reads the whole string.
#define OSTREAM_IN_HEADER \
  bool (*pull_dictionary_start)(ObjStreamIn in, size_t *ret_len); \
  bool (*pull_array_start)(ObjStreamIn in, size_t *ret_len); \
  bool (*pull_string_start)(ObjStreamIn in, size_t *ret_len); \
  bool (*pull_string_buf)(ObjStreamIn in, char *ret_buf, size_t *ret_len, \
                          size_t *remaining_len); \
  bool (*pull_string)(ObjStreamIn in, char **ret_buf, size_t *ret_len, \
                      size_t *remaining_len); \
  bool (*pull_string_alloc)(ObjStreamIn in, char **ret_buf, size_t *ret_len); \
  bool (*pull_integer)(ObjStreamIn in, Integer *ret_int); \
  bool (*pull_float)(ObjStreamIn in, Float *ret_flt); \
  bool (*pull_boolean)(ObjStreamIn in, Boolean *ret_bln); \
  bool (*pull_reference)(ObjStreamIn in, Reference *ret_ref); \
  bool (*pull_object)(ObjStreamIn in, Object *ret_obj); \
  bool (*pull_window)(ObjStreamIn in, win_T **ret_win); \
  bool (*pull_tabpage)(ObjStreamIn in, tabpage_T **ret_tabpage); \
  bool (*pull_buffer)(ObjStreamIn in, buf_T **ret_buf); \
  Error err

typedef struct {
  OSTREAM_IN_HEADER;
} ObjStreamInBase;

typedef ptrdiff_t (*ObjStreamReader)(void *rcookie, char *buf, size_t buf_len);

typedef struct {
  OSTREAM_IN_HEADER;
  RBuffer *rbuf;
  ObjStreamReader read;
  void *rcookie;
} ObjStreamInMpack;

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "api/private/obj_stream.h.generated.h"
#endif
#endif  // NVIM_API_PRIVATE_OBJ_STREAM_H
