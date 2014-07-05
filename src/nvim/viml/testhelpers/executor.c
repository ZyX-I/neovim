#include <stddef.h>
#include <assert.h>
#include "nvim/types.h"
#include "nvim/api/private/defs.h"
#include "nvim/memory.h"
#include "nvim/os/msgpack_rpc_helpers.h"

#include "nvim/viml/viml.h"
#include "nvim/viml/parser/ex_commands.h"
#include "nvim/viml/translator/translator.h"
#include "nvim/viml/executor/executor.h"
#include "nvim/viml/testhelpers/object.h"
#include "nvim/viml/testhelpers/fgetline.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "viml/testhelpers/executor.c.generated.h"
#endif

char *execute_viml_test(const char *const s)
{
  CommandParserOptions o = {
    .flags = 0,            // FIXME add CPO, RL and ALTKEYMAP options
    .early_return = false
  };
  CommandPosition position = {
    .lnr = 1,
    .col = 1,
    .fname = (const char_u *) "<:execute string>"
  };
  Error err = {
    .set = false
  };
  char *const dup = xstrdup(s);

  CommandNode *node = parse_cmd_sequence(o, position,
                                         (VimlLineGetter) &fgetline_string,
                                         (void *) &dup);
  if (node == NULL) {
    return NULL;
  }

  String lua_str_test = {
    .data = "vim.ret = vim.list:new(nil)\n"
            "vim.commands.echo = function(state, ...)\n"
            "  vim.ret[#vim.ret + 1] = ...\n"
            "end"
  };
  lua_str_test.size = STRLEN(lua_str_test.data);
  Object lua_test_ret = eval_lua(lua_str_test, &err);
  if (err.set) {
    return NULL;
  }
  msgpack_rpc_free_object(lua_test_ret);

  size_t len = stranslate_len(kTransUser, node);
#define TEST_RET "return vim.ret"
  String lua_str = {
    .size = len,
    .data = xcalloc(len + sizeof(TEST_RET), 1)
  };
  char *p = lua_str.data;
  stranslate(kTransUser, node, &p);
  assert(p - lua_str.data <= (ptrdiff_t) lua_str.size);
  memcpy(p, TEST_RET, sizeof(TEST_RET));
  lua_str.size = (p - lua_str.data + sizeof(TEST_RET) - 1);
#undef TEST_RET

  Object lua_ret = eval_lua(lua_str, &err);
  if (err.set) {
    return NULL;
  }

  size_t ret_len = sdump_object_len(lua_ret);
  char *ret = xcalloc(ret_len + 1, 1);
  p = ret;
  sdump_object(lua_ret, &p);

  msgpack_rpc_free_object(lua_ret);

  return ret;
}
