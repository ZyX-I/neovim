// vim: ts=8 sts=2 sw=2 tw=80

//
// Copyright 2014 Nikolay Pavlov

// cmd.c: Ex commands parsing

#include "vim.h"
#include "types.h"
#include "cmd.h"
#include "expr.h"
#include "ex_cmds_defs.h"
#define DO_DECLARE_EXCMD_NOFUNC
#include "ex_cmds_defs.h"
#undef DO_DECLARE_EXCMD_NOFUNC
#include "misc2.h"


static CommandNode *cmd_alloc(CommandType type, CommandParserError error)
{
  // XXX May allocate less space then needed to hold the whole struct: less by 
  // one size of CommandArg.
  size_t size = sizeof(CommandNode)
              + (sizeof(CommandArg) * (cmdnames[type].num_args));
  CommandNode *node = (CommandNode *) alloc_clear(size);

  if (node != NULL)
    node->type = type;

  return node;
}

static void free_menu_item(MenuItem *menu_item)
{
  if (menu_item == NULL)
    return;

  free_menu_item(menu_item->subitem);
  vim_free(menu_item->name);
}

static void free_cmd(CommandNode *node);

static void free_cmd_arg(CommandArg *arg)
{
  switch(arg->type) {
    case kArgCommand: {
      free_cmd(arg->arg.command);
      break;
    }
    case kArgExpression:
    case kArgExpressions:
    case kArgAssignLhs: {
      free_expr(arg->arg.expr);
      break;
    }
    case kArgFlags:
    case kArgNumber:
    case kArgAuEvent:
    case kArgChar: {
      break;
    }
    case kArgNumbers: {
      vim_free(arg->arg.numbers);
      break;
    }
    case kArgString: {
      vim_free(arg->arg.str);
      break;
    }
    case kArgPattern: {
      // FIXME
      break;
    }
    case kArgGlob: {
      // FIXME
      break;
    }
    case kArgRegex: {
      // FIXME
      break;
    }
    case kArgReplacement: {
      // FIXME
      break;
    }
    case kArgLines:
    case kArgStrings: {
      char_u *s = *(arg->arg.strs);
      while (s != NULL) {
        vim_free(s);
        ++s;
      }
      vim_free(arg->arg.strs);
      break;
    }
    case kArgMenuName: {
      free_menu_item(arg->arg.menu_item);
      break;
    }
    case kArgAuEvents: {
      vim_free(arg->arg.events);
      break;
    }
    case kArgAddress: {
      // FIXME
      break;
    }
    case kArgCmdComplete: {
      // FIXME
      break;
    }
    case kArgArgs: {
      CommandArg *subargs = arg->arg.args.args;
      size_t numsubargs = arg->arg.args.num_args;
      size_t i;
      for (i = 0; i < numsubargs; i++)
        free_cmd_arg(&(subargs[i]));
      vim_free(subargs);
      break;
    }
  }
}

void free_cmd(CommandNode *node)
{
  size_t numargs = cmdnames[node->type].num_args;
  size_t i;

  if (node == NULL)
    return;

  for (i = 0; i < numargs; i++)
    free_cmd_arg(&(node->args[i]));

  vim_free(node);
}
