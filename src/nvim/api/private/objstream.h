#ifndef NVIM_API_PRIVATE_OBJSTREAM_H
#define NVIM_API_PRIVATE_OBJSTREAM_H

#include "nvim/func_attr.h"
#include "nvim/buffer_defs.h"
#include "nvim/api/private/defs.h"

typedef struct obj_stream_in_def ObjStreamInDef;

/// Input stream
typedef struct {
  ObjStreamInDef *def;  ///< Stream definition: callbacks used with the stream.
  Error err;  ///< Error.
  char data[];  ///< Data for the stream. To be replaced in other structures.
} ObjStreamIn;

/// Definition (collection of callbacks) of the input stream
///
/// Used to receive data from remote.
///
/// @note All `recv_` functions below are supposed to call `discard()` on error
///       and set `istream->err`.
struct {
  /// Discard any pending input
  void (*discard)(void);
  /// Receive dictionary
  ///
  /// After this user must call recv_… 2*ret times: calls in pairs, each first
  /// call to receive a key, each second to receive a corresponding value.
  ///
  /// @param  istream  Input stream.
  ///
  /// @return Dictionary size (number of keys).
  size_t (*recv_dictionary)(ObjStreamIn *istream)
      REAL_FATTR_NONNULL_ALL REAL_FATTR_WARN_UNUSED_RESULT;
  /// Receive array
  ///
  /// After this user must call recv_… ret times to receive one value at a time.
  ///
  /// @param  istream  Input stream.
  ///
  /// @return Array size.
  size_t (*recv_array)(ObjStreamIn *istream)
      REAL_FATTR_NONNULL_ALL REAL_FATTR_WARN_UNUSED_RESULT;
  /// Receive full string into a newly allocated memory
  ///
  /// @param  istream  Input stream.
  /// @param[out]  ret_size  Location where size of the returned string should
  ///                        be stored. If NULL, either returns NUL-terminated
  ///                        string or errors out if received string appears to
  ///                        contain NUL bytes. If ret_size is not NULL,
  ///                        returned string will not be NUL-terminated.
  ///
  /// @return [allocated] string in an allocated memory or NULL on error.
  char *(*recv_string)(ObjStreamIn *istream, size_t *ret_size)
      REAL_FATTR_MALLOC REAL_FATTR_NONNULL_ARG(1) REAL_FATTR_WARN_UNUSED_RESULT;
  /// Receive string into a buffer
  ///
  /// @param  istream  Input stream.
  /// @param[out]  ret_buf  Buffer where string should be saved.
  /// @param[in]  buf_len  Buffer length, must be greater then zero.
  /// @param[out]  ret_remaining  Remaining length. If NULL then errors out if
  ///                             string to receive does not fit into the
  ///                             buffer. Resulting buffer is not
  ///                             NUL-terminated.
  ///
  /// @return number of bytes put into the buffer.
  size_t (*recv_string_buf)(ObjStreamIn *istream, char *ret_buf, size_t buf_len,
                            size_t *ret_remaining)
      REAL_FATTR_NONNULL_ARG(1, 2) REAL_FATTR_WARN_UNUSED_RESULT;
  /// Receive NIL value
  ///
  /// @param  istream  Input stream.
  void (*recv_nil)(ObjStreamIn *istream)
      REAL_FATTR_NONNULL_ALL REAL_FATTR_WARN_UNUSED_RESULT;
  /// Receive boolean value
  ///
  /// @param  istream  Input stream.
  ///
  /// @return boolean value.
  Boolean (*recv_boolean)(ObjStreamIn *istream)
      REAL_FATTR_NONNULL_ALL REAL_FATTR_WARN_UNUSED_RESULT;
  /// Receive integer value
  ///
  /// @param  istream  Input stream.
  ///
  /// @return integer value.
  Integer (*recv_integer)(ObjStreamIn *istream)
      REAL_FATTR_NONNULL_ALL REAL_FATTR_WARN_UNUSED_RESULT;
  /// Receive floating-point value
  ///
  /// @param  istream  Input stream.
  ///
  /// @return floating-point value.
  Float (*recv_float)(ObjStreamIn *istream)
      REAL_FATTR_NONNULL_ALL REAL_FATTR_WARN_UNUSED_RESULT;
  /// Receive buffer
  ///
  /// @param  istream  Input stream.
  ///
  /// @return Pointer to the buffer structure or NULL.
  buf_T *(*recv_buffer)(ObjStreamIn *istream)
      REAL_FATTR_NONNULL_ALL REAL_FATTR_WARN_UNUSED_RESULT;
  /// Receive window
  ///
  /// @param  istream  Input stream.
  ///
  /// @return Pointer to the window structure or NULL.
  win_T *(*recv_window)(ObjStreamIn *istream)
      REAL_FATTR_NONNULL_ALL REAL_FATTR_WARN_UNUSED_RESULT;
  /// Receive tabpage
  ///
  /// @param  istream  Input stream.
  ///
  /// @return Pointer to the tabpage structure or NULL.
  tabpage_T *(*recv_tabpage)(ObjStreamIn *istream)
      REAL_FATTR_NONNULL_ALL REAL_FATTR_WARN_UNUSED_RESULT;
  /// Start receiving object
  ///
  /// @note After calling this function next `recv_…` call will operate with the
  ///       same input, so intended usage is “switch on recv_object(), run
  ///       recv_… in cases like this: `case kObjectTypeInteger:
  ///       i = istream->def->recv_integer(istream)`”.
  ///
  /// @param  istream  Input stream.
  ///
  /// @return Object type.
  ObjectType (*recv_object)(ObjStreamIn *istream)
      REAL_FATTR_NONNULL_ALL REAL_FATTR_WARN_UNUSED_RESULT;
};

/// ObjStreamInDef->recv_… wrapper
///
/// @param  varass  Variable assignment: either `varname =` or nothing.
/// @param  istream  Input stream to work with.
/// @param  what  What to receive: ObjStreamInDef attribute name without `recv_`
///               prefix.
/// @param  args  All arguments to the recv_… function, including istream. Must
///               be enclosed in parenthesis.
/// @param  errcode  Code to run on error.
#define _OBJSTREAM_RECV(varass, istream, what, args, errcode) \
    do { \
      varass istream->def->recv_##what args; \
      if (ERROR_SET(&istream->err)) { \
        errcode; \
      } \
    } while (0)

/// Receive scalar and save that into varname, goto objstream_error on error
#define _OBJSTREAM_RECV_SCALAR(varname, istream, what) \
    _OBJSTREAM_RECV(varname =, istream, what, (istream), goto objstream_error)

/// Receive boolean value
#define OBJSTREAM_RECV_BOOLEAN(varname, istream) \
    OBJSTREAM_RECV_SCALAR(varname, istream, boolean)

/// Receive integer value
#define OBJSTREAM_RECV_INTEGER(varname, istream) \
    OBJSTREAM_RECV_SCALAR(varname, istream, integer)

/// Receive floating-point value
#define OBJSTREAM_RECV_FLOAT(varname, istream) \
    OBJSTREAM_RECV_SCALAR(varname, istream, float)

/// Receive buffer value
#define OBJSTREAM_RECV_BUFFER(varname, istream) \
    OBJSTREAM_RECV_SCALAR(varname, istream, buffer)

/// Receive window value
#define OBJSTREAM_RECV_WINDOW(varname, istream) \
    OBJSTREAM_RECV_SCALAR(varname, istream, window)

/// Receive tabpage value
#define OBJSTREAM_RECV_TABPAGE(varname, istream) \
    OBJSTREAM_RECV_SCALAR(varname, istream, tabpage)

/// Receive object type
#define _OBJSTREAM_RECV_OBJECT(varname, istream) \
    OBJSTREAM_RECV_SCALAR(varname, istream, object)

/// Receive object type and use it in switch()
#define OBJSTREAM_RECV_OBJECT_SWITCH(type, istream) \
    ObjectType type; \
    _OBJSTREAM_RECV_OBJECT(type, istream); \
    switch (type)

/// Receive NIL, goto objstream_error on error
#define OBJSTREAM_RECV_NIL(istream) \
    _OBJSTREAM_RECV(, istream, nil, (istream), goto objstream_error)

/// Receive string into an allocated memory, goto objstream_error on error
#define OBJSTREAM_RECV_STRING_ALLOC(varname, istream, ret_size) \
    _OBJSTREAM_RECV(varname =, istream, string, (istream, ret_size), \
                    goto objstream_error)

/// Receive string into a buffer, goto objstream_error on error
#define OBJSTREAM_RECV_STRING_BUF(varname, istream, ret_buf, buf_size, \
                                  ret_remaining) \
    _OBJSTREAM_RECV(varname =, istream, string_buf, \
                    (istream, ret_buf, buf_size, ret_remaining), \
                    goto objstream_error)

/// Definition (collection of callbacks) of the output stream
///
/// Used to send data to remote.
typedef struct {
} ObjStreamOutDef;

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "api/private/objstream.h.generated.h"
#endif

#endif  // NVIM_API_PRIVATE_OBJSTREAM_H
