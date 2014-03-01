#include <stddef.h>
#include <assert.h>
#include "nvim/api/private/defs.h"
#include "nvim/memory.h"

#include "nvim/viml/viml.h"
#include "nvim/viml/parser/ex_commands.h"
#include "nvim/viml/translator/translator.h"
#include "nvim/viml/executor/executor.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "viml/viml.c.generated.h"
#endif

/// Execute viml string
///
/// @param[in]  s  String which will be executed.
///
/// @return OK in case of success, FAIL otherwise.
Object execute_viml(const char *const s)
  FUNC_ATTR_NONNULL_ALL
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
  char *const dup = xstrdup(s);

  CommandNode *node = parse_cmd_sequence(o, position,
                                         (LineGetter) &do_fgetline_allocated,
                                         (void *) &dup);
  if (node == NULL) {
    return (Object) {.type = kObjectTypeNil};
  }

  size_t len = stranslate_len(kTransUser, node);
  String lua_str = {
    .size = len,
    .data = xcalloc(len, 1)
  };
  char *p = lua_str.data;
  stranslate(kTransUser, node, &p);
  assert(p - lua_str.data <= (ptrdiff_t) lua_str.size);
  lua_str.size = (p - lua_str.data);

  Error err = {
    .set = false
  };
  // FIXME Propagate returns, breaks, continues, …
  Object lua_ret = eval_lua(lua_str, &err);
  if (err.set) {
    return (Object) {.type = kObjectTypeNil};
  }

  return lua_ret;
}
