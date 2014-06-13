#include <stdbool.h>
#include <stddef.h>
#include "nvim/vim.h"
#include "nvim/viml/viml.h"
#include "nvim/viml/parser/ex_commands.h"
#include "nvim/viml/translator/translator.h"
#include "nvim/viml/executor/executor.h"
#include "nvim/api/private/defs.h"
#include "nvim/memory.h"
#include "nvim/garray.h"
#include "nvim/os/msgpack_rpc.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "viml/viml.c.generated.h"
#endif

/// Add given string to growarray
///
/// Has nearly the same interface as fwrite does.
static size_t ga_write(const void *ptr, size_t size, size_t nmemb, void *cookie)
  FUNC_ATTR_NONNULL_ALL
{
  garray_T *gap = (garray_T *) cookie;
  const char *s = (const char *) ptr;
  size_t len = size * nmemb;
  assert(size == 1);

  ga_grow(gap, len);
  memcpy(((char *) gap->ga_data) + gap->ga_len, s, len);
  gap->ga_len += len;

  return len;
}

/// Execute viml string
///
/// @param[in]  s  String which will be executed.
///
/// @return OK in case of success, FAIL otherwise.
int execute_viml(const char *const s)
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
  if (node == NULL)
    return FAIL;

  garray_T lua_ga;
  // TODO investigate what growsize will be the best here
  ga_init(&lua_ga, 1, 64);
  if (translate_input(node, &ga_write, (void *) &lua_ga) == FAIL)
    return FAIL;

  String lua_str = {
    .size = lua_ga.ga_len,
    .data = (char *) lua_ga.ga_data
  };
  Error err = {
    .set = false
  };
  // FIXME Propagate returns, breaks, continues, …
  Object lua_ret = eval_lua(lua_str, &err);
  if (err.set)
    return FAIL;
  msgpack_rpc_free_object(lua_ret);

  return OK;
}
