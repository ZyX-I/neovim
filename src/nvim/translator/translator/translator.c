#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "nvim/vim.h"
#include "nvim/memory.h"

#include "nvim/translator/translator/translator.h"
#include "nvim/translator/parser/expressions.h"
#include "nvim/translator/parser/ex_commands.h"
#include "nvim/translator/parser/command_definitions.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "translator/translator/translator.c.generated.h"
#endif

#define WFDEC(f, ...) int f(__VA_ARGS__, Writer write, void *cookie)
#define CALL(f, ...) \
    { \
      if (f(__VA_ARGS__, write, cookie) == FAIL) \
        return FAIL; \
    }
#define CALL_ERR(error_label, f, ...) \
    { \
      if (f(__VA_ARGS__, write, cookie) == FAIL) \
        goto error_label; \
    }
#define W_LEN(s, len) \
    CALL(write_string_len, (char *) s, len)
#define W_LEN_ERR(error_label, s, len) \
    CALL_ERR(error_label, write_string_len, (char *) s, len)
#define W(s) \
    W_LEN(s, STRLEN(s))
#define W_ERR(error_label, s) \
    W_LEN_ERR(error_label, s, STRLEN(s))
#define W_END(s, e) \
    W_LEN(s, e - s + 1)
#define W_END_ERR(error_label, s, e) \
    W_LEN_ERR(error_label, s, e - s + 1)
#define W_EXPR_POS(expr) \
    W_END(expr->position, expr->end_position)
#define W_EXPR_POS_ERR(error_label, expr) \
    W_END_ERR(error_label, expr->position, expr->end_position)
#define W_EXPR_POS_ESCAPED(expr) \
    CALL(dump_string_len, expr->position, \
         expr->end_position - expr->position + 1)
#define W_EXPR_POS_ESCAPED_ERR(error_label, expr) \
    CALL_ERR(error_label, dump_string_len, expr->position, \
             expr->end_position - expr->position + 1)
#define WS(s) \
    W_LEN(s, sizeof(s) - 1)
#define WS_ERR(error_label, s) \
    W_LEN_ERR(error_label, s, sizeof(s) - 1)
#define WINDENT(indent) \
    { \
      for (size_t i = 0; i < indent; i++) \
        WS("  ") \
    }
#define WINDENT_ERR(error_label, indent) \
    { \
      for (size_t i = 0; i < indent; i++) \
        WS_ERR(error_label, "  ") \
    }

// {{{ Function declarations
static WFDEC(write_string_len, char *s, size_t len);
static WFDEC(dump_number, long number);
static WFDEC(dump_string, char_u *s);
static WFDEC(dump_bool, bool b);
static WFDEC(translate_regex, Regex *regex);
static WFDEC(translate_address_followup, AddressFollowup *followup);
static WFDEC(translate_range, Range *range);
static WFDEC(translate_ex_flags, uint_least8_t exflags);
static WFDEC(translate_node, CommandNode *node, size_t indent);
static WFDEC(translate_nodes, CommandNode *node, size_t indent);
static int translate_script_stdout(CommandNode *node);
static char_u *fgetline_file(int c, FILE *file, int indent);
// }}}

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

/// Dump number that is not a vim Number
///
/// Use translate_number to dump vim Numbers (kExpr*Number)
static WFDEC(dump_number, long number)
{
  char buf[NUMBUFLEN];

  if (sprintf(buf, "%ld", number) == 0)
    return FAIL;

  W(buf)

  return OK;
}

/// Dump string that is not a vim String
///
/// Use translate_string to dump vim String (kExpr*String)
static WFDEC(dump_string, char_u *s)
{
  // FIXME escape characters
  WS("'")
  W(s)
  WS("'")
  return OK;
}

/// Dump string that is not a vim String
///
/// Use translate_string to dump vim String (kExpr*String)
static WFDEC(dump_string_len, char_u *s, size_t len)
{
  // FIXME escape characters
  WS("'")
  W_LEN(s, len)
  WS("'")
  return OK;
}

static WFDEC(dump_bool, bool b)
{
  if (b)
    WS("true")
  else
    WS("false")

  return OK;
}

static WFDEC(translate_regex, Regex *regex)
{
  CALL(dump_string, regex->string)
  return OK;
}

static WFDEC(translate_address_followup, AddressFollowup *followup)
{
  switch (followup->type) {
    case kAddressFollowupShift: {
      WS("0, ")
      CALL(dump_number, (long) followup->data.shift)
      break;
    }
    case kAddressFollowupForwardPattern: {
      WS("1, ")
      CALL(dump_string, followup->data.regex->string)
      break;
    }
    case kAddressFollowupBackwardPattern: {
      WS("2, ")
      CALL(dump_string, followup->data.regex->string)
      break;
    }
    case kAddressFollowupMissing: {
      assert(FALSE);
    }
  }
  return OK;
}

static WFDEC(translate_range, Range *range)
{
  Range *current_range = range;

  if (current_range->address.type == kAddrMissing) {
    WS("nil")
    return OK;
  }

  WS("vim.range.compose(state, ")

  while (current_range != NULL) {
    AddressFollowup *current_followup = current_range->address.followups;
    size_t followup_number = 0;

    assert(current_range->address.type != kAddrMissing);

    while (current_followup != NULL) {
      WS("vim.range.apply_followup(state, ")
      CALL(translate_address_followup, current_followup)
      WS(", ")
      followup_number++;
      current_followup = current_followup->next;
    }

    switch (current_range->address.type) {
      case kAddrFixed: {
        CALL(dump_number, (long) current_range->address.data.lnr);
        break;
      }
      case kAddrEnd: {
        WS("vim.range.last(state)")
        break;
      }
      case kAddrCurrent: {
        WS("vim.range.current(state)")
        break;
      }
      case kAddrMark: {
        char_u mark[2] = {current_range->address.data.mark, NUL};

        WS("vim.range.mark(state, '")
        CALL(dump_string, mark)
        WS("')")
        break;
      }
      case kAddrForwardSearch: {
        WS("vim.range.forward_search(state, ")
        CALL(translate_regex, current_range->address.data.regex)
        WS(")")
        break;
      }
      case kAddrBackwardSearch: {
        WS("vim.range.backward_search(state, ")
        CALL(translate_regex, current_range->address.data.regex)
        WS(")")
        break;
      }
      case kAddrSubstituteSearch: {
        WS("vim.range.substitute_search(state)")
        break;
      }
      case kAddrForwardPreviousSearch: {
        WS("vim.range.forward_previous_search(state)")
        break;
      }
      case kAddrBackwardPreviousSearch: {
        WS("vim.range.backward_previous_search(state)")
        break;
      }
      case kAddrMissing: {
        assert(FALSE);
      }
    }

    while (followup_number--) {
      WS(")")
    }
    WS(", ")

    CALL(dump_bool, current_range->setpos)
    WS(", ")

    current_range = current_range->next;
  }

  WS(")")
  return OK;
}

static WFDEC(translate_ex_flags, uint_least8_t exflags)
{
  WS("{")
  if (exflags & FLAG_EX_LIST)
    WS("list=true, ")
  if (exflags & FLAG_EX_LNR)
    WS("lnr=true, ")
  if (exflags & FLAG_EX_PRINT)
    WS("print=true, ")
  WS("}")
  return OK;
}

static WFDEC(translate_expr, ExpressionNode *expr)
{
  switch (expr->type) {
    case kExprOption: {
      // TODO
      break;
    }
    case kExprRegister: {
      WS("state.registers[")
      if (expr->end_position < expr->position) {
        WS("nil")
      } else {
        CALL(dump_string_len, expr->position, 1)
      }
      break;
    }
    case kExprEnvironmentVariable: {
      WS("state.environment[")
      W_EXPR_POS_ESCAPED(expr)
      WS("]")
      break;
    }
    case kExprSimpleVariableName: {
      char_u *start = expr->position;
      WS("vim.subscript(state, ")
      if (expr->position[1] == ':') {
        start += 2;
        switch (*expr->position) {
          case 's':
          case 'v':
          case 'a':
          case 'l':
          case 'g': {
            WS("state.")
            W_LEN(expr->position, 1)
            break;
          }
          case 't': {
            WS("state.tabpage.t")
            break;
          }
          case 'w': {
            WS("state.window.w")
            break;
          }
          case 'b': {
            WS("state.buffer.b")
            break;
          }
          default: {
            start -= 2;
            WS("state.current_scope")
            break;
          }
        }
      } else {
        WS("state.current_scope")
      }
      WS(", '")
      W_END(start, expr->end_position)
      WS("')")
      break;
    }
    case kExprVariableName: {
      // TODO
      break;
    }
    case kExprCurlyName: {
      // TODO
      break;
    }
    case kExprIdentifier: {
      // TODO
      break;
    }
    case kExprConcatOrSubscript: {
      WS("vim.concat_or_subscript(state, ")
      W_EXPR_POS_ESCAPED(expr)
      WS(", ")
      CALL(translate_expr, expr->children)
      WS(")")
      break;
    }
    case kExprEmptySubscript: {
      WS("nil")
      break;
    }
    default: {
      ExpressionNode *current_expr;
      assert(expr->children != NULL);
      switch (expr->type) {
        case kExprDictionary: {
          WS("vim.dict(state, ")
          break;
        }
        case kExprList: {
          WS("vim.list(state, ")
          break;
        }
        case kExprSubscript: {
          if (expr->children->next->next == NULL)
            WS("vim.subscript(state, ")
          else
            WS("vim.slice(state, ")
        }
        case kExprAdd: {
          WS("vim.add(state, ")
          break;
        }
        case kExprSubtract: {
          WS("vim.subtract(state, ")
          break;
        }
        case kExprDivide: {
          WS("vim.divide(state, ")
          break;
        }
        case kExprMultiply: {
          WS("vim.multiply(state, ")
          break;
        }
        case kExprModulo: {
          WS("vim.modulo(state, ")
          break;
        }
        case kExprCall: {
          WS("vim.call(state, ")
          break;
        }
        case kExprMinus: {
          WS("vim.negate(state, ")
          break;
        }
        case kExprNot: {
          WS("vim.negate_logical(state, ")
          break;
        }
        case kExprPlus: {
          WS("vim.promote_integer(state, ")
          break;
        }
        default: {
          assert(FALSE);
        }
      }

      current_expr = expr->children;
      while (current_expr != NULL) {
        CALL(translate_expr, current_expr)
        current_expr = current_expr->next;
      }
      WS(")")
      break;
    }
  }
  return OK;
}

static WFDEC(translate_node, CommandNode *node, size_t indent)
{
  char_u *name;

  if (node->type == kCmdEndwhile
      || node->type == kCmdEndfor
      || node->type == kCmdEndif) {
    return OK;
  }

  WINDENT(indent);

  if (node->type == kCmdComment) {
    WS("--")
    W(node->args[ARG_NAME_NAME].arg.str)
    WS("\n")
    return OK;
  } else if (node->type == kCmdHashbangComment) {
    WS("-- #!")
    W(node->args[ARG_NAME_NAME].arg.str)
    WS("\n")
    return OK;
  } else if (node->type == kCmdSyntaxError) {
    WS("vim.error(state, ")
    // FIXME Dump error information
    WS(")\n")
    return OK;
  } else if (node->type == kCmdMissing) {
    WS("\n")
    return OK;
  } else if (node->type == kCmdUSER) {
    WS("vim.run_user_command(state, '")
    W(node->name)
    WS("', ")
    CALL(translate_range, &(node->range))
    WS(", ")
    CALL(dump_bool, node->bang)
    WS(", ")
    CALL(dump_string, node->args[ARG_USER_ARG].arg.str)
    WS(")\n")
    return OK;
  } else if (node->type == kCmdFunction
             && node->args[ARG_FUNC_ARGS].arg.strs.ga_growsize != 0) {
    // TODO
    return OK;
  } else if (node->type == kCmdFor) {
    char iter_var[NUMBUFLEN + 1];
    size_t iter_var_len;
    if ((iter_var_len = sprintf(iter_var, "i%ld", (long) indent)) <= 1)
      return FAIL;
    W_LEN(iter_var, iter_var_len)
    WS(" = vim.iter_start(state, ")
    // FIXME dump list
    WS(")\n")

    WINDENT(indent)
    WS("while (")
    W_LEN(iter_var, iter_var_len)
    WS(" ~= nil) do\n")

    WINDENT(indent + 1)
    W_LEN(iter_var, iter_var_len)
    WS(", ")
    // TODO assign variables
    WS(" = ")
    W_LEN(iter_var, iter_var_len)
    WS(":next()\n")

    CALL(translate_nodes, node->children, indent + 1)

    WINDENT(indent)
    WS("end\n")
    return OK;
  } else if (node->type == kCmdWhile) {
    WS("while (")
    CALL(translate_expr, node->args[ARG_EXPR_EXPR].arg.expr)
    WS(") do\n")

    CALL(translate_nodes, node->children, indent + 1)

    WINDENT(indent)
    WS("end\n")
    return OK;
  } else if (node->type == kCmdIf
             || node->type == kCmdElseif
             || node->type == kCmdElse) {
    switch (node->type) {
      case kCmdElse: {
        WS("else\n")
        break;
      }
      case kCmdElseif: {
        WS("else ")
        // fallthrough
      }
      case kCmdIf: {
        WS("if (")
        CALL(translate_expr, node->args[ARG_EXPR_EXPR].arg.expr)
        WS(") then\n")
        break;
      }
      default: {
        assert(FALSE);
      }
    }

    CALL(translate_nodes, node->children, indent + 1)

    if (node->next == NULL
        || (node->next->type != kCmdElseif
            && node->next->type != kCmdElse)) {
      WINDENT(indent)
      WS("end\n")
    }
    return OK;
  }

  name = CMDDEF(node->type).name;
  assert(name != NULL);

  WS("vim.commands")
  if (ASCII_ISALPHA(*name)) {
    WS(".")
    W(name)
  } else {
    WS("['")
    W(name)
    WS("']")
  }
  WS("(state, ")

  if (CMDDEF(node->type).flags & RANGE) {
    CALL(translate_range, &(node->range))
    WS(", ")
  }

  if (CMDDEF(node->type).flags & BANG) {
    CALL(dump_bool, node->bang)
    WS(", ")
  }

  if (CMDDEF(node->type).flags & EXFLAGS) {
    CALL(translate_ex_flags, node->exflags)
    WS(", ")
  }

  if (CMDDEF(node->type).parse == CMDDEF(kCmdAbclear).parse) {
    CALL(dump_bool, (bool) node->args[ARG_CLEAR_BUFFER].arg.flags)
  }

  WS(")\n")

  return OK;
}

static WFDEC(translate_nodes, CommandNode *node, size_t indent)
{
  CommandNode *current_node = node;

  while (current_node != NULL) {
    CALL(translate_node, current_node, indent)
    current_node = current_node->next;
  }

  return OK;
}

int translate_script(CommandNode *node, Writer write, void *cookie)
{
  WS("vim = require 'vim'\n"
     "s = vim.new_scope()\n"
     "return {\n"
     "  run=function(state)\n"
     "    state = state:set_script_locals(s)\n")

  CALL(translate_nodes, node, 2)

  WS("  end\n"
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

  if (res[len - 1] == '\n')
    res[len - 1] = NUL;

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
