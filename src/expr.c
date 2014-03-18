// vim: ts=8 sts=2 sw=2 tw=80
//
// NeoVim - Neo Vi IMproved
//
// Do ":help uganda"  in Vim to read copying and usage conditions.
// Do ":help credits" in Vim to see a list of people who contributed.
// See README.txt for an overview of the Vim source code.
//
// Copyright 2014 Nikolay Pavlov

// expr.c: Expression parsing

#include "vim.h"
#include "misc2.h"
#include "types.h"
#include "expr.h"
#include "charset.h"

#define MAX_FUNC_ARGS   20      /* maximum number of function arguments */

#define UP_NODE(type, error, old_top_node, top_node, next_node) \
  { \
    if ((top_node = expr_alloc(type, error)) == NULL) \
      return FAIL; \
    next_node = &((*old_top_node)->next); \
    top_node->children = *old_top_node; \
    *old_top_node = top_node; \
  }

#define TOP_NODE(type, error, old_top_node, top_node, next_node) \
  { \
    if ((top_node = expr_alloc(type, error)) == NULL) \
      return FAIL; \
    next_node = &(top_node->children); \
    *old_top_node = top_node; \
  }

#define VALUE_NODE(type, error, node, pos, end_pos) \
  { \
    if ((*node = expr_alloc(type, error)) == NULL) \
      return FAIL; \
    (*node)->position = pos; \
    (*node)->end_position = end_pos; \
  }

#define IS_SCOPE_CHAR(c) (   (c) == 'g' || (c) == 'b' || (c) == 'w' \
                          || (c) == 't' || (c) == 'v' || (c) == 'a' \
                          || (c) == 'l' || (c) == 's')

//{{{ Function declarations
static ExpressionNode *expr_alloc(ExpressionType type,
                                  ExpressionParserError *error);
static int isnamechar(int c);
static char_u *find_id_end(char_u **arg);
static int get_fname_script_len(char_u *p);
static int parse_name(char_u **arg,
                      ExpressionNode **node,
                      ExpressionParserError *error,
                      ExpressionNode *parse1_node,
                      char_u *parse1_arg);
static int parse_list(char_u **arg,
                      ExpressionNode **node,
                      ExpressionParserError *error);
static int parse_dictionary(char_u **arg,
                            ExpressionNode **node,
                            ExpressionParserError *error,
                            ExpressionNode **parse1_node,
                            char_u **parse1_arg);
static char_u *find_option_end(char_u **arg);
static int parse_option(char_u **arg,
                        ExpressionNode **node,
                        ExpressionParserError *error);
static char_u *find_env_end(char_u **arg);
static int parse_environment_variable(char_u **arg,
                                      ExpressionNode **node,
                                      ExpressionParserError *error);
static int parse_dot_subscript(char_u **arg,
                               ExpressionNode **node,
                               ExpressionParserError *error);
static int parse_func_call(char_u **arg,
                           ExpressionNode **node,
                           ExpressionParserError *error);
static int parse_subscript(char_u **arg,
                           ExpressionNode **node,
                           ExpressionParserError *error);
static int handle_subscript(char_u **arg,
                            ExpressionNode **node,
                            ExpressionParserError *error);
static int parse7(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error,
                  int want_string);
static int parse6(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error,
                  int want_string);
static int parse5(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error);
static int parse4(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error);
static int parse23(char_u **arg,
                   ExpressionNode **node,
                   ExpressionParserError *error,
                   int level);
static int parse3(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error);
static int parse2(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error);
static int parse1(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error);
static size_t node_repr_len(ExpressionNode *node);
static void node_repr(ExpressionNode *node, char **pp);
//}}}

static ExpressionNode *expr_alloc(ExpressionType type,
                                  ExpressionParserError *error)
{
  ExpressionNode *node;

  if ((node = ALLOC_CLEAR_NEW(ExpressionNode, 1)) == NULL)
    return NULL;

  node->type = type;
  return node;
}

void free_expr(ExpressionNode *node)
{
  if (node == NULL)
    return;

  free_expr(node->children);
  free_expr(node->next);
  vim_free(node);
}

/*
 * Return TRUE if character "c" can be used in a variable or function name.
 * Does not include '{' or '}' for magic braces.
 */
static int isnamechar(int c)
{
  return ASCII_ISALNUM(c) || c == '_' || c == ':' || c == AUTOLOAD_CHAR;
}

/*
 * Find the end of the name of a function or internal variable.
 * "arg" is advanced to the first non-white character after the name.
 * Return NULL if something is wrong.
 */
static char_u *find_id_end(char_u **arg)
{
  char_u *p;

  // Find the end of the name.
  for (p = *arg; isnamechar(*p); p++) {
  }
  if (p == *arg)  // no name found
    return NULL;
  *arg = p;
  return p - 1;
}

/*
 * Return 5 if "p" starts with "<SID>" or "<SNR>" (ignoring case).
 * Return 2 if "p" starts with "s:".
 * Return 0 otherwise.
 */
static int get_fname_script_len(char_u *p)
{
  if (p[0] == '<' && (   STRNICMP(p + 1, "SID>", 4) == 0
                      || STRNICMP(p + 1, "SNR>", 4) == 0))
    return 5;
  if (p[0] == 's' && p[1] == ':')
    return 2;
  return 0;
}

/*
 * Only the name is recognized, does not handle ".key" or "[idx]".
 * "arg" is advanced to the first non-white character after the name.
 */
static int parse_name(char_u **arg,
                      ExpressionNode **node,
                      ExpressionParserError *error,
                      ExpressionNode *parse1_node,
                      char_u *parse1_arg)
{
  int len;
  char_u *p;
  char_u *s;
  ExpressionNode *top_node = NULL;
  ExpressionNode **next_node = node;

  TOP_NODE(kTypeVariableName, error, node, top_node, next_node)

  if (parse1_node == NULL) {
    s = *arg;
    if (   (*arg)[0] == K_SPECIAL
        && (*arg)[1] == KS_EXTRA
        && (*arg)[2] == (int)KE_SNR) {
      // hard coded <SNR>, already translated
      *arg += 3;
      if ((p = find_id_end(arg)) == NULL) {
        // XXX Note: vim does not have special error message for this
        error->message = N_("E15: expected variable name");
        error->position = *arg;
        return FAIL;
      }
      (*node)->type = kTypeSimpleVariableName;
      (*node)->position = s;
      (*node)->end_position = p;
      return OK;
    }

    len = get_fname_script_len(*arg);
    if (len > 0) {
      // literal "<SID>", "s:" or "<SNR>"
      *arg += len;
    }

    p = find_id_end(arg);

    if (p == NULL && len)
      p = *arg - 1;

    if (**arg != '{') {
      if (p == NULL) {
        // XXX Note: vim does not have special error message for this
        error->message = N_("E15: expected expr7 (value)");
        error->position = *arg;
        return FAIL;
      }
      (*node)->type = kTypeSimpleVariableName;
      (*node)->position = s;
      (*node)->end_position = p;
      return OK;
    }
  } else {
    VALUE_NODE(kTypeCurlyName, error, next_node, *arg, NULL);
    (*next_node)->children = parse1_node;
    next_node = &((*next_node)->next);
    *arg = parse1_arg + 1;
    s = *arg;
    p = find_id_end(arg);
  }

  while (**arg == '{') {
    if (p != NULL) {
      VALUE_NODE(kTypeIdentifier, error, next_node, s, p)
      next_node = &((*next_node)->next);
    }

    s = *arg;
    (*arg)++;
    *arg = skipwhite(*arg);

    VALUE_NODE(kTypeCurlyName, error, next_node, s, NULL)

    if ((parse1(arg, &((*next_node)->children), error)) == FAIL)
      return FAIL;

    if (**arg != '}') {
      // XXX Note: vim does not have special error message for this
      error->message = N_("E15: missing closing curly brace");
      error->position = *arg;
      return FAIL;
    }
    (*arg)++;
    next_node = &((*next_node)->next);
    s = *arg;
    p = find_id_end(arg);
  }

  if (p != NULL) {
    VALUE_NODE(kTypeIdentifier, error, next_node, s, p)
    next_node = &((*next_node)->next);
  }

  return OK;
}

/*
 * Allocate a variable for a List and fill it from "*arg".
 * Return OK or FAIL.
 */
static int parse_list(char_u **arg,
                      ExpressionNode **node,
                      ExpressionParserError *error)
{
  ExpressionNode *top_node = NULL;
  ExpressionNode **next_node = node;

  TOP_NODE(kTypeList, error, node, top_node, next_node)

  *arg = skipwhite(*arg + 1);
  while (**arg != ']' && **arg != NUL) {
    if (parse1(arg, next_node, error) == FAIL)
      return FAIL;

    next_node = &((*next_node)->next);

    if (**arg == ']')
      break;
    if (**arg != ',') {
      error->message = N_("E696: Missing comma in List");
      error->position = *arg;
      return FAIL;
    }
    *arg = skipwhite(*arg + 1);
  }

  if (**arg != ']') {
    error->message = N_("E697: Missing end of List");
    error->position = *arg;
    return FAIL;
  }

  *arg = skipwhite(*arg + 1);

  return OK;
}

/*
 * Allocate a variable for a Dictionary and fill it from "*arg".
 * Return OK or FAIL.  Returns NOTDONE for {expr}.
 */
static int parse_dictionary(char_u **arg,
                            ExpressionNode **node,
                            ExpressionParserError *error,
                            ExpressionNode **parse1_node,
                            char_u **parse1_arg)
{
  ExpressionNode *top_node = NULL;
  ExpressionNode **next_node = node;
  char_u *start = skipwhite(*arg + 1);

  *parse1_node = NULL;

  // First check if it's not a curly-braces thing: {expr}.
  // But {} is an empty Dictionary.
  if (*start != '}') {
    *parse1_arg = start;
    if (parse1(parse1_arg, parse1_node, error) == FAIL) {
      free_expr(*parse1_node);
      return FAIL;
    }
    if (**parse1_arg == '}')
      return NOTDONE;
  }

  if ((top_node = expr_alloc(kTypeDictionary, error)) == NULL) {
    free_expr(*parse1_node);
    return FAIL;
  }
  next_node = &(top_node->children);
  *node = top_node;

  *arg = start;
  while (**arg != '}' && **arg != NUL) {
    if (*parse1_node != NULL) {
      *next_node = *parse1_node;
      *parse1_node = NULL;
      *arg = *parse1_arg;
    } else if (parse1(arg, next_node, error) == FAIL) {
      return FAIL;
    }

    next_node = &((*next_node)->next);

    if (**arg != ':') {
      error->message = N_("E720: Missing colon in Dictionary");
      error->position = *arg;
      return FAIL;
    }

    *arg = skipwhite(*arg + 1);
    if (parse1(arg, next_node, error) == FAIL)
      return FAIL;

    next_node = &((*next_node)->next);

    if (**arg == '}')
      break;
    if (**arg != ',') {
      error->message = N_("E722: Missing comma in Dictionary");
      error->position = *arg;
      return FAIL;
    }
    *arg = skipwhite(*arg + 1);
  }

  if (**arg != '}') {
    error->message = N_("E723: Missing end of Dictionary");
    error->position = *arg;
    return FAIL;
  }

  *arg = skipwhite(*arg + 1);

  return OK;
}

/*
 * Skip over the name of an option: "&option", "&g:option" or "&l:option".
 * "arg" points to the "&" or '+' when called.
 * Advance "arg" to the first character after the name.
 * Returns NULL when no option name found.  Otherwise pointer to the last char 
 * of the option name.
 */
static char_u *find_option_end(char_u **arg)
{
  char_u      *p = *arg;

  p++;
  if (*p == 'g' && p[1] == ':')
    p += 2;
  else if (*p == 'l' && p[1] == ':')
    p += 2;

  if (!ASCII_ISALPHA(*p))
    return NULL;

  if (p[0] == 't' && p[1] == '_' && p[2] != NUL && p[3] != NUL)
    p += 4;
  else
    while (ASCII_ISALPHA(*p))
      p++;

  *arg = p;

  return p - 1;
}

/*
 * Parse an option value.
 * "arg" points to the '&' or '+' before the option name.
 * "arg" is advanced to character after the option name.
 * Return OK or FAIL.
 */
static int parse_option(char_u **arg,
                        ExpressionNode **node,
                        ExpressionParserError *error)
{
  char_u *option_end;
  char_u *s = *arg;

  if ((option_end = find_option_end(arg)) == NULL) {
    error->message = N_("E112: Option name missing");
    error->position = *arg;
    return FAIL;
  }

  VALUE_NODE(kTypeOption, error, node, s + 1, option_end)
  return OK;
}

/*
 * Find the end of an environment variable name.
 * Advance "arg" to the first character after the name.
 * Return NULL for error.
 */
static char_u *find_env_end(char_u **arg)
{
  char_u *p;

  for (p = *arg; vim_isIDc(*p); p++) {
  }
  if (p == *arg)            /* no name found */
    return NULL;

  *arg = p;
  return p - 1;
}

/*
 * Parse an environment variable.
 * "arg" is pointing to the '$'.  It is advanced to after the name.
 * If the environment variable was not set, silently assume it is empty.
 * Always return OK.
 */
static int parse_environment_variable(char_u **arg,
                                      ExpressionNode **node,
                                      ExpressionParserError *error)
{
  char_u *s = *arg;
  char_u *e;

  (*arg)++;
  e = find_env_end(arg);
  if (e == NULL)
    e = s;

  VALUE_NODE(kTypeEnvironmentVariable, error, node, s + 1, e)

  return OK;
}

/*
 * Parse .key subscript
 * Return OK, FAIL or NOTDONE, latter only in case handle_subscript can proceed 
 * handling subsequent subscripts (i.e. .key subscript was found).
 */
static int parse_dot_subscript(char_u **arg,
                               ExpressionNode **node,
                               ExpressionParserError *error)
{
  char_u *s = *arg;
  char_u *e;
  ExpressionNode *top_node = NULL;

  // It cannot be subscript if previous character is "\"" or "'"
  if (s[-1] == '"' || s[-1] == '\'')
    return OK;
  for (e = s + 1; ASCII_ISALNUM(*e) || *e == '_'; e++) {
  }
  if (e == s + 1)
    return OK;
  // XXX Workaround for concat ambiguity: s.g:var
  if ((e - s) == 2 && *e == ':' && IS_SCOPE_CHAR(s[1]))
    return OK;
  // XXX Workaround for concat ambiguity: s:autoload#var
  if (*e == AUTOLOAD_CHAR)
    return OK;
  if ((top_node = expr_alloc(kTypeConcatOrSubscript, error)) == NULL)
    return FAIL;
  top_node->children = *node;
  top_node->position = s + 1;
  top_node->end_position = e - 1;
  *node = top_node;
  *arg = e;
  return NOTDONE;
}

/*
 * Parse function call arguments
 * Return OK or FAIL.
 */
static int parse_func_call(char_u **arg,
                           ExpressionNode **node,
                           ExpressionParserError *error)
{
  char_u *argp;
  int argcount = 0;
  ExpressionNode *top_node = NULL;
  ExpressionNode **next_node = node;

  UP_NODE(kTypeCall, error, node, top_node, next_node)

  // Get the arguments.
  argp = *arg;
  while (argcount < MAX_FUNC_ARGS) {
    argp = skipwhite(argp + 1);  // skip the '(' or ','
    if (*argp == ')' || *argp == ',' || *argp == NUL)
      break;
    if (parse1(&argp, next_node, error) == FAIL)
      return FAIL;
    next_node = &((*next_node)->next);
    argcount++;
    if (*argp != ',')
      break;
  }

  if (*argp != ')') {
    // XXX Note: vim does not have special error message for this
    error->message = N_("E15: expected closing parenthesis");
    error->position = argp;
    return FAIL;
  }

  argp++;

  *arg = skipwhite(argp);
  return OK;
}

/*
 * Parse an "[expr]" or "[expr:expr]" index.
 * "*arg" points to the '['.
 * Returns FAIL or OK. "*arg" is advanced to after the ']'.
 */
static int parse_subscript(char_u **arg,
                           ExpressionNode **node,
                           ExpressionParserError *error)
{
  ExpressionNode *top_node = NULL;
  ExpressionNode **next_node = node;

  UP_NODE(kTypeSubscript, error, node, top_node, next_node)

  // Get the (first) variable from inside the [].
  *arg = skipwhite(*arg + 1);  // skip the '['
  if (**arg == ':')
    VALUE_NODE(kTypeEmptySubscript, error, next_node, *arg, NULL)
  else if (parse1(arg, next_node, error) == FAIL)
    return FAIL;
  next_node = &((*next_node)->next);

  // Get the second variable from inside the [:].
  if (**arg == ':') {
    *arg = skipwhite(*arg + 1);
    if (**arg == ']')
      VALUE_NODE(kTypeEmptySubscript, error, next_node, *arg, NULL)
    else if (parse1(arg, next_node, error) == FAIL)
      return FAIL;
  }

  // Check for the ']'.
  if (**arg != ']') {
    error->message = N_("E111: Missing ']'");
    error->position = *arg;
    return FAIL;
  }
  *arg = skipwhite(*arg + 1);  // skip the ']'

  return OK;
}

/*
 * Handle expr[expr], expr[expr:expr] subscript and .name lookup.
 * Also handle function call with Funcref variable: func(expr)
 * Can all be combined: dict.func(expr)[idx]['func'](expr)
 */
static int handle_subscript(char_u **arg,
                            ExpressionNode **node,
                            ExpressionParserError *error)
{
  while ((**arg == '[' || **arg == '.' || **arg == '(')
         && !vim_iswhite(*(*arg - 1))) {
    switch (**arg) {
      case '.': {
        int ret = parse_dot_subscript(arg, node, error);
        if (ret == FAIL)
          return FAIL;
        if (ret != NOTDONE)
          return OK;
        break;
      }
      case '(': {
        if (parse_func_call(arg, node, error) == FAIL)
          return FAIL;
        break;
      }
      case '[': {
        if (parse_subscript(arg, node, error) == FAIL)
          return FAIL;
        break;
      }
    }
  }
  return OK;
}

void find_nr_end(char_u **arg,
                 ExpressionType *type,
                 int dooct,             // recognize octal number
                 int dohex)             // recognize hex number
{
  char_u          *ptr = *arg;
  int n;

  *type = kTypeDecimalNumber;

  if (ptr[0] == '-')
    ptr++;

  // Recognize hex and octal.
  if (ptr[0] == '0' && ptr[1] != '8' && ptr[1] != '9') {
    if (dohex && (ptr[1] == 'x' || ptr[1] == 'X') && vim_isxdigit(ptr[2])) {
      *type = kTypeHexNumber;
      ptr += 2;  // hexadecimal
    } else {
      *type = kTypeDecimalNumber;
      if (dooct) {
        // Don't interpret "0", "08" or "0129" as octal.
        for (n = 1; VIM_ISDIGIT(ptr[n]); n++) {
          if (ptr[n] > '7') {
            *type = kTypeDecimalNumber;  // can't be octal
            break;
          }
          if (ptr[n] >= '0')
            *type = kTypeOctalNumber;  // assume octal
        }
      }
    }
  }
  switch (*type) {
    case kTypeDecimalNumber: {
      while (VIM_ISDIGIT(*ptr))
        ptr++;
      break;
    }
    case kTypeOctalNumber: {
      while ('0' <= *ptr && *ptr <= '7')
        ptr++;
      break;
    }
    case kTypeHexNumber: {
      while (vim_isxdigit(*ptr))
        ptr++;
      break;
    }
    default: {
      assert(FALSE);
    }
  }

  *arg = ptr;
}

/*
 * Handle sixth level expression:
 *  number		number constant
 *  "string"		string constant
 *  'string'		literal string constant
 *  &option-name	option value
 *  @r			register contents
 *  identifier		variable value
 *  function()		function call
 *  $VAR		environment variable
 *  (expression)	nested expression
 *  [expr, expr]	List
 *  {key: val, key: val}  Dictionary
 *
 *  Also handle:
 *  ! in front		logical NOT
 *  - in front		unary minus
 *  + in front		unary plus (ignored)
 *  trailing []		subscript in String or List
 *  trailing .name	entry in Dictionary
 *
 * "arg" must point to the first non-white of the expression.
 * "arg" is advanced to the next non-white after the recognized expression.
 *
 * Return OK or FAIL.
 */
static int parse7(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error,
                  int want_string)
{
  ExpressionType type = kTypeUnknown;
  ExpressionNode *parse1_node = NULL;
  char_u *parse1_arg;
  char_u *s, *e;
  char_u *start_leader, *end_leader;
  int ret = OK;

  // Skip '!' and '-' characters.  They are handled later.
  start_leader = *arg;
  while (**arg == '!' || **arg == '-' || **arg == '+')
    *arg = skipwhite(*arg + 1);
  end_leader = *arg;

  switch (**arg) {
    // Number constant.
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
      char_u *p;

      s = *arg;
      p = skipdigits(*arg + 1);
      e = p - 1;
      type = kTypeDecimalNumber;

      /* We accept a float when the format matches
       * "[0-9]\+\.[0-9]\+\([eE][+-]\?[0-9]\+\)\?".  This is very
       * strict to avoid backwards compatibility problems.
       * Don't look for a float after the "." operator, so that
       * ":let vers = 1.2.3" doesn't fail. */
      if (!want_string && p[0] == '.' && VIM_ISDIGIT(p[1])) {
        type = kTypeFloat;
        p = skipdigits(p + 2);
        if (*p == 'e' || *p == 'E') {
          p++;
          if (*p == '-' || *p == '+')
            p++;
          if (!VIM_ISDIGIT(*p))
            type = kTypeDecimalNumber;
          else
            p = skipdigits(p + 1);
        }
        if (ASCII_ISALPHA(*p) || *p == '.')
          type = kTypeDecimalNumber;
        if (type != kTypeDecimalNumber)
          e = p - 1;
      }
      if (type == kTypeFloat) {
        *arg = e + 1;
      } else {
        find_nr_end(arg, &type, TRUE, TRUE);
        e = *arg - 1;
      }
      VALUE_NODE(type, error, node, s, e)
      break;
    }

    // String constant
    case '"':
    case '\'': {
      char_u *p;

      s = *arg;
      p = s + 1;

      if (*s == '"') {
        while (*p != '"' && *p != NUL)
        {
          if (*p == '\\' && p[1] != NUL)
            p += 2;
          else
            p++;
        }
      } else {
        while (*p != '\'' && *p != NUL)
        {
          p++;
          if (*p == '\'' && p[1] == '\'')
            p += 2;
        }
      }
      if (*p == NUL) {
        // TODO: also report which quote is missing
        error->message = N_("E114: Missing quote");
        error->position = s;
        return FAIL;
      }
      p++;

      if (*s == '"')
        type = kTypeDoubleQuotedString;
      else
        type = kTypeSingleQuotedString;

      VALUE_NODE(type, error, node, s, p - 1)
      *arg = p;
      break;
    }

    // List: [expr, expr]
    case '[': {
      ret = parse_list(arg, node, error);
      break;
    }

    // Dictionary: {key: val, key: val}
    case '{': {
      ret = parse_dictionary(arg, node, error, &parse1_node, &parse1_arg);
      break;
    }

    // Option value: &name
    case '&': {
      ret = parse_option(arg, node, error);
      break;
    }

    // Environment variable: $VAR.
    case '$': {
      ret = parse_environment_variable(arg, node, error);
      break;
    }

    // Register contents: @r.
    case '@': {
      s = *arg;
      ++*arg;
      if (**arg != NUL)
        ++*arg;
      // XXX Sigil is included: `:echo @` does the same as `:echo @"`
      // But Vim does not bother itself checking whether next character is 
      // a valid register name so you cannot just use `@` in place of `@"` 
      // everywhere: only at the end of string.
      VALUE_NODE(kTypeRegister, error, node, s, s + 1)
      break;
    }

    // nested expression: (expression).
    case '(': {
      VALUE_NODE(kTypeExpression, error, node, *arg, NULL)
      *arg = skipwhite(*arg + 1);
      ret = parse1(arg, &((*node)->children), error);
      if (**arg == ')') {
        (*arg)++;
      } else if (ret == OK) {
        error->message = N_("E110: Missing ')'");
        error->position = *arg;
        ret = FAIL;
      }
      break;
    }

    default: {
      ret = NOTDONE;
      break;
    }
  }

  if (ret == NOTDONE) {
    // Must be a variable or function name.
    // Can also be a curly-braces kind of name: {expr}.
    ret = parse_name(arg, node, error, parse1_node, parse1_arg);
  }

  *arg = skipwhite(*arg);

  /* Handle following '[', '(' and '.' for expr[expr], expr.name,
   * expr(expr). */
  if (ret == OK)
    ret = handle_subscript(arg, node, error);

  /*
   * Apply logical NOT and unary '-', from right to left, ignore '+'.
   */
  if (ret == OK && end_leader > start_leader) {
    while (end_leader > start_leader) {
      ExpressionNode *top_node = NULL;
      --end_leader;
      switch (*end_leader) {
        case '!': {
          type = kTypeNot;
          break;
        }
        case '-': {
          type = kTypeMinus;
          break;
        }
        case '+': {
          type = kTypePlus;
          break;
        }
      }
      if ((top_node = expr_alloc(type, error)) == NULL)
        return FAIL;
      top_node->children = *node;
      *node = top_node;
    }
  }

  return ret;
}

/*
 * Handle fifth level expression:
 *	*	number multiplication
 *	/	number division
 *	%	number modulo
 *
 * "arg" must point to the first non-white of the expression.
 * "arg" is advanced to the next non-white after the recognized expression.
 *
 * Return OK or FAIL.
 */
static int parse6(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error,
                  int want_string)
{
  ExpressionType type = kTypeUnknown;
  ExpressionNode *top_node = NULL;
  ExpressionNode **next_node = node;

  // Get the first variable.
  if (parse7(arg, node, error, want_string) == FAIL)
    return FAIL;

  // Repeat computing, until no '*', '/' or '%' is following.
  for (;;) {
    switch (**arg) {
      case '*': {
        type = kTypeMultiply;
        break;
      }
      case '/': {
        type = kTypeDivide;
        break;
      }
      case '%': {
        type = kTypeModulo;
        break;
      }
      default: {
        type = kTypeUnknown;
        break;
      }
    }
    if (type == kTypeUnknown)
      break;

    if (top_node == NULL || top_node->type != type)
      UP_NODE(type, error, node, top_node, next_node)
    else
      next_node = &((*next_node)->next);

    // Get the second variable.
    *arg = skipwhite(*arg + 1);
    if (parse7(arg, next_node, error, want_string) == FAIL)
      return FAIL;
  }
  return OK;
}

/*
 * Handle fourth level expression:
 *	+	number addition
 *	-	number subtraction
 *	.	string concatenation
 *
 * "arg" must point to the first non-white of the expression.
 * "arg" is advanced to the next non-white after the recognized expression.
 *
 * Return OK or FAIL.
 */
static int parse5(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error)
{
  ExpressionType type = kTypeUnknown;
  ExpressionNode *top_node = NULL;
  ExpressionNode **next_node = node;

  // Get the first variable.
  if (parse6(arg, node, error, FALSE) == FAIL)
    return FAIL;

  // Repeat computing, until no '+', '-' or '.' is following.
  for (;;) {
    switch (**arg) {
      case '+': {
        type = kTypeAdd;
        break;
      }
      case '-': {
        type = kTypeSubstract;
        break;
      }
      case '.': {
        type = kTypeStringConcat;
        break;
      }
      default: {
        type = kTypeUnknown;
        break;
      }
    }
    if (type == kTypeUnknown)
      break;

    if (top_node == NULL || top_node->type != type)
      UP_NODE(type, error, node, top_node, next_node)
    else
      next_node = &((*next_node)->next);

    // Get the second variable.
    *arg = skipwhite(*arg + 1);
    if (parse6(arg, next_node, error, type == kTypeStringConcat) == FAIL)
      return FAIL;
  }
  return OK;
}

/*
 * Handle third level expression:
 *	var1 == var2
 *	var1 =~ var2
 *	var1 != var2
 *	var1 !~ var2
 *	var1 > var2
 *	var1 >= var2
 *	var1 < var2
 *	var1 <= var2
 *	var1 is var2
 *	var1 isnot var2
 *
 * "arg" must point to the first non-white of the expression.
 * "arg" is advanced to the next non-white after the recognized expression.
 *
 * Return OK or FAIL.
 */
static int parse4(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error)
{
  char_u *p;
  ExpressionType type = kTypeUnknown;
  int len = 2;

  // Get the first variable.
  if (parse5(arg, node, error) == FAIL)
    return FAIL;

  p = *arg;
  switch (p[0]) {
    case '=': {
      if (p[1] == '=')
        type = kTypeEquals;
      else if (p[1] == '~')
        type = kTypeMatches;
      break;
    }
    case '!': {
      if (p[1] == '=')
        type = kTypeNotEquals;
      else if (p[1] == '~')
        type = kTypeNotMatches;
      break;
    }
    case '>': {
      if (p[1] != '=') {
        type = kTypeGreater;
        len = 1;
      } else {
        type = kTypeGreaterThanOrEqualTo;
      }
      break;
    }
    case '<': {
      if (p[1] != '=') {
        type = kTypeLess;
        len = 1;
      } else {
        type = kTypeLessThanOrEqualTo;
      }
      break;
    }
    case 'i': {
      if (p[1] == 's') {
        if (p[2] == 'n' && p[3] == 'o' && p[4] == 't')
          len = 5;
        if (!vim_isIDc(p[len]))
          type = len == 2 ? kTypeIdentical : kTypeNotIdentical;
      }
      break;
    }
  }

  // If there is a comparative operator, use it.
  if (type != kTypeUnknown) {
    ExpressionNode *top_node = NULL;
    ExpressionNode **next_node = node;

    UP_NODE(type, error, node, top_node, next_node)

    // extra question mark appended: ignore case
    if (p[len] == '?') {
      top_node->ignore_case = kCCStrategyIgnoreCase;
      len++;
    // extra '#' appended: match case
    } else if (p[len] == '#') {
      top_node->ignore_case = kCCStrategyMatchCase;
      len++;
    }
    // nothing appended: use kCCStrategyUseOption (default)

    // Get the second variable.
    *arg = skipwhite(p + len);
    if (parse5(arg, next_node, error) == FAIL)
      return FAIL;
  }

  return OK;
}

/*
 * Handle second and third level expressions:
 *	expr3 && expr3 && expr3	    logical AND (level==3)
 *	expr2 || expr2 || expr2	    logical OR  (level==2)
 *
 * "arg" must point to the first non-white of the expression.
 * "arg" is advanced to the next non-white after the recognized expression.
 *
 * Return OK or FAIL.
 */
static int parse23(char_u **arg,
                   ExpressionNode **node,
                   ExpressionParserError *error,
                   int level)
{
  ExpressionNode *top_node = NULL;
  ExpressionNode **next_node = node;
  ExpressionType type;
  char_u c;
  int (*parse_next)(char_u **, ExpressionNode **, ExpressionParserError *);

  if (level == 2) {
    type = kTypeLogicalOr;
    parse_next = &parse3;
    c = '|';
  } else {
    type = kTypeLogicalAnd;
    parse_next = &parse4;
    c = '&';
  }

  // Get the first variable.
  if (parse_next(arg, node, error) == FAIL)
    return FAIL;

  // Repeat until there is no following "&&".
  while ((*arg)[0] == c && (*arg)[1] == c) {
    if (top_node == NULL)
      UP_NODE(type, error, node, top_node, next_node)

    // Get the second variable.
    *arg = skipwhite(*arg + 2);
    if (parse_next(arg, next_node, error) == FAIL)
      return FAIL;
    next_node = &((*next_node)->next);
  }

  return OK;
}

static int parse3(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error)
{
  return parse23(arg, node, error, 3);
}

static int parse2(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error)
{
  return parse23(arg, node, error, 2);
}

/*
 * Handle top level expression:
 *	expr2 ? expr1 : expr1
 *
 * "arg" must point to the first non-white of the expression.
 * "arg" is advanced to the next non-white after the recognized expression.
 *
 * Note: "rettv.v_lock" is not set.
 *
 * Return OK or FAIL.
 */
static int parse1(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error)
{
  // Get the first variable.
  if (parse2(arg, node, error) == FAIL)
    return FAIL;

  if ((*arg)[0] == '?') {
    ExpressionNode *top_node;
    ExpressionNode **next_node = node;

    UP_NODE(kTypeTernaryConditional, error, node, top_node, next_node)

    // Get the second variable.
    *arg = skipwhite(*arg + 1);
    if (parse1(arg, next_node, error) == FAIL)
      return FAIL;

    // Check for the ":".
    if (**arg != ':') {
      error->message = N_("E109: Missing ':' after '?'");
      error->position = *arg;
      return FAIL;
    }

    next_node = &((*next_node)->next);

    // Get the third variable.
    *arg = skipwhite(*arg + 1);
    if (parse1(arg, next_node, error) == FAIL)
      return FAIL;
  }

  return OK;
}

ExpressionNode *parse0_err(char_u **arg, ExpressionParserError *error)
{
  ExpressionNode *result = NULL;

  error->message = NULL;
  error->position = NULL;

  *arg = skipwhite(*arg);
  if (parse1(arg, &result, error) == FAIL) {
    free_expr(result);
    return NULL;
  }

  return result;
}

void free_test_expr_result(TestExprResult *result)
{
  free_expr(result->node);
  vim_free(result);
}

TestExprResult *parse0_test(char_u *arg)
{
  TestExprResult *result;

  if ((result = ALLOC_CLEAR_NEW(TestExprResult, 1)) == NULL)
    return NULL;

  result->end = arg;
  result->node = parse0_err(&(result->end), &(result->error));

  return result;
}

static char *expression_type_string[] = {
  "Unknown",
  "?:",
  "||",
  "&&",
  ">",
  ">=",
  "<",
  "<=",
  "==",
  "!=",
  "is",
  "isnot",
  "=~",
  "!~",
  "+",
  "-",
  "..",
  "*",
  "/",
  "%",
  "!",
  "-!",
  "+!",
  "N",
  "O",
  "X",
  "F",
  "\"",
  "'",
  "&",
  "@",
  "$",
  "cvar",
  "var",
  "id",
  "curly",
  "expr",
  "[]",
  "{}",
  "index",
  ".",
  "call",
  "empty",
};

static char *case_compare_strategy_string[] = {
  "",
  "#",
  "?",
};

static size_t node_repr_len(ExpressionNode *node)
{
  size_t len = 0;

  len += STRLEN(expression_type_string[node->type]);
  len += STRLEN(case_compare_strategy_string[node->ignore_case]);

  if (node->position != NULL) {
    // 4 for [++] or [!!]
    len += 4;
    if (node->end_position != NULL)
      len += node->end_position - node->position + 1;
    else
      len++;
  }

  if (node->children != NULL)
    // 2 for parenthesis
    len += 2 + node_repr_len(node->children);

  if (node->next != NULL)
    // 2 for ", "
    len += 2 + node_repr_len(node->next);

  return len;
}

static void node_repr(ExpressionNode *node, char **pp)
{
  char *p = *pp;

  STRCPY(p, expression_type_string[node->type]);
  p += STRLEN(expression_type_string[node->type]);
  STRCPY(p, case_compare_strategy_string[node->ignore_case]);
  p += STRLEN(case_compare_strategy_string[node->ignore_case]);

  if (node->position != NULL) {
    *p++ = '[';
    if (node->end_position != NULL) {
      size_t len = node->end_position - node->position + 1;

      *p++ = '+';

      if (node->type == kTypeRegister && *(node->end_position) == NUL)
        len--;

      memcpy((void *) p, node->position, len);
      p += len;

      *p++ = '+';
    } else {
      *p++ = '!';
      *p++ = *(node->position);
      *p++ = '!';
    }
    *p++ = ']';
  }

  if (node->children != NULL) {
    *p++ = '(';
    node_repr(node->children, &p);
    *p++ = ')';
  }

  if (node->next != NULL) {
    *p++ = ',';
    *p++ = ' ';
    node_repr(node->next, &p);
  }

  *pp = p;
}

char *parse0_dump(char_u *arg)
{
  TestExprResult *p0_result;
  size_t len = 0;
  size_t shift = 0;
  size_t offset = 0;
  size_t i;
  char *result = NULL;
  char *p;

  if ((p0_result = parse0_test(arg)) == NULL)
    goto theend;

  if (p0_result->error.message != NULL)
    len = 6 + STRLEN(p0_result->error.message);
  else
    len = node_repr_len(p0_result->node);

  offset = p0_result->end - arg;
  i = offset;
  do {
    shift++;
    i = i >> 4;
  } while (i);

  len += shift + 1;

  if (!(result = ALLOC_CLEAR_NEW(char, len + 1)))
    goto theend;

  p = result;

  i = shift;
  do {
    size_t digit = (offset >> ((i - 1) * 4)) & 0xF;
    *p++ = (digit < 0xA ? ('0' + digit) : ('A' + (digit - 0xA)));
  } while(--i);

  *p++ = ':';

  if (p0_result->error.message != NULL) {
    memcpy(p, "error:", 6);
    p += 6;
    STRCPY(p, p0_result->error.message);
  } else {
    node_repr(p0_result->node, &p);
  }

theend:
  free_test_expr_result(p0_result);
  return result;
}
