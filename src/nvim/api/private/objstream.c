/// @file objstream.c
///
/// Contains definitions for object stream variant of API.

#include "nvim/api/private/defs.h"
#include "nvim/api/private/helpers.h"
#include "nvim/api/private/objstream.h"
#include "nvim/lib/kvec.h"

typedef kvec_withinit_t(Stack, 16);

struct objstream_mpack_stream_in {
  ObjStreamInDef *def;
  Error err;
  struct {
    Stream *stream;
  } state;
};

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "api/private/objstream.c.generated.h"
#endif

static ObjStreamInDef mpack_stream_def = {
#define DEF(name) .name = objstream_mpack_##name
#define RECV_DEF(name) DEF(recv_##name)
  DEF(discard),
  RECV_DEF(dictionary),
  RECV_DEF(array),
  RECV_DEF(string),
  RECV_DEF(string_buf),
  RECV_DEF(nil),
  RECV_DEF(boolean),
  RECV_DEF(integer),
  RECV_DEF(float),
  RECV_DEF(buffer),
  RECV_DEF(window),
  RECV_DEF(tabpage),
  RECV_DEF(object),
#undef DEF
#undef RECV_DEF
};
