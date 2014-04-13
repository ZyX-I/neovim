#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "nvim/vim.h"
#include "nvim/memory.h"

#include "nvim/translator/translator/translator.h"
#include "nvim/translator/parser/expressions.h"
#include "nvim/translator/parser/ex_commands.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "translator/translator/translator.c.generated.h"
#endif

#define CALL(f, ...) \
    if (f(__VA_ARGS__, write, cookie) == FAIL) \
      return FAIL;
#define CALL_ERR(error_label, f, ...) \
    if (f(__VA_ARGS__, write, cookie) == FAIL) \
      goto error_label;
#define W_LEN(s, len) \
    CALL(write_string_len, s, len)
#define W_LEN_ERR(s, len) \
    CALL_ERR(write_string_len, s, len)
#define W(s) W_LEN(s, STRLEN(s))
#define W_ERR(s) W_LEN_ERR(s, STRLEN(s))
#define WFDEC(f, ...) int f(__VA_ARGS__, Writer write, void *cookie)
#define WINDENT(indent) \
    { \
      for (size_t i = 0; i < indent; i++) \
        W("  ") \
    }

static WFDEC(write_string_len, char *s, size_t len)
{
  size_t written;
  if (len) {
    written = write(s, 1, len, cookie);
    if (written < len) {
      // TODO: generate error message
      return FAIL;
    }
  }
  return OK;
}

static Writer write_file = (Writer) &fwrite;

static WFDEC(translate_node, CommandNode *node, size_t indent)
{
  WINDENT(indent);

  switch (node->type) {
    case kCmdAbclear: {
      W("vim.commands.")
      W((char *) CMDDEF(node->type).name)
      W("(state, ")
      if (node->args[ARG_CLEAR_BUFFER].arg.flags) {
        W("true")
      } else {
        W("false")
      }
      W(")\n")
      break;
    }
    default: {
      assert(FALSE);
    }
  }
  return OK;
}

int translate_script(CommandNode *node, Writer write, void *cookie)
{
  CommandNode *current_node = node;
  W("vim = require 'vim'\n"
    "s = vim.new_scope()\n"
    "return {\n"
    "  run=function(state)\n"
    "    state = state:set_script_locals(s)\n")

  while (current_node != NULL) {
    CALL(translate_node, current_node, 2)
    current_node = current_node->next;
  }

  W("  end\n"
    "}\n")
  return OK;
}

static int translate_script_stdout(CommandNode *node)
{
  return translate_script(node, write_file, (void *) stdout);
}

static char_u *fgetline_file(int c, FILE *file, int indent)
{
  char *res;
  size_t len;

  if ((res = ALLOC_CLEAR_NEW(char, 1024)) == NULL)
    return NULL;

  if (fgets(res, 1024, file) == NULL)
    return NULL;

  len = STRLEN(res);

  if (res[len] == '\n')
    res[len] = NUL;

  return (char_u *) res;
}

int translate_script_std(void)
{
  CommandNode *node;
  CommandParserOptions o = {0, FALSE};
  CommandPosition position = {1, 1, (char_u *) "<test input>"};
  int ret;

  if ((node = parse_cmd_sequence(o, position, (LineGetter) fgetline_file,
                                 stdin)) == NULL)
    return FAIL;

  ret = translate_script_stdout(node);

  free_cmd(node);
  return ret;
}
