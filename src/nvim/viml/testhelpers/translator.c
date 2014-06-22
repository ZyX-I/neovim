#include <stdio.h>

#include "nvim/vim.h"
#include "nvim/types.h"

#include "nvim/viml/parser/ex_commands.h"
#include "nvim/viml/testhelpers/fgetline.h"
#include "nvim/viml/translator/translator.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "viml/testhelpers/translator.c.generated.h"
#endif

static Writer write_file = (Writer) &fwrite;

/// Translate given sequence of nodes as a .vim script and dump result to stdout
///
/// @param[in]  node  Pointer to the first command inside this script.
///
/// @return OK in case of success, FAIL otherwise.
static int translate_script_stdout(const CommandNode *const node)
{
  return translate(kTransScript, node, write_file, (void *) stdout);
}

/// Translate script passed through stdin to stdout
///
/// @return OK in case of success, FAIL otherwise.
int translate_script_std(void)
{
  CommandNode *node;
  CommandParserOptions o = {0, false};
  CommandPosition position = {1, 1, (char_u *) "<test input>"};
  int ret;

  if ((node = parse_cmd_sequence(o, position, (LineGetter) &fgetline_file,
                                 stdin)) == NULL)
    return FAIL;

  ret = translate_script_stdout(node);

  free_cmd(node);
  return ret;
}

/// Translate script passed as a single string to given file
///
/// @param[in]  str    Translated script.
/// @param[in]  fname  Target filename.
///
/// @return OK in case of success, FAIL otherwise.
int translate_script_str_to_file(const char_u *const str,
                                 const char *const fname)
{
  CommandNode *node;
  CommandParserOptions o = {0, false};
  CommandPosition position = {1, 1, (char_u *) "<test input>"};
  int ret;
  char_u **pp;
  FILE *f;

  pp = (char_u **) &str;

  if ((node = parse_cmd_sequence(o, position, (LineGetter) &fgetline_string,
                                 pp)) == NULL)
    return FAIL;

  if ((f = fopen(fname, "w")) == NULL)
    return FAIL;

  ret = translate(kTransScript, node, write_file, (void *) f);

  fclose(f);

  return ret;
}
