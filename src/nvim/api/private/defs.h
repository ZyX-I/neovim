#ifndef NVIM_API_PRIVATE_DEFS_H
#define NVIM_API_PRIVATE_DEFS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "nvim/func_attr.h"
#include "nvim/eval_defs.h"

#define ARRAY_DICT_INIT {.size = 0, .capacity = 0, .items = NULL}
#define STRING_INIT {.data = NULL, .size = 0}
#define OBJECT_INIT { .type = kObjectTypeNil }
#define ERROR_INIT { .set = false }
#define REMOTE_TYPE(type) typedef uint64_t type

#ifdef INCLUDE_GENERATED_DECLARATIONS
# define ArrayOf(...) Array
# define DictionaryOf(...) Dictionary
#endif

// Basic types
typedef enum {
  kErrorTypeException,
  kErrorTypeValidation
} ErrorType;

typedef enum {
  kMessageTypeRequest,
  kMessageTypeResponse,
  kMessageTypeNotification
} MessageType;

/// Used as the message ID of notifications.
#define NO_RESPONSE UINT64_MAX

typedef struct {
  ErrorType type;
  char msg[1024];
  bool set;
} Error;

typedef bool Boolean;
typedef int64_t Integer;
typedef double Float;

/// Maximum value of an Integer
#define API_INTEGER_MAX INT64_MAX

/// Minimum value of an Integer
#define API_INTEGER_MIN INT64_MIN

typedef struct {
  char *data;
  size_t size;
} String;

REMOTE_TYPE(Buffer);
REMOTE_TYPE(Window);
REMOTE_TYPE(Tabpage);

typedef struct object Object;

typedef struct {
  Object *items;
  size_t size, capacity;
} Array;

typedef struct key_value_pair KeyValuePair;

typedef struct {
  KeyValuePair *items;
  size_t size, capacity;
} Dictionary;

/// Reference object implementation
///
/// Contains references to functions which do something with referenced object.
typedef struct reference_def ReferenceDef;

/// Reference object
///
/// Is used to remotely access containers and functions.
typedef struct {
  const struct reference_def *def;  ///< Functions used to manipulate reference.
  /// Data
  ///
  /// This pointer is also an identifier. When serializing to msgpack pointer is
  /// looked up in a reference table, where it will be added with .def when it
  /// is missing, serialized msgpack contains pointer as-is. When deserializing
  /// msgpack .def is searched in a reference table.
  void *data;
} Reference;

typedef enum {
  kObjectTypeBuffer,
  kObjectTypeWindow,
  kObjectTypeTabpage,
  kObjectTypeNil,
  kObjectTypeBoolean,
  kObjectTypeInteger,
  kObjectTypeFloat,
  kObjectTypeString,
  kObjectTypeArray,
  kObjectTypeDictionary,
  kObjectTypeReference,   ///< Reference object.
} ObjectType;

struct object {
  ObjectType type;
  union {
    Buffer buffer;
    Window window;
    Tabpage tabpage;
    Boolean boolean;
    Integer integer;
    Float floating;
    String string;
    Array array;
    Dictionary dictionary;
    Reference reference;    ///< Used with kObjectTypeReference.
  } data;
};

/// Type of a function used to get item from Reference object
typedef Object (*ReferenceGetItem)(const Reference ref,
                                   const Object key,
                                   Error *const err)
  REAL_FATTR_WARN_UNUSED_RESULT REAL_FATTR_NONNULL_ALL;

/// Type of a function used to get item from Reference object
///
/// @return Deleted item.
typedef Object (*ReferenceDelItem)(Reference ref,
                                   const Object key,
                                   Error *const err)
  REAL_FATTR_WARN_UNUSED_RESULT REAL_FATTR_NONNULL_ALL;

/// Type of a function used to get item from Reference object
///
/// @return A pair (Boolean, Object) where first is true if Object is a replaced
///         item (i.e. there was some item at that key). If there were no items,
///         first element in a pair is false and second is NIL.
typedef Array (*ReferenceSetItem)(Reference ref,
                                  const Object key,
                                  Object val,
                                  Error *const err)
  REAL_FATTR_WARN_UNUSED_RESULT REAL_FATTR_NONNULL_ALL;

/// Type of a function used to get slice
typedef Array (*ReferenceGetSlice)(const Reference ref,
                                   const Object start,
                                   const Object end,
                                   Error *const err)
  REAL_FATTR_WARN_UNUSED_RESULT REAL_FATTR_NONNULL_ALL;

/// Type of a function used to delete slice
///
/// @return Deleted items.
typedef Array (*ReferenceDelSlice)(Reference ref,
                                   const Object start,
                                   const Object end,
                                   Error *const err)
  REAL_FATTR_WARN_UNUSED_RESULT REAL_FATTR_NONNULL_ALL;

/// Type of a function used to replace slice with values
///
/// @return Either NIL or an array with removed items.
typedef Object (*ReferenceSetSlice)(Reference ref,
                                    const Object start,
                                    const Object end,
                                    Array val,
                                    Error *const err)
  REAL_FATTR_WARN_UNUSED_RESULT REAL_FATTR_NONNULL_ALL;

/// Type of a function used to convert a Reference to a VimL object
typedef void (*ReferenceCoerceToVimL)(Reference ref, typval_T *const rettv,
                                      Error *const err)
  REAL_FATTR_NONNULL_ALL;

/// Type of a function used to get reference length
typedef Integer (*ReferenceGetLen)(const Reference ref)
  REAL_FATTR_WARN_UNUSED_RESULT;

/// Type of a function used to clear a container
typedef void (*ReferenceClear)(Reference ref, Error *const err)
  REAL_FATTR_NONNULL_ALL;

/// Type of a function used to lock a container
typedef void (*ReferenceLock)(Reference ref, Error *const err)
  REAL_FATTR_NONNULL_ALL;

/// Type of a function used to lock an item in a container
typedef void (*ReferenceLockItem)(Reference ref, const Object key,
                                  Error *const err)
  REAL_FATTR_NONNULL_ALL;

/// Type of a function used to get lock status
///
/// @return 1 if container is locked, -1 if it is locked and lock status is
///         fixed and 0 if it is not locked.
typedef Integer (*ReferenceLockStatus)(Reference ref, Error *err)
  REAL_FATTR_WARN_UNUSED_RESULT REAL_FATTR_NONNULL_ALL;

/// Type of a function used to get lock status of an item in a container
///
/// @return 1 if item is locked, -1 if it is locked and lock status is fixed and
///         0 if it is not locked.
typedef Integer (*ReferenceItemLockStatus)(Reference ref, const Object key,
                                           Error *const err)
  REAL_FATTR_WARN_UNUSED_RESULT REAL_FATTR_NONNULL_ALL;

/// Type of a function used to call a function reference
///
/// @param[in]  ref  Reference to call.
/// @param[in]  args  Arguments.
/// @param[in]  self  Dictionary self (may be Reference that coerces into
///                   #VAR_DICT object) or NIL.
///
/// @return Result of the call or NIL.
typedef Object (*ReferenceCall)(const Reference ref,
                                Array args, Object self, Error *const err)
  FUNC_ATTR_WARN_UNUSED_RESULT FUNC_ATTR_NONNULL_ALL;

/// Type of a function used to convert Reference to Object
///
/// Resulting Object may not contain any container Reference objects, but may
/// still contain non-container Reference objects.
typedef Object (*ReferenceCoerceToObject)(const Reference ref,
                                          Error *const err);

/// Type of a function used to get an array of dictionary keys
typedef Array (*ReferenceGetKeys)(const Reference ref, Error *const err)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT;

/// Type of a function used to get an array of dictionary values
typedef Array (*ReferenceGetValues)(const Reference ref, Error *const err)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT;

/// Type of a function used to get an array of dictionary items
typedef ArrayOf(Array) (*ReferenceGetItems)(const Reference ref,
                                            Error *const err)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT;

/// Type of a function used to append item to an array
typedef void (*ReferenceAppend)(Reference ref, Error *const err)
  FUNC_ATTR_NONNULL_ALL;

/// Type of a function used to update a dictionary with a dictionary
///
/// @param[out]  ref  Reference to update.
/// @param[in]  dict  Dictionary to update with. May be a Reference that coerces
///                   to #VAR_DICT.
/// @param[in]  conflict_resolver  How to resolve conflicts: should be either
///                                NIL (acts like `extend()` by default), any of
///                                the strings that can be passed to `extend()`
///                                or a Reference to a function which receives
///                                key, old value, new value, the fourth
///                                argument and should either return the fourth
///                                argument which signals that conflicting key
///                                should be removed or desired value. It may
///                                also raise an exception.
/// @param[out]  err  Location where error will be saved.
typedef void (*ReferenceUpdate)(Reference ref, const Object dict,
                                Object conflict_resolver,
                                Error *const err)
  FUNC_ATTR_NONNULL_ALL;

struct reference_def {
  struct {
    ReferenceCoerceToObject coerce_to_object;
    // Functions common for all containers
    ReferenceGetItem get_item;
    ReferenceSetItem set_item;
    ReferenceDelItem del_item;
    ReferenceGetLen get_len;
    ReferenceClear clear;
    // Lock, unlock and get_lock_status functions must be defined at the same
    // time.
    ReferenceLock lock;
    ReferenceLock unlock;
    ReferenceLockStatus get_lock_status;
    // Lock, unlock and get_lock_status functions used for items must be defined
    // at the same time.
    ReferenceLockItem lock_item;
    ReferenceLockItem unlock_item;
    ReferenceItemLockStatus get_item_lock_status;
    struct {
      ReferenceGetSlice get_slice;
      ReferenceSetSlice set_slice;
      ReferenceDelSlice del_slice;
      ReferenceAppend append;
    } list;
    struct {
      ReferenceGetKeys get_keys;
      ReferenceGetValues get_values;
      ReferenceGetItems get_items;
      ReferenceUpdate update;
    } dict;
  } container;
  struct {
    ReferenceCall call;
  } func;
  /// Internal functions: they do not have a corresponding capability and must
  /// always be defined.
  struct internal_data {
    ReferenceCoerceToVimL coerce;
    char capabilities[3];  ///< Saved output of ref_capabilities().
  } internal;
};

/// Number of bytes needed to hold all capabilities
#define REF_DEF_FLAGS_SIZE ( \
    ((((sizeof(ReferenceDef) - sizeof(struct internal_data)) \
       / sizeof(void *)) \
      - 2 /* unlock and lock_status … */ * 2 /* … of item and container */) \
     / 8) + 1)

struct key_value_pair {
  String key;
  Object value;
};

/// Table where references are saved.
typedef struct reference_table ReferenceTable;

#endif  // NVIM_API_PRIVATE_DEFS_H
