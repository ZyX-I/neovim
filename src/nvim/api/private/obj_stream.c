#include <stdbool.h>

#include <mpack.h>

#include "nvim/api/private/defs.h"
#include "nvim/api/private/obj_stream.h"
#include "nvim/lib/kvec.h"
#include "nvim/func_attr.h"
#include "nvim/os/fileio.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "api/private/obj_stream.c.generated.h"
#endif

typedef struct {
  const Object *aobj;
  bool container;
  size_t idx;
} PushStackItem;

bool ostream_push_object(ObjStreamOut out_, const Object obj)
  FUNC_ATTR_NONNULL_ALL
{
  ObjStreamOutBase *const out = (ObjStreamOutBase *)out_;
  kvec_t(PushStackItem) stack = KV_INITIAL_VALUE;
  kv_push(stack, ((PushStackItem) { &obj, false, 0 }));
  while (kv_size(stack) && !out->err.set) {
    PushStackItem cur = kv_last(stack);
    switch (cur.aobj->type) {
      case kObjectTypeNil: {
        out->push_nil(out);
        break;
      }
      case kObjectTypeBoolean: {
        msgpack_rpc_from_boolean(, res);
        out->push_boolean(out, cur.aobj->data.boolean);
        break;
      }
      case kObjectTypeInteger: {
        out->push_integer(out, cur.aobj->data.integer);
        break;
      }
      case kObjectTypeFloat: {
        out->push_floating(out, cur.aobj->data.floating);
        break;
      }
      case kObjectTypeString: {
        out->push_string(out, cur.aobj->data.string);
        break;
      }
      case kObjectTypeBuffer: {
        out->push_buffer(out, cur.aobj->data.buffer);
        break;
      }
      case kObjectTypeWindow: {
        out->push_window(out, cur.aobj->data.window);
        break;
      }
      case kObjectTypeTabpage: {
        out->push_tabpage(out, cur.aobj->data.tabpage);
        break;
      }
      case kObjectTypeArray: {
        const size_t size = cur.aobj->data.array.size;
        if (cur.container) {
          if (cur.idx >= size) {
            (void)kv_pop(stack);
          } else {
            const size_t idx = cur.idx;
            cur.idx++;
            kv_last(stack) = cur;
            kv_push(stack, ((PushStackItem) {
              .aobj = &cur.aobj->data.array.items[idx],
              .container = false,
            }));
          }
        } else {
          out->push_array_start(out, size);
          cur.container = true;
          kv_last(stack) = cur;
        }
        break;
      }
      case kObjectTypeDictionary: {
        const size_t size = cur.aobj->data.dictionary.size;
        if (cur.container) {
          if (cur.idx >= size) {
            (void)kv_pop(stack);
          } else {
            const size_t idx = cur.idx;
            cur.idx++;
            kv_last(stack) = cur;
            out->push_string(out, cur.aobj->data.dictionary.items[idx].key);
            kv_push(stack, ((PushStackItem) {
              .aobj = &cur.aobj->data.dictionary.items[idx].value,
              .container = false,
            }));
          }
        } else {
          out->push_dictionary_start(out, size);
          cur.container = true;
          kv_last(stack) = cur;
        }
        break;
      }
      case kObjectTypeReference: {
        out->push_reference(out, cur.aobj->data.reference);
        break;
      }
    }
    if (!cur.container) {
      (void)kv_pop(stack);
    }
  }
  kv_destroy(stack);
  return !out->err.set;
}

typedef struct {
  Object *tgt;
} PullStackItem;

bool ostream_mpack_pull_object(ObjStreamIn in_, Object *const ret_obj)
  FUNC_ATTR_NONNULL_ALL
{
  ObjStreamInMpack *in = (ObjStreamInBase *)in_;
  kvec_t(PullStackItem) stack = KV_INITIAL_VALUE;
  kv_push(stack, (PullStackItem) { ret_obj });
  mpack_tokbuf_t tokbuf = MPACK_TOKBUF_INITIAL_VALUE;
  while (kv_size(stack) && !in->err.set) {
    PullStackItem cur = kv_last(stack);
    // TODO
  }
  return !in->err.set;
}

ObjStreamIn ostream_in_mpack_init(ObjStreamReader read)
{
  ObjStreamInMpack ret = {
    .pull_object = &ostream_mpack_pull_object,
    .err = { .set = false },
    .rbuf = rbuffer_new(kRWBufferSize),
  };
  return xmemdup(&ret, sizeof(ret));
}
