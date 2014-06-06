#include "nvim/vim.h"
#include "nvim/misc1.h"
#include "nvim/term.h"
#include "nvim/func_attr.h"
#include "nvim/api/private/helpers.h"
#include "nvim/api/vim.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "translator/executor/executor.c.generated.h"
#endif

/// Name of the run code for use in messages
#define NLUA_EVAL_NAME "<VimL compiled string>"

/// Call C function which does not expect any arguments
///
/// @param  function  Called function
/// @param  numret    Number of returned arguments
#define NLUA_CALL_C_FUNCTION_0(lstate, function, numret) \
    lua_pushcfunction(lstate, &function); \
    lua_call(lstate, 0, numret)
/// Call C function which expects four arguments
///
/// @param  function  Called function
/// @param  numret    Number of returned arguments
/// @param  a…        Supplied argument (should be a void* pointer)
#define NLUA_CALL_C_FUNCTION_3(lstate, function, numret, a1, a2, a3) \
    lua_pushcfunction(lstate, &function); \
    lua_pushlightuserdata(lstate, a1); \
    lua_pushlightuserdata(lstate, a2); \
    lua_pushlightuserdata(lstate, a3); \
    lua_call(lstate, 3, numret)

static void set_lua_error(lua_State *lstate, Error *err) FUNC_ATTR_NONNULL_ALL
{
  size_t len;
  const char *str;
  str = lua_tolstring(lstate, -1, &len);
  lua_pop(lstate, 1);

  // FIXME? More specific error?
  set_api_error("Error while executing lua code", err);

  // FIXME!! Print error message
}

/// Convert lua value to Object
///
/// @param[in]   lstate  Lua state.
/// @param[in]   index   Index of the object on the stack.
/// @param[out]  obj     Pointer to Object where the result will be saved.
static void convert_lua_to_object(lua_State *lstate, int index, Object *obj)
  FUNC_ATTR_NONNULL_ALL
{
  switch(lua_type(lstate, index)) {
    case LUA_TBOOLEAN: {
      obj->type = kObjectTypeBoolean;
      obj->data.boolean = lua_toboolean(lstate, index);
      break;
    }
    case LUA_TSTRING: {
      obj->type = kObjectTypeString;
      obj->data.string.data = (char *) lua_tolstring(lstate, index,
                                                     &(obj->data.string.size));
      break;
    }
    case LUA_TNUMBER: {
      obj->type = kObjectTypeFloat;
      obj->data.floating = lua_tonumber(lstate, index);
      break;
    }
    case LUA_TNONE: {
      // FIXME Give error message?
    }
    case LUA_TTABLE: {
      // FIXME Based on chosen implementation this or the next types should be 
      // converted to kObjectTypeArray or kObjectTypeDictionary based on their 
      // metatable.
      // FIXME What to do with FuncRefs?
    }
    case LUA_TUSERDATA: {
      // FIXME
    }
    case LUA_TNIL:
    default: {
      obj->type = kObjectTypeNil;
      break;
    }
  }
}

/// Evaluate lua string
///
/// Expects three values on the stack: string to evaluate, pointer to the 
/// location where result is saved, pointer to the location where error is 
/// saved. Always returns nothing (from the lua point of view).
static int nlua_eval_lua_string(lua_State *lstate) FUNC_ATTR_NONNULL_ALL
{
  String *str = (String *) lua_touserdata(lstate, 1);
  Object *obj = (Object *) lua_touserdata(lstate, 2);
  Error *err = (Error *) lua_touserdata(lstate, 3);
  lua_pop(lstate, 3);

  if (luaL_loadbuffer(lstate, str->data, str->size, NLUA_EVAL_NAME)) {
    set_lua_error(lstate, err);
    return 0;
  }
  if (lua_pcall(lstate, 0, 1, 0)) {
    set_lua_error(lstate, err);
    return 0;
  }
  convert_lua_to_object(lstate, -1, obj);
  lua_pop(lstate, -1);
  return 0;
}

/// Initialize lua interpreter state
///
/// Called by lua interpreter itself to initialize state.
static int nlua_state_init(lua_State *lstate) FUNC_ATTR_NONNULL_ALL
{
  return 0;
}

/// Initialize lua interpreter
///
/// Crashes NeoVim if initialization fails. Should be called once per lua 
/// interpreter instance.
static lua_State *init_lua(void)
  FUNC_ATTR_NONNULL_RET FUNC_ATTR_WARN_UNUSED_RESULT
{
  lua_State *lstate = luaL_newstate();
  if (lstate == NULL) {
    OUT_STR("Vim: Error: Failed to initialize lua interpreter.\n");
    preserve_exit();
  }
  luaL_openlibs(lstate);
  NLUA_CALL_C_FUNCTION_0(lstate, nlua_state_init, 0);
  return lstate;
}

static Object eval_lua_string(lua_State *lstate, String str, Error *err)
  FUNC_ATTR_NONNULL_ALL
{
  Object ret = {kObjectTypeNil, {false}};
  NLUA_CALL_C_FUNCTION_3(lstate, nlua_eval_lua_string, 0, &str, &ret, err);
  return ret;
}

static lua_State *global_lstate = NULL;

Object eval_lua(String str, Error *err)
{
  if (global_lstate == NULL)
    global_lstate = init_lua();

  return eval_lua_string(global_lstate, str, err);
}
