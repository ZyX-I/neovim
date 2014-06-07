#ifndef NVIM_TRANSLATOR_EXECUTOR_CONVERTER_H
#define NVIM_TRANSLATOR_EXECUTOR_CONVERTER_H

#include <lua.h>
#include <stdbool.h>
#include "nvim/api/private/defs.h"
#include "nvim/func_attr.h"

inline void nlua_push_type_idx(lua_State *lstate)
  FUNC_ATTR_NONNULL_ALL
{
  lua_pushnumber(lstate, -2);
}

inline void nlua_push_locks_idx(lua_State *lstate)
  FUNC_ATTR_NONNULL_ALL
{
  lua_pushnumber(lstate, -1);
}

inline void nlua_push_val_idx(lua_State *lstate)
  FUNC_ATTR_NONNULL_ALL
{
  lua_pushnumber(lstate, (lua_Number) 0);
}


inline void nlua_push_type(lua_State *lstate, const char *const type)
{
  lua_getglobal(lstate, "vim");
  lua_getfield(lstate, -1, type);
  lua_remove(lstate, -2);
}

inline void nlua_create_typed_table(lua_State *lstate,
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
inline void nlua_push_String(lua_State *lstate, const String s)
  FUNC_ATTR_NONNULL_ALL
{
  lua_pushlstring(lstate, s.data, s.size);
}

/// Convert given Integer to lua number
///
/// Leaves converted number on top of the stack.
inline void nlua_push_Integer(lua_State *lstate, const Integer n)
  FUNC_ATTR_NONNULL_ALL
{
  lua_pushnumber(lstate, (lua_Number) n);
}

/// Convert given Float to lua table
///
/// Leaves converted table on top of the stack.
inline void nlua_push_Float(lua_State *lstate, const Float f)
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
inline void nlua_push_Boolean(lua_State *lstate, const Boolean b)
  FUNC_ATTR_NONNULL_ALL
{
  lua_pushboolean(lstate, b);
}

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "translator/executor/converter.h.generated.h"
#endif
#endif  // NVIM_TRANSLATOR_EXECUTOR_CONVERTER_H
