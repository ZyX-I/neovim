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
    if ((top_node = node_alloc(type, error)) == NULL) \
      return FAIL; \
    next_node = &((*old_top_node)->next); \
    top_node->children = *old_top_node; \
    *old_top_node = top_node; \
  }

#define VALUE_NODE(type, error, node, pos, end_pos) \
  { \
    if ((*node = node_alloc(type, error)) == NULL) \
      return FAIL; \
    (*node)->position = pos; \
    (*node)->end_position = end_pos; \
  }

#define IS_SCOPE_CHAR(c) (   (c) == 'g' || (c) == 'b' || (c) == 'w' \
                          || (c) == 't' || (c) == 'v' || (c) == 'a' \
                          || (c) == 'l' || (c) == 's')

static int parse1(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error);

static ExpressionNode *node_alloc(ExpressionType type,
                                  ExpressionParserError *error)
{
  ExpressionNode *node;

  if ((node = ALLOC_CLEAR_NEW(ExpressionNode, 1)) == NULL)
    return NULL;

  node->type = type;
  return node;
}

void free_node(ExpressionNode *node)
{
  if (node == NULL)
    return;

  free_node(node->children);
  free_node(node->next);
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
  for (p = *arg; isnamechar(*p); ++p) {
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

  if ((top_node = node_alloc(kTypeVariableName, error)) == NULL)
    return FAIL;
  next_node = &(top_node->children);
  *node = top_node;

  if (parse1_node == NULL) {
    s = *arg;
    if (   (*arg)[0] == K_SPECIAL
        && (*arg)[1] == KS_EXTRA
        && (*arg)[2] == (int)KE_SNR) {
      // hard coded <SNR>, already translated
      *arg += 3;
      if ((p = find_id_end(arg)) == NULL) {
        // FIXME Proper error message
        error->message = "expected variable name";
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
        // FIXME Proper error message
        error->message = "expected variable name";
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
    ++(*arg);
    *arg = skipwhite(*arg);

    VALUE_NODE(kTypeCurlyName, error, next_node, s, NULL)

    if ((parse1(arg, &((*next_node)->children), error)) == FAIL)
      return FAIL;

    if (**arg != '}') {
      // FIXME Proper error message
      error->message = "missing closing curly brace";
      error->position = *arg;
      return FAIL;
    }
    ++(*arg);
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

  UP_NODE(kTypeList, error, node, top_node, next_node)

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
      free_node(*parse1_node);
      return FAIL;
    }
    if (**parse1_arg == '}')
      return NOTDONE;
  }

  if ((top_node = node_alloc(kTypeVariableName, error)) == NULL) {
    free_node(*parse1_node);
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

  ++p;
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
      ++p;

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

  for (p = *arg; vim_isIDc(*p); ++p) {
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

  ++(*arg);
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
  char_u *s;
  char_u *e;
  ExpressionNode *top_node = NULL;

  s = *arg;
  for (e = s + 1; ASCII_ISALNUM(*e) || *e == '_'; ++e) {
  }
  if (e == s + 1)
    return OK;
  // XXX Workaround for concat ambiguity
  if ((e - s) == 2 && *e == ':' && IS_SCOPE_CHAR(s[1]))
    return OK;
  if ((top_node = node_alloc(kTypeConcatOrSubscript, error)) == NULL)
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
    ++argcount;
    if (*argp != ',')
      break;
  }

  if (*argp != ')') {
    // FIXME Proper error message
    error->message = "expected closing parenthesis";
    error->position = argp;
    return FAIL;
  }

  ++argp;

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
      type = kTypeNumber;

      /* We accept a float when the format matches
       * "[0-9]\+\.[0-9]\+\([eE][+-]\?[0-9]\+\)\?".  This is very
       * strict to avoid backwards compatibility problems.
       * Don't look for a float after the "." operator, so that
       * ":let vers = 1.2.3" doesn't fail. */
      if (!want_string && p[0] == '.' && VIM_ISDIGIT(p[1])) {
        type = kTypeFloat;
        p = skipdigits(p + 2);
        if (*p == 'e' || *p == 'E') {
          ++p;
          if (*p == '-' || *p == '+')
            ++p;
          if (!VIM_ISDIGIT(*p))
            type = kTypeNumber;
          else
            p = skipdigits(p + 1);
        }
        if (ASCII_ISALPHA(*p) || *p == '.')
          type = kTypeNumber;
        if (type != kTypeNumber)
          e = p - 1;
      }
      VALUE_NODE(type, error, node, s, e)
      *arg = e + 1;
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
            ++p;
        }
      } else {
        while (*p != '\'' && *p != NUL)
        {
          ++p;
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
      ++p;

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
        ++(*arg);
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
      if ((top_node = node_alloc(type, error)) == NULL)
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
      ++len;
    // extra '#' appended: match case
    } else if (p[len] == '#') {
      top_node->ignore_case = kCCStrategyMatchCase;
      ++len;
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
 * Handle second level expression:
 *	expr3 && expr3 && expr3	    logical AND
 *
 * "arg" must point to the first non-white of the expression.
 * "arg" is advanced to the next non-white after the recognized expression.
 *
 * Return OK or FAIL.
 */
static int parse3(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error)
{
  ExpressionNode *top_node = NULL;
  ExpressionNode **next_node = node;

  // Get the first variable.
  if (parse4(arg, node, error) == FAIL)
    return FAIL;

  // Repeat until there is no following "&&".
  while ((*arg)[0] == '&' && (*arg)[1] == '&') {
    if (top_node == NULL)
      UP_NODE(kTypeLogicalOr, error, node, top_node, next_node)

    // Get the second variable.
    *arg = skipwhite(*arg + 2);
    if (parse4(arg, next_node, error) == FAIL)
      return FAIL;
    next_node = &((*next_node)->next);
  }

  return OK;
}

/*
 * Handle first level expression:
 *	expr2 || expr2 || expr2	    logical OR
 *
 * "arg" must point to the first non-white of the expression.
 * "arg" is advanced to the next non-white after the recognized expression.
 *
 * Return OK or FAIL.
 */
static int parse2(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error)
{
  ExpressionNode *top_node = NULL;
  ExpressionNode **next_node = node;

  // Get the first variable.
  if (parse3(arg, node, error) == FAIL)
    return FAIL;

  // Repeat until there is no following "||".
  while ((*arg)[0] == '|' && (*arg)[1] == '|') {
    if (top_node == NULL)
      UP_NODE(kTypeLogicalAnd, error, node, top_node, next_node)

    // Get the second variable.
    *arg = skipwhite(*arg + 2);
    if (parse3(arg, next_node, error) == FAIL)
      return FAIL;
    next_node = &((*next_node)->next);
  }

  return OK;
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

ExpressionNode *parse0_err(char_u *arg, ExpressionParserError *error)
{
  ExpressionNode *result = NULL;
  char_u *p = arg;

  error->message = NULL;
  error->position = NULL;

  p = skipwhite(arg);
  if (parse1(&p, &result, error) == FAIL) {
    free_node(result);
    return NULL;
  }

  // Set error->position to p for testing
  error->position = p;
  return result;
}

ExpressionNode *parse0(char_u *arg)
{
  ExpressionNode *result;
  ExpressionParserError error;

  if ((result = parse0_err(arg, &error)) == NULL) {
    // FIXME print error properly
    puts((char *) error.message);
    puts((char *) error.position);
    return NULL;
  }

  return result;
}
