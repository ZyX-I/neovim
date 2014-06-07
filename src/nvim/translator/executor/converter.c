#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "nvim/api/private/defs.h"
#include "nvim/api/private/helpers.h"
#include "nvim/func_attr.h"
#include "nvim/memory.h"
#include "nvim/os/msgpack_rpc.h"

#include "nvim/translator/executor/converter.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "translator/executor/converter.c.generated.h"
#endif

static inline void nlua_push_type_idx(lua_State *lstate)
  FUNC_ATTR_NONNULL_ALL
{
  lua_pushnumber(lstate, -2);
}

static inline void nlua_push_locks_idx(lua_State *lstate)
  FUNC_ATTR_NONNULL_ALL
{
  lua_pushnumber(lstate, -1);
}

static inline void nlua_push_val_idx(lua_State *lstate)
  FUNC_ATTR_NONNULL_ALL
{
  lua_pushnumber(lstate, (lua_Number) 0);
}


static inline void nlua_push_type(lua_State *lstate, const char *const type)
{
  lua_getglobal(lstate, "vim");
  lua_getfield(lstate, -1, type);
  lua_remove(lstate, -2);
}

static inline void nlua_create_typed_table(lua_State *lstate,
                                    const size_t narr,
                                    const size_t nrec,
                                    const char *const type)
  FUNC_ATTR_NONNULL_ALL
{
  lua_createtable(lstate, narr, 1 + nrec);
  nlua_push_type_idx(lstate);
  nlua_push_type(lstate, type);
  lua_rawset(lstate, -3);
}


/// Convert given String to lua string
///
/// Leaves converted string on top of the stack.
void nlua_push_String(lua_State *lstate, const String s)
  FUNC_ATTR_NONNULL_ALL
{
  lua_pushlstring(lstate, s.data, s.size);
}

/// Convert given Integer to lua number
///
/// Leaves converted number on top of the stack.
void nlua_push_Integer(lua_State *lstate, const Integer n)
  FUNC_ATTR_NONNULL_ALL
{
  lua_pushnumber(lstate, (lua_Number) n);
}

/// Convert given Float to lua table
///
/// Leaves converted table on top of the stack.
void nlua_push_Float(lua_State *lstate, const Float f)
  FUNC_ATTR_NONNULL_ALL
{
  nlua_create_typed_table(lstate, 0, 1, "VIM_FLOAT");
  nlua_push_val_idx(lstate);
  lua_pushnumber(lstate, (lua_Number) f);
  lua_rawset(lstate, -3);
}

/// Convert given Float to lua boolean
///
/// Leaves converted value on top of the stack.
void nlua_push_Boolean(lua_State *lstate, const Boolean b)
  FUNC_ATTR_NONNULL_ALL
{
  lua_pushboolean(lstate, b);
}

static inline void nlua_add_locks_table(lua_State *lstate)
{
  nlua_push_locks_idx(lstate);
  lua_newtable(lstate);
  lua_rawset(lstate, -3);
}

/// Convert given Dictionary to lua table
///
/// Leaves converted table on top of the stack.
void nlua_push_Dictionary(lua_State *lstate, const Dictionary dict)
  FUNC_ATTR_NONNULL_ALL
{
  nlua_create_typed_table(lstate, 0, 1 + dict.size, "VIM_DICTIONARY");
  nlua_add_locks_table(lstate);
  for (size_t i = 0; i < dict.size; i++) {
    nlua_push_String(lstate, dict.items[i].key);
    nlua_push_Object(lstate, dict.items[i].value);
    lua_rawset(lstate, -3);
  }
}

/// Convert given Array to lua table
///
/// Leaves converted table on top of the stack.
void nlua_push_Array(lua_State *lstate, const Array array)
  FUNC_ATTR_NONNULL_ALL
{
  nlua_create_typed_table(lstate, array.size, 1, "VIM_FLOAT");
  nlua_add_locks_table(lstate);
  for (size_t i = 0; i < array.size; i++) {
    nlua_push_Integer(lstate, (Integer) i + 1);
    nlua_push_Object(lstate, array.items[i]);
    lua_rawset(lstate, -3);
  }
}

/// Convert given Object to lua value
///
/// Leaves converted value on top of the stack.
void nlua_push_Object(lua_State *lstate, const Object obj)
  FUNC_ATTR_NONNULL_ALL
{
  switch (obj.type) {
    case kObjectTypeNil: {
      lua_pushnil(lstate);
      break;
    }
#define ADD_TYPE(type, data_key) \
    case kObjectType##type: { \
      nlua_push_##type(lstate, obj.data.data_key); \
      break; \
    }
    ADD_TYPE(Boolean,    boolean)
    ADD_TYPE(Integer,    integer)
    ADD_TYPE(Float,      floating)
    ADD_TYPE(String,     string)
    ADD_TYPE(Array,      array)
    ADD_TYPE(Dictionary, dictionary)
#undef ADD_TYPE
  }
}


/// Convert lua value to string
///
/// Always pops one value from the stack.
String nlua_pop_String(lua_State *lstate, Error *err)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  String ret;

  ret.data = (char *) lua_tolstring(lstate, -1, &(ret.size));

  if (ret.data == NULL) {
    set_api_error("Expected lua string", err);
    return (String) {.size = 0, .data = NULL};
  }

  ret.data = xmemdup(ret.data, ret.size);
  lua_pop(lstate, 1);

  return ret;
}

/// Convert lua value to integer
///
/// Always pops one value from the stack.
Integer nlua_pop_Integer(lua_State *lstate, Error *err)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  Integer ret = 0;

  if (!lua_isnumber(lstate, -1)) {
    set_api_error("Expected lua number", err);
    return ret;
  }
  ret = (Integer) lua_tonumber(lstate, -1);
  lua_pop(lstate, 1);

  return ret;
}

/// Convert lua value to boolean
///
/// Always pops one value from the stack.
Boolean nlua_pop_Boolean(lua_State *lstate, Error *err)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  Boolean ret = lua_toboolean(lstate, -1);
  lua_pop(lstate, 1);
  return ret;
}

static inline bool nlua_check_type(lua_State *lstate, Error *err,
                                   const char *const type)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  if (lua_type(lstate, -1) != LUA_TTABLE) {
    set_api_error("Expected lua table", err);
    return true;
  }

  // TODO Cache type pointers
  nlua_push_type_idx(lstate);
  lua_rawget(lstate, -2);
  nlua_push_type(lstate, type);
  if (!lua_rawequal(lstate, -2, -1)) {
    lua_pop(lstate, 2);
    set_api_error("Expected lua table with VIM_FLOAT type", err);
    return true;
  }
  lua_pop(lstate, 2);

  return false;
}

/// Convert lua table to float
///
/// Always pops one value from the stack.
Float nlua_pop_Float(lua_State *lstate, Error *err)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  Float ret = 0;

  if (nlua_check_type(lstate, err, "VIM_FLOAT")) {
    lua_pop(lstate, 1);
    return 0;
  }

  nlua_push_val_idx(lstate);
  lua_rawget(lstate, -2);

  if (!lua_isnumber(lstate, -1)) {
    set_api_error("Value field should be lua number", err);
    return ret;
  }
  ret = lua_tonumber(lstate, -1);
  lua_pop(lstate, 2);

  return ret;
}

/// Convert lua table to array
///
/// Always pops one value from the stack.
Array nlua_pop_Array(lua_State *lstate, Error *err)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  Array ret = {.size = 0, .items = NULL};

  if (nlua_check_type(lstate, err, "VIM_LIST")) {
    lua_pop(lstate, 1);
    return ret;
  }

  for (lua_Number i = 1; ; i++, ret.size++) {
    lua_pushnumber(lstate, (lua_Number) i);
    lua_rawget(lstate, -2);

    if (lua_isnil(lstate, -1))
      break;
  }

  if (ret.size == 0)
    return ret;

  ret.items = xcalloc(ret.size, sizeof(*ret.items));
  for (size_t i = 1; i <= ret.size; i++) {
    Object val;

    lua_pushnumber(lstate, (lua_Number) i);
    lua_rawget(lstate, -2);

    val = nlua_pop_Object(lstate, err);
    if (err->set) {
      ret.size = i;
      lua_pop(lstate, 1);
      msgpack_rpc_free_array(ret);
      return (Array) {.size = 0, .items = NULL};
    }
    ret.items[i - 1] = val;
  }
  lua_pop(lstate, 1);

  return ret;
}

/// Convert lua table to dictionary
///
/// Always pops one value from the stack. Does not check whether
/// `table[type_idx] == VIM_DICTIONARY` or whether topmost value on the stack is 
/// a table.
Dictionary nlua_pop_Dictionary_unchecked(lua_State *lstate, Error *err)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  Dictionary ret = {.size = 0, .items = NULL};

  lua_pushnil(lstate);

  while (lua_next(lstate, -2)) {
    if (lua_type(lstate, -2) == LUA_TSTRING)
      ret.size++;
    lua_pop(lstate, 1);
  }

  if (ret.size == 0) {
    lua_pop(lstate, 1);
    return ret;
  }
  ret.items = xcalloc(ret.size, sizeof(*ret.items));

  lua_pushnil(lstate);
  for (size_t i = 0; lua_next(lstate, -2); i++) {
    // stack: dict, key, value

    if (lua_type(lstate, -2) == LUA_TSTRING) {
      lua_pushvalue(lstate, -2);
      // stack: dict, key, value, key

      ret.items[i].key = nlua_pop_String(lstate, err);
      // stack: dict, key, value

      if (!err->set)
        ret.items[i].value = nlua_pop_Object(lstate, err);
        // stack: dict, key
      else
        lua_pop(lstate, 1);
        // stack: dict, key

      if (err->set) {
        ret.size = i;
        msgpack_rpc_free_dictionary(ret);
        lua_pop(lstate, 2);
        // stack:
        return (Dictionary) {.size = 0, .items = NULL};
      }
    } else {
      lua_pop(lstate, 1);
      // stack: dict, key
    }
  }
  lua_pop(lstate, 1);

  return ret;
}

/// Convert lua table to dictionary
///
/// Always pops one value from the stack.
Dictionary nlua_pop_Dictionary(lua_State *lstate, Error *err)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  if (nlua_check_type(lstate, err, "VIM_DICTIONARY")) {
    lua_pop(lstate, 1);
    return (Dictionary) {.size = 0, .items = NULL};
  }

  return nlua_pop_Dictionary_unchecked(lstate, err);
}

/// Convert lua table to object
///
/// Always pops one value from the stack.
Object nlua_pop_Object(lua_State *lstate, Error *err)
  FUNC_ATTR_NONNULL_ALL FUNC_ATTR_WARN_UNUSED_RESULT
{
  Object ret = {.type = kObjectTypeNil};

  switch (lua_type(lstate, -1)) {
    case LUA_TNIL: {
      ret.type = kObjectTypeNil;
      lua_pop(lstate, 1);
      break;
    }
    case LUA_TSTRING: {
      ret.type = kObjectTypeString;
      ret.data.string = nlua_pop_String(lstate, err);
      break;
    }
    case LUA_TNUMBER: {
      ret.type = kObjectTypeInteger;
      ret.data.integer = nlua_pop_Integer(lstate, err);
      break;
    }
    case LUA_TBOOLEAN: {
      ret.type = kObjectTypeBoolean;
      ret.data.boolean = nlua_pop_Boolean(lstate, err);
      break;
    }
    case LUA_TTABLE: {
      lua_getglobal(lstate, "vim");
      // stack: obj, vim
      nlua_push_type_idx(lstate);
      // stack: obj, vim, type_idx
      lua_rawget(lstate, -3);
      // stack: obj, vim, type:obj
#define CHECK_TYPE(Type, key, vim_type) \
      lua_getfield(lstate, -2, #vim_type); \
      /* stack: obj, vim, type:obj, type:VIM_FLOAT */ \
      if (lua_rawequal(lstate, -2, -1)) { \
        lua_pop(lstate, 3); \
        /* stack: obj */ \
        ret.type = kObjectType##Type; \
        ret.data.key = nlua_pop_##Type(lstate, err); \
        /* stack: */ \
        break; \
      } \
      lua_pop(lstate, 1); \
      /* stack: obj, vim, type:obj */
      CHECK_TYPE(Float, floating, VIM_FLOAT)
      CHECK_TYPE(Array, array, VIM_LIST)
      CHECK_TYPE(Dictionary, dictionary, VIM_DICTIONARY)
#undef CHECK_TYPE
      lua_pop(lstate, 2);
      // stack: obj
      ret.type = kObjectTypeDictionary;
      ret.data.dictionary = nlua_pop_Dictionary_unchecked(lstate, err);
      break;
    }
    default: {
      lua_pop(lstate, 1);
      set_api_error("Cannot convert given lua type", err);
      break;
    }
  }

  return ret;
}
