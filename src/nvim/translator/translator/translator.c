#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "nvim/vim.h"
#include "nvim/memory.h"

#include "nvim/translator/translator/translator.h"
#include "nvim/translator/parser/expressions.h"
#include "nvim/translator/parser/ex_commands.h"
#include "nvim/translator/parser/command_definitions.h"
#include "nvim/translator/printer/ex_commands.h"

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

typedef WFDEC((*AssignmentValueDump), void *dump_cookie);
typedef struct {
  CommandNode *node;
  size_t indent;
} TranslateFuncArgs;

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

  for (;;) {
    AddressFollowup *current_followup = current_range->address.followups;
    size_t followup_number = 0;

    assert(current_range->address.type != kAddrMissing);

    for (;;) {
      WS("vim.range.apply_followup(state, ")
      CALL(translate_address_followup, current_followup)
      followup_number++;
      current_followup = current_followup->next;
      if (current_followup == NULL)
        break;
      else
        WS(", ")
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

    current_range = current_range->next;
    if (current_range == NULL)
      break;
    else
      WS(", ")
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

static WFDEC(translate_number, ExpressionType type, char_u *s, char_u *e)
{
  WS("vim.number.new(state, ")
  switch (type) {
    case kExprHexNumber:
    case kExprDecimalNumber: {
      W_END(s, e)
      break;
    }
    case kExprOctalNumber: {
      unsigned long number;
      char_u saved_char = e[1];
      int n;

      e[1] = NUL;
      if ((n = sscanf((char *) s, "%lo", &number)) != (int) (e - s + 1)) {
        // TODO: check errno
        // TODO: give error message
        return FAIL;
      }
      e[1] = saved_char;
      CALL(dump_number, (long) number)
      break;
    }
    default: {
      assert(FALSE);
    }
  }
  WS(")")
  return OK;
}

#define TS_LAST_SEGMENT 0x01
#define TS_ONLY_SEGMENT 0x02
#define TS_FUNCCALL     0x04
#define TS_FUNCASSIGN   0x08

/// Dumps scope variable
///
/// @param[out]  start  Is set to the start of variable name, just after the 
///                     scope. If function failed to detect scope it is set to 
///                     NULL.
/// @param[in]   expr   Translated variable name.
/// @param[in]   flags  Flags.
/// @parblock
///   Supported flags:
///
///   TS_LAST_SEGMENT
///   :   Determines whether currently dumped segment is the last one: if it is 
///       then single character name like "a" may only refer to the variable in 
///       the current scope, if it is not it may be a construct like "a{':abc'}" 
///       which refers to "a:abc" variable.
///
///   TS_ONLY_SEGMENT
///   :   Determines whether this segment is the only one. Affects cases when 
///       "state.user_functions" is preferred over "state.current_scope".
///
///   TS_FUNCCALL
///   :   Use "state.functions" in place of "state.current_scope" in some cases. 
///       Note that vim.call implementation should still be able to use 
///       "state.user_functions" if appropriate.
///
///   TS_FUNCASSIGN
///   :   Use "state.user_functions" in place of "state.current_scope" in some 
///       cases.
/// @endparblock
///
/// @return FAIL in case of unrecoverable error, OK otherwise.
static WFDEC(translate_scope, char_u **start, ExpressionNode *expr,
             uint_least8_t flags)
{
  assert(expr->type == kExprSimpleVariableName);
  if (expr->end_position == expr->position) {
    if (!(flags & (TS_LAST_SEGMENT|TS_ONLY_SEGMENT))) {
      *start = NULL;
    } else {
      *start = expr->position;
      if ((flags & TS_FUNCCALL) && ASCII_ISLOWER(*expr->position))
        WS("state.functions")
      else if (flags & (TS_FUNCASSIGN|TS_ONLY_SEGMENT))
        WS("state.user_functions")
      else
        WS("state.current_scope")
    }
  } else if (expr->position[1] == ':') {
    switch (*expr->position) {
      case 's':
      case 'v':
      case 'a':
      case 'l':
      case 'g': {
        *start = expr->position + 2;
        WS("state.")
        W_LEN(expr->position, 1)
        break;
      }
      case 't': {
        *start = expr->position + 2;
        WS("state.tabpage.t")
        break;
      }
      case 'w': {
        *start = expr->position + 2;
        WS("state.window.w")
        break;
      }
      case 'b': {
        *start = expr->position + 2;
        WS("state.buffer.b")
        break;
      }
      default: {
        *start = expr->position;
        if (flags & (TS_FUNCASSIGN|TS_ONLY_SEGMENT))
          WS("state.user_functions")
        else
          WS("state.current_scope")
        break;
      }
    }
  } else {
    bool isfunc = FALSE;

    *start = expr->position;
    if ((flags & TS_FUNCCALL) && ASCII_ISLOWER(*expr->position)) {
      char_u *s;
      for (s = expr->position + 1; s != expr->end_position; s++) {
        if (!(ASCII_ISLOWER(*s) || VIM_ISDIGIT(*s))) {
          isfunc = TRUE;
          break;
        }
      }
    }
    if (isfunc && !(flags & TS_FUNCASSIGN))
      WS("state.functions")
    else if (flags & (TS_FUNCASSIGN|TS_ONLY_SEGMENT))
      WS("state.user_functions")
    else
      WS("state.current_scope")
  }
  return OK;
}

static WFDEC(translate_expr, ExpressionNode *expr)
{
  switch (expr->type) {
    case kExprFloat: {
      WS("vim.float.new(state, ")
      W_END(expr->position, expr->end_position)
      WS(")")
      break;
    }
    case kExprDecimalNumber:
    case kExprOctalNumber:
    case kExprHexNumber: {
      CALL(translate_number, expr->type, expr->position, expr->end_position)
      break;
    }
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
      WS("]")
      break;
    }
    case kExprEnvironmentVariable: {
      WS("state.environment[")
      W_EXPR_POS_ESCAPED(expr)
      WS("]")
      break;
    }
    case kExprSimpleVariableName: {
      char_u *start;
      WS("vim.subscript(state, ")
      CALL(translate_scope, &start, expr, TS_ONLY_SEGMENT)
      assert(start != NULL);
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
    case kExprExpression: {
      WS("(")
      CALL(translate_expr, expr->children)
      WS(")")
      break;
    }
    default: {
      ExpressionNode *current_expr;
      bool reversed = FALSE;

      assert(expr->children != NULL
             || expr->type == kExprDictionary
             || expr->type == kExprList);
      switch (expr->type) {
        case kExprDictionary: {
          WS("vim.dict.new(state, ")
          break;
        }
        case kExprList: {
          WS("vim.list.new(state, ")
          break;
        }
        case kExprSubscript: {
          if (expr->children->next->next == NULL)
            WS("vim.subscript(state, ")
          else
            WS("vim.slice(state, ")
        }

#define OPERATOR(op_type, op) \
        case op_type: { \
          WS("vim." op "(state, ") \
          break; \
        }

        OPERATOR(kExprAdd,          "add")
        OPERATOR(kExprSubtract,     "subtract")
        OPERATOR(kExprDivide,       "divide")
        OPERATOR(kExprMultiply,     "multiply")
        OPERATOR(kExprModulo,       "modulo")
        OPERATOR(kExprCall,         "call")
        OPERATOR(kExprMinus,        "negate")
        OPERATOR(kExprNot,          "negate_logical")
        OPERATOR(kExprPlus,         "promote_integer")
        OPERATOR(kExprStringConcat, "concat")
#undef OPERATOR

#define COMPARISON(forward_type, rev_type, op) \
        case forward_type: \
        case rev_type: { \
          if (expr->type == rev_type) { \
            reversed = TRUE; \
            WS("(") \
          } \
          WS("vim." op "(state, ") \
          CALL(dump_bool, (bool) expr->ignore_case) \
          WS(", ") \
          break; \
        }

        COMPARISON(kExprEquals,    kExprNotEquals,            "equals")
        COMPARISON(kExprIdentical, kExprNotIdentical,         "identical")
        COMPARISON(kExprMatches,   kExprNotMatches,           "matches")
        COMPARISON(kExprGreater,   kExprLessThanOrEqualTo,    "greater")
        COMPARISON(kExprLess,      kExprGreaterThanOrEqualTo, "less")
#undef COMPARISON

        default: {
          assert(FALSE);
        }
      }

      if (reversed)
        WS(" == 1 and 0 or 1)")

      current_expr = expr->children;
      for (;;) {
        CALL(translate_expr, current_expr)
        current_expr = current_expr->next;
        if (current_expr == NULL)
          break;
        else
          WS(", ")
      }
      WS(")")
      break;
    }
  }
  return OK;
}

static WFDEC(translate_exprs, ExpressionNode *expr)
{
  ExpressionNode *current_expr = expr;

  for (;;) {
    CALL(translate_expr, current_expr)
    current_expr = current_expr->next;
    if (current_expr == NULL)
      break;
    else
      WS(", ")
  }

  return OK;
}

static WFDEC(translate_function, TranslateFuncArgs *args)
{
  size_t i;
  char_u **data = (char_u **) args->node->args[ARG_FUNC_ARGS].arg.strs.ga_data;
  size_t len = (size_t) args->node->args[ARG_FUNC_ARGS].arg.strs.ga_len;
  WS("function(state")
  for (i = 0; i < len; i++) {
    WS(", ")
    W(data[i])
  }
  if (args->node->args[ARG_FUNC_FLAGS].arg.flags & FLAG_FUNC_VARARGS)
    WS(", ...")
  WS(")\n")
  if (args->node->children != NULL) {
    WINDENT(args->indent + 1)
    // TODO; dump information about function call
    WS("state = state:enter_function({})\n")
    // FIXME: there may appear nodes after return, instruct translate_nodes to 
    //        drop it or lua will fail
    CALL(translate_nodes, args->node->children, args->indent + 1)
  } else {
    // Empty function: do not bother creating scope dictionaries, just return 
    // zero
    WINDENT(args->indent + 1)
    WS("return vim.number.new(0)\n")
  }
  WINDENT(args->indent)
  WS("end")
  return OK;
}

/// Translate lvalue into one of vim.assign_* calls
///
/// @param[in]  expr         Translated expression.
/// @param[in]  is_funccall  True if it translates :function definition.
/// @param[in]  unique       True if assignment must be unique.
/// @param[in]  dump         Function used to dump value that will be assigned.
/// @param[in]  dump_cookie  First argument to the above function.
///
/// @return FAIL in case of unrecoverable error, OK otherwise.
static WFDEC(translate_lval, ExpressionNode *expr, bool is_funccall,
                             bool unique,
                             AssignmentValueDump dump, void *dump_cookie)
{
#define ADD_ASSIGN(what) \
  { \
    if (is_funccall) { \
      WS("vim.assign_" what "_function(state, ") \
      CALL(dump_bool, unique) \
      WS(", ") \
    } else { \
      WS("vim.assign_" what "(state, ") \
    } \
  }
  switch (expr->type) {
    case kExprSimpleVariableName: {
      char_u *start;
      ADD_ASSIGN("scope")
      CALL(dump, dump_cookie)
      WS(", ")
      CALL(translate_scope, &start, expr, TS_ONLY_SEGMENT|TS_LAST_SEGMENT
                                          |(is_funccall
                                            ? TS_FUNCASSIGN
                                            : 0))
      assert(start != NULL);
      WS(", '")
      W_END(start, expr->end_position)
      WS("')")
      break;
    }
    case kExprVariableName: {
      ExpressionNode *current_expr = expr->children;
      bool add_concat;

      assert(current_expr != NULL);

      ADD_ASSIGN("scope")
      CALL(dump, dump_cookie)
      WS(", ")

      if (current_expr->type == kExprIdentifier) {
        char_u *start;
        CALL(translate_scope, &start, expr, TS_ONLY_SEGMENT
                                            |(is_funccall
                                              ? TS_FUNCASSIGN
                                              : 0))
        if (start == NULL) {
          WS("vim.get_scope_and_key(state, '")
          W_EXPR_POS(expr)
          WS("'")
        } else {
          WS(", '")
          W_END(start, expr->end_position)
          WS("'")
        }
        add_concat = TRUE;
      } else {
        WS("vim.get_scope_and_key(state, ")
        add_concat = FALSE;
      }

      for (current_expr = current_expr->next; current_expr != NULL;
           current_expr = current_expr->next) {
        if (add_concat)
          WS(" .. ")
        else
          add_concat = TRUE;
        switch (current_expr->type) {
          case kExprIdentifier: {
            WS("'")
            W_EXPR_POS(current_expr)
            WS("'")
            break;
          }
          case kExprCurlyName: {
            WS("(")
            CALL(translate_expr, current_expr->children)
            WS(")")
            break;
          }
          default: {
            assert(FALSE);
          }
        }
      }
      WS(")")
      break;
    }
    case kExprConcatOrSubscript: {
      ADD_ASSIGN("dict")
      CALL(dump, dump_cookie)
      WS(", ")
      CALL(translate_expr, expr->children)
      WS(", '")
      W_EXPR_POS(expr)
      WS("')")
      break;
    }
    case kExprSubscript: {
      if (expr->children->next->next == NULL)
        ADD_ASSIGN("subscript")
      else
        ADD_ASSIGN("slice")
      CALL(dump, dump_cookie)
      WS(", ")
      CALL(translate_expr, expr->children)
      WS(", ")
      CALL(translate_expr, expr->children->next)
      if (expr->children->next->next != NULL) {
        WS(", ")
        CALL(translate_expr, expr->children->next->next)
      }
      WS(")")
      break;
    }
    default: {
      assert(FALSE);
    }
  }
  WS("\n")
  return OK;
#undef ADD_ASSIGN
}

static WFDEC(translate_node, CommandNode *node, size_t indent)
{
  char_u *name;
  size_t start_from_arg = 0;
  bool do_arg_dump = TRUE;
  bool add_comma = FALSE;

  if (node->type == kCmdEndwhile
      || node->type == kCmdEndfor
      || node->type == kCmdEndif
      || node->type == kCmdEndfunction
      || node->type == kCmdEndtry
      // kCmdCatch and kCmdFinally are handled in kCmdTry handler
      || node->type == kCmdCatch
      || node->type == kCmdFinally) {
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
             && node->args[ARG_FUNC_ARGS].arg.strs.ga_itemsize != 0) {
    TranslateFuncArgs args = {node, indent};
    CALL(translate_lval, node->args[ARG_FUNC_NAME].arg.expr, TRUE, !node->bang,
                         (AssignmentValueDump) &translate_function,
                         (void *) &args)
    return OK;
    return OK;
  } else if (node->type == kCmdFor) {
#define INDENTVAR(var, prefix) \
    char var[NUMBUFLEN + sizeof(prefix) - 1]; \
    size_t var##_len; \
    if ((var##_len = sprintf(var, prefix "%ld", (long) indent)) <= 1) \
      return FAIL;
#define ADDINDENTVAR(var) \
    W_LEN(var, var##_len)

    INDENTVAR(iter_var, "i")
    WS("local ")
    ADDINDENTVAR(iter_var)
    WS("\n")

    WINDENT(indent)
    WS("for _, ")
    ADDINDENTVAR(iter_var)
    WS(" in vim.list.iterator(state, ")
    CALL(translate_expr, node->args[ARG_FOR_RHS].arg.expr)
    WS(") do\n")

    // TODO assign variables

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
        WS("else")
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
  } else if (node->type  == kCmdTry) {
    CommandNode *first_catch = NULL;
    CommandNode *finally = NULL;
    CommandNode *next = node->next;

    for (next = node->next;
         next->type == kCmdCatch || next->type == kCmdFinally;
         next = next->next) {
      switch (next->type) {
        case kCmdCatch: {
          if (first_catch == NULL)
            first_catch = next;
          continue;
        }
        case kCmdFinally: {
          finally = next;
          break;
        }
        default: {
          assert(FALSE);
        }
      }
      break;
    }

    INDENTVAR(ok_var,      "ok")
    INDENTVAR(err_var,     "err")
    INDENTVAR(ret_var,     "ret")
    INDENTVAR(new_ret_var, "new_ret")
    INDENTVAR(fin_var,     "fin")
    INDENTVAR(catch_var,   "catch")

    WS("local ")
    ADDINDENTVAR(ok_var)
    WS(", ")
    ADDINDENTVAR(err_var)
    WS(", ")
    ADDINDENTVAR(ret_var)
    WS(" = pcall(function(state)\n")
    CALL(translate_nodes, node->children, indent + 1)
    WINDENT(indent)
    WS("end, state:trying())\n")

    if (finally != NULL) {
      WINDENT(indent)
      WS("local ")
      ADDINDENTVAR(fin_var)
      WS(" = function(state)\n")
      CALL(translate_nodes, finally->children, indent + 1)
      WINDENT(indent)
      WS("end\n")
    }

    if (first_catch != NULL) {
      WINDENT(indent)
      WS("local ")
      ADDINDENTVAR(catch_var)
      WINDENT(indent)
      WS("\n")
    }

    if (first_catch != NULL) {
      bool did_first_if = FALSE;

      WINDENT(indent)
      WS("if (not ")
      ADDINDENTVAR(ok_var)
      WS(")\n")

      for (next = first_catch; next->type == kCmdCatch; next = next->next) {
        size_t current_indent;

        if (did_first_if) {
          WINDENT(indent + 1)
          WS("else")
        }

        if (next->args[ARG_REG_REG].arg.reg == NULL) {
          if (did_first_if)
            WS("\n")
        } else {
          WINDENT(indent + 1)
          WS("if (vim.error.matches(state, ")
          ADDINDENTVAR(err_var)
          WS(", ")
          CALL(translate_regex, next->args[ARG_REG_REG].arg.reg)
          WS(")\n")
          did_first_if = TRUE;
        }
        current_indent = indent + 1 + (did_first_if ? 1 : 0);
        WINDENT(current_indent)
        ADDINDENTVAR(catch_var)
        WS(" = function(state)\n")
        CALL(translate_nodes, next->children, current_indent + 1)
        WINDENT(current_indent)
        WS("end\n")
        WINDENT(current_indent)
        ADDINDENTVAR(ok_var)
        WS(" = 'caught'\n")  // String "'caught'" is true

        if (next->args[ARG_REG_REG].arg.reg == NULL)
          break;
      }

      if (did_first_if) {
        WINDENT(indent + 1)
        WS("end\n")
      }

      WINDENT(indent)
      WS("end\n")
    }

#define ADD_VAR_CALL(var, indent) \
    { \
      WINDENT(indent) \
      WS("local ") \
      ADDINDENTVAR(new_ret_var) \
      WS(" = ") \
      ADDINDENTVAR(var) \
      WS("(state)\n") \
      WINDENT(indent) \
      WS("if (") \
      ADDINDENTVAR(new_ret_var) \
      WS(" ~= nil)\n") \
      WINDENT(indent + 1) \
      ADDINDENTVAR(ret_var) \
      WS(" = ") \
      ADDINDENTVAR(new_ret_var) \
      WS("\n") \
      WINDENT(indent) \
      WS("end\n") \
    }

    if (first_catch != NULL) {
      WINDENT(indent)
      WS("if (")
      ADDINDENTVAR(catch_var)
      WS(")\n")
      ADD_VAR_CALL(catch_var, indent + 1)
      WINDENT(indent)
      WS("end\n")
    }

    if (finally != NULL)
      ADD_VAR_CALL(fin_var, indent)

    WINDENT(indent)
    WS("if (not ")
    ADDINDENTVAR(ok_var)
    WS(")\n")
    WINDENT(indent + 1)
    WS("vim.err.propagate(state, ")
    ADDINDENTVAR(err_var)
    WS(")\n")
    WINDENT(indent)
    WS("end\n")

    WINDENT(indent)
    WS("if (")
    ADDINDENTVAR(ret_var)
    WS(" ~= nil)\n")
    WINDENT(indent + 1)
    WS("return ")
    ADDINDENTVAR(ret_var)
    WS("\n")
    WINDENT(indent)
    WS("end\n")

#undef ADD_VAR_CALL
#undef INDENTVAR
#undef ADDINDENTVAR

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
    if (add_comma)
      WS(", ")
    CALL(translate_range, &(node->range))
    add_comma = TRUE;
  }

  if (CMDDEF(node->type).flags & BANG) {
    if (add_comma)
      WS(", ")
    CALL(dump_bool, node->bang)
    add_comma = TRUE;
  }

  if (CMDDEF(node->type).flags & EXFLAGS) {
    if (add_comma)
      WS(", ")
    CALL(translate_ex_flags, node->exflags)
    add_comma = TRUE;
  }

  if (CMDDEF(node->type).parse == CMDDEF(kCmdAbclear).parse) {
    if (add_comma)
      WS(", ")
    CALL(dump_bool, (bool) node->args[ARG_CLEAR_BUFFER].arg.flags)
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdEcho).parse) {
    start_from_arg = 1;
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdCaddexpr).parse) {
    start_from_arg = 1;
  }

  if (do_arg_dump) {
    size_t i;
    size_t num_args = CMDDEF(node->type).num_args;

    for (i = start_from_arg; i < num_args; i++) {
      if (add_comma)
        WS(", ")
      add_comma = FALSE;
      switch (CMDDEF(node->type).arg_types[i]) {
        case kArgExpression: {
          CALL(translate_expr, node->args[i].arg.expr)
          add_comma = TRUE;
          break;
        }
        case kArgExpressions: {
          CALL(translate_exprs, node->args[i].arg.expr)
          add_comma = TRUE;
          break;
        }
        case kArgString: {
          CALL(dump_string, node->args[i].arg.str)
          add_comma = TRUE;
          break;
        }
        default: {
          break;
        }
      }
    }
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
     "s = vim.new_scope(false)\n"
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

  if ((node = parse_cmd_sequence(o, position, (LineGetter) &fgetline_file,
                                 stdin)) == NULL)
    return FAIL;

  ret = translate_script_stdout(node);

  free_cmd(node);
  return ret;
}

int translate_script_str_to_file(char_u *str, char *fname)
{
  CommandNode *node;
  CommandParserOptions o = {0, FALSE};
  CommandPosition position = {1, 1, (char_u *) "<test input>"};
  int ret;
  char_u **pp;
  FILE *f;

  pp = &str;

  if ((node = parse_cmd_sequence(o, position, (LineGetter) &fgetline_string,
                                 pp)) == NULL)
    return FAIL;

  if ((f = fopen(fname, "w")) == NULL)
    return FAIL;

  ret = translate_script(node, write_file, (void *) f);

  fclose(f);

  return ret;
}
