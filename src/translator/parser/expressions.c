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
#include "charset.h"

#include "translator/parser/expressions.h"

// maximum number of function arguments
#define MAX_FUNC_ARGS   20

#define UP_NODE(type, error, old_top_node, top_node, next_node) \
  { \
    if ((top_node = expr_alloc(type)) == NULL) \
      return FAIL; \
    next_node = &((*old_top_node)->next); \
    top_node->children = *old_top_node; \
    *old_top_node = top_node; \
  }

#define TOP_NODE(type, error, old_top_node, top_node, next_node) \
  { \
    if ((top_node = expr_alloc(type)) == NULL) \
      return FAIL; \
    next_node = &(top_node->children); \
    *old_top_node = top_node; \
  }

#define VALUE_NODE(type, error, node, pos, end_pos) \
  { \
    if ((*node = expr_alloc(type)) == NULL) \
      return FAIL; \
    (*node)->position = pos; \
    (*node)->end_position = end_pos; \
  }

#define IS_SCOPE_CHAR(c)    ((c) == 'g' || (c) == 'b' || (c) == 'w' \
                          || (c) == 't' || (c) == 'v' || (c) == 'a' \
                          || (c) == 'l' || (c) == 's')

// {{{ Function declarations
static ExpressionNode *expr_alloc(ExpressionType type);
static bool isnamechar(int c);
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
                            ExpressionParserError *error,
                            bool parse_funccall);
static void find_nr_end(char_u **arg, ExpressionType *type,
                        bool dooct, bool dohex);
static int parse7(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error,
                  bool want_string,
                  bool parse_funccall);
static int parse6(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error,
                  bool want_string);
static int parse5(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error);
static int parse4(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error);
static int parse23(char_u **arg,
                   ExpressionNode **node,
                   ExpressionParserError *error,
                   uint8_t level);
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
// }}}

/// Allocate new expression node and assign its type property
///
/// @param[in]  type   Node type.
///
/// @return Pointer to allocated block of memory or NULL in case of error.
static ExpressionNode *expr_alloc(ExpressionType type)
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

/// Check whether given character is a valid name character
///
/// @param[in]  c  Tested character.
///
/// @return TRUE if character can be used in a variable of function name, FALSE 
///         otherwise. Does not include '{' or '}' for magic braces.
static bool isnamechar(int c)
{
  return ASCII_ISALNUM(c) || c == '_' || c == ':' || c == AUTOLOAD_CHAR;
}

/// Find the end of the name of a function or internal variable
///
/// @param[in,out]  arg  Searched argument. It is advanced to the first 
///                      non-white character after the name.
///
/// @return Last character of the name if name was found (i.e. *arg - 1). NULL 
///         if name was not found
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

/// Get length of s:/<SID>/<SNR> function name prefix
///
/// @param[in]  p  Searched string.
///
/// @return 5 if "p" starts with "<SID>" or "<SNR>" (ignoring case).
///         2 if "p" starts with "s:".
///         0 otherwise.
static int get_fname_script_len(char_u *p)
{
  if (p[0] == '<' && (   STRNICMP(p + 1, "SID>", 4) == 0
                      || STRNICMP(p + 1, "SNR>", 4) == 0))
    return 5;
  if (p[0] == 's' && p[1] == ':')
    return 2;
  return 0;
}

/// Parse variable/function name
///
/// @param[in,out]  arg          Parsed string. Is advanced to the first 
///                              character after the name.
/// @param[out]     node         Location where results are saved.
/// @param[out]     error        Structure where errors are saved.
/// @param[in]      parse1_node  Cached results of parsing first expression in 
///                              curly-braces-name ({expr}). Only expected if 
///                              '{' is the first symbol (i.e. *arg == '{'). 
///                              Must be NULL if it is not the first symbol.
/// @param[in]      parse1_arg   Cached end of curly braces expression. Only 
///                              expected under the same conditions with 
///                              parse1_node.
///
/// @return FAIL if parsing failed, OK otherwise.
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

/// Parse list literal
///
/// @param[in,out]  arg    Parsed string. Is advanced to the first character 
///                        after the list. Must point to the opening bracket.
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
///
/// @return FAIL if parsing failed, OK otherwise.
static int parse_list(char_u **arg,
                      ExpressionNode **node,
                      ExpressionParserError *error)
{
  ExpressionNode *top_node = NULL;
  ExpressionNode **next_node = node;

  TOP_NODE(kTypeList, error, node, top_node, next_node)

  top_node->position = *arg;

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

/// Parse dictionary literal
///
/// @param[in,out]  arg          Parsed string. Is advanced to the first 
///                              character after the dictionary. Must point to 
///                              the opening curly brace.
/// @param[out]     node         Location where parsing results are saved.
/// @param[out]     error        Structure where errors are saved.
/// @param[out]     parse1_node  Location where parsing results are saved if 
///                              expression proved to be curly braces name part.
/// @param[out]     parse1_arg   Location where end of curly braces name 
///                              expression is saved.
///
/// @return FAIL if parsing failed, NOTDONE if curly braces name found, OK 
///         otherwise.
static int parse_dictionary(char_u **arg,
                            ExpressionNode **node,
                            ExpressionParserError *error,
                            ExpressionNode **parse1_node,
                            char_u **parse1_arg)
{
  ExpressionNode *top_node = NULL;
  ExpressionNode **next_node = node;
  char_u *s = *arg;
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

  if ((top_node = expr_alloc(kTypeDictionary)) == NULL) {
    free_expr(*parse1_node);
    return FAIL;
  }
  next_node = &(top_node->children);
  *node = top_node;

  top_node->position = s;

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

/// Skip over the name of an option ("&option", "&g:option", "&l:option")
///
/// @param[in,out]  arg  Start of the option name. It must point to option sigil 
///                      (i.e. '&' or '+'). Advanced to the first character 
///                      after the option name.
///
/// @return NULL if no option name found, pointer to the last character of the 
///         option name otherwise (i.e. *arg - 1).
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

/// Parse an option literal
///
/// @param[in,out]  arg    Parsed string. It should point to "&" before the 
///                        option name. Advanced to the first character after 
///                        the option name.
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
///
/// @return FAIL if parsing failed, OK otherwise.
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

/// Skip over the name of an environment variable
///
/// @param[in,out]  arg  Start of the variable name. Advanced to the first 
///                      character after the variable name.
///
/// @return NULL if no variable name found, pointer to the last character of the 
///         variable name otherwise (i.e. *arg - 1).
///
/// @note Uses vim_isIDc() function: depends on &isident option.
static char_u *find_env_end(char_u **arg)
{
  char_u *p;

  for (p = *arg; vim_isIDc(*p); p++) {
  }
  if (p == *arg)            // no name found
    return NULL;

  *arg = p;
  return p - 1;
}

/// Parse an environment variable literal
///
/// @param[in,out]  arg    Parsed string. Is expected to point to the sigil 
///                        ('$'). Is advanced after the variable name.
/// @parblock
///   @note If there is no variable name after '$' it is simply assumed that 
///         this name is empty.
/// @endparblock
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
///
/// @return FAIL when out of memory, OK otherwise.
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

/// Parse .key subscript
///
/// @param[in,out]  arg    Parsed string. Is advanced to the first character 
///                        after the subscript.
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
///
/// @return FAIL when out of memory, NOTDONE if subscript was found, OK 
///         otherwise.
static int parse_dot_subscript(char_u **arg,
                               ExpressionNode **node,
                               ExpressionParserError *error)
{
  char_u *s = *arg;
  char_u *e;
  ExpressionNode *top_node = NULL;

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
  if ((top_node = expr_alloc(kTypeConcatOrSubscript)) == NULL)
    return FAIL;
  top_node->children = *node;
  top_node->position = s + 1;
  top_node->end_position = e - 1;
  *node = top_node;
  *arg = e;
  return NOTDONE;
}

/// Parse function call arguments
///
/// @param[in,out]  arg    Parsed string. Is advanced to the first character 
///                        after the closing parenthesis of given function call. 
///                        Should point to the opening parenthesis.
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
///
/// @return FAIL if parsing failed, OK otherwise.
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
    error->message = N_("E116: expected closing parenthesis");
    error->position = argp;
    return FAIL;
  }

  argp++;

  *arg = skipwhite(argp);
  return OK;
}

/// Parse "[expr]" or "[expr : expr]" subscript
///
/// @param[in,out]  arg    Parsed string. Is advanced to the first character 
///                        after the subscript. Is expected to point to the 
///                        opening bracket.
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
///
/// @return FAIL if parsing failed, OK otherwise.
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

/// Parse all following "[...]" subscripts, .key subscripts and function calls
///
/// @param[in,out]  arg    Parsed string. Is advanced to the first character 
///                        after the last subscript.
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
///
/// @return FAIL if parsing failed, OK otherwise.
static int handle_subscript(char_u **arg,
                            ExpressionNode **node,
                            ExpressionParserError *error,
                            bool parse_funccall)
{
  while ((**arg == '[' || **arg == '.'
          || (parse_funccall && **arg == '('))
         && !vim_iswhite(*(*arg - 1))) {
    switch (**arg) {
      case '.': {
        int ret;
        if ((*node)->type == kTypeDecimalNumber
            || (*node)->type == kTypeOctalNumber
            || (*node)->type == kTypeHexNumber
            || (*node)->type == kTypeSingleQuotedString
            || (*node)->type == kTypeDoubleQuotedString)
          return OK;
        ret = parse_dot_subscript(arg, node, error);
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

/// Find end of VimL number (100, 0xA0, 0775, possibly with minus sign)
///
/// @param[in,out]  arg    Parsed string.
/// @param[out]     type   Type of the resulting number.
/// @param[in]      dooct  If true allow function to recognize octal numbers.
/// @param[in]      dohex  If true allow function to recognize hexadecimal 
///                        numbers.
static void find_nr_end(char_u **arg, ExpressionType *type,
                        bool dooct, bool dohex)
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

/// Parse seventh level expression: values
///
/// Parsed values:
///
/// Value                | Description
/// -------------------- | -----------------------
/// number               | number constant
/// "string"             | string constant
/// 'string'             | literal string constant
/// &option-name         | option value
/// @r                   | register contents
/// identifier           | variable value
/// function()           | function call
/// $VAR                 | environment variable
/// (expression)         | nested expression
/// [expr, expr]         | List
/// {key: val, key: val} | Dictionary
///
/// Also handles unary operators (logical NOT, unary minus, unary plus) and 
/// subscripts ([], .key, func()).
///
/// @param[in,out]  arg             Parsed string. Must point to the first 
///                                 non-white character. Advanced to the next 
///                                 non-white after the recognized expression.
/// @param[out]     node            Location where parsing results are saved.
/// @param[out]     error           Structure where errors are saved.
/// @param[in]      want_string     True if the result should be string. Is used 
///                                 to preserve compatibility with vim: "a".1.2 
///                                 is a string "a12" (uses string concat), not 
///                                 floating-point value. This flag is set in 
///                                 parse5 that handles concats.
/// @param[in]      parse_funccall  Determines whether function calls should be 
///                                 parsed. I.e. if this is TRUE then "abc(def)" 
///                                 will be parsed as "call(abc, def)", if this 
///                                 is FALSE it will parse this as "abc" and 
///                                 stop at opening parenthesis.
///
/// @return FAIL if parsing failed, OK otherwise.
static int parse7(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error,
                  bool want_string,
                  bool parse_funccall)
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

      // We accept a float when the format matches
      // "[0-9]\+\.[0-9]\+\([eE][+-]\?[0-9]\+\)\?".  This is very
      // strict to avoid backwards compatibility problems.
      // Don't look for a float after the "." operator, so that
      // ":let vers = 1.2.3" doesn't fail.
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

    *arg = skipwhite(*arg);

    if (**arg == '(' && parse_funccall)
      // Function call. First function call is not handled by handle_subscript 
      // for whatever reasons. Allows expressions like "tr   (1, 2, 3)"
      ret = parse_func_call(arg, node, error);
  }

  *arg = skipwhite(*arg);

  // Handle following '[', '(' and '.' for expr[expr], expr.name,
  // expr(expr).
  if (ret == OK)
    ret = handle_subscript(arg, node, error, parse_funccall);

  // Apply logical NOT and unary '-', from right to left, ignore '+'.
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
      if ((top_node = expr_alloc(type)) == NULL)
        return FAIL;
      top_node->children = *node;
      *node = top_node;
    }
  }

  return ret;
}

/// Parse value (actually used for lvals)
///
/// @param[in,out]  arg    Parsed string. May point to whitespace character. Is 
///                        advanced to the next non-white after the recognized 
///                        expression.
/// @param[out]     error  Structure where errors are saved.
/// @param[in]      multi  Determines whether it should parse a sequence of 
///                        expressions (e.g. for ":echo").
///
/// @return NULL if parsing failed or memory was exhausted, pointer to the 
///         allocated expression node otherwise.
ExpressionNode *parse7_nofunc(char_u **arg, ExpressionParserError *error,
                              bool multi)
{
  ExpressionNode *result = NULL;

  error->message = NULL;
  error->position = NULL;

  *arg = skipwhite(*arg);
  if (parse7(arg, &result, error, FALSE, FALSE) == FAIL) {
    free_expr(result);
    return NULL;
  }

  if (multi) {
    ExpressionNode **next = &(result->next);
    while (**arg && **arg != '\n' && **arg != '|') {
      *arg = skipwhite(*arg);
      if (parse7(arg, next, error, FALSE, FALSE) == FAIL) {
        free_expr(result);
        return NULL;
      }
      next = &((*next)->next);
    }
  }

  return result;
}

/// Handle sixths level expression: multiplication/division/modulo
///
/// Operators supported:
///
/// Operator | Operation
/// -------- | ---------------------
///   "*"    | Number multiplication
///   "/"    | Number division
///   "%"    | Number modulo
///
/// @param[in,out]  arg    Parsed string. Must point to the first non-white 
///                        character. Advanced to the next non-white after the 
///                        recognized expression.
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
///
/// @return FAIL if parsing failed, OK otherwise.
static int parse6(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error,
                  bool want_string)
{
  ExpressionType type = kTypeUnknown;
  ExpressionNode *top_node = NULL;
  ExpressionNode **next_node = node;

  // Get the first variable.
  if (parse7(arg, node, error, want_string, TRUE) == FAIL)
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
    if (parse7(arg, next_node, error, want_string, TRUE) == FAIL)
      return FAIL;
  }
  return OK;
}

/// Handle fifth level expression: addition/subtraction/concatenation
///
/// Operators supported:
///
/// Operator | Operation
/// -------- | --------------------
///   "+"    | List/number addition
///   "/"    | Number subtraction
///   "%"    | String concatenation
///
/// @param[in,out]  arg    Parsed string. Must point to the first non-white 
///                        character. Advanced to the next non-white after the 
///                        recognized expression.
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
///
/// @return FAIL if parsing failed, OK otherwise.
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
        type = kTypeSubtract;
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

/// Handle fourth level expression: boolean relations
///
/// Relation types:
///
/// Operator | Relation
/// -------- | ---------------------
///   "=="   | Equals
///   "=~"   | Matches pattern
///   "!="   | Not equals
///   "!~"   | Not matches pattern
///   ">"    | Is greater than
///   ">="   | Is greater than or equal to
///   "<"    | Is less than
///   "<="   | Is less than or equal to
///   "is"   | Is identical to
///  "isnot" | Is not identical to
///
/// Accepts "#" or "?" after each operator to designate case compare strategy 
/// (by default it is taken from &ignore case option, trailing "#" means that 
/// case is respected, trailing "?" means that it is ignored).
///
/// @param[in,out]  arg    Parsed string. Must point to the first non-white 
///                        character. Advanced to the next non-white after the 
///                        recognized expression.
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
///
/// @return FAIL if parsing failed, OK otherwise.
static int parse4(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error)
{
  char_u *p;
  ExpressionType type = kTypeUnknown;
  size_t len = 2;

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

/// Handle second and third level expression: logical operations
///
/// Operators used:
///
/// Operator | Operation
/// -------- | ----------------------
///   "&&"   | Logical AND (level==3)
///   "||"   | Logical OR  (level==2)
///
/// @param[in,out]  arg    Parsed string. Must point to the first non-white 
///                        character. Advanced to the next non-white after the 
///                        recognized expression.
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
/// @param[in]      level  Expression level: determines which logical operator 
///                        should be handled.
///
/// @return FAIL if parsing failed, OK otherwise.
static int parse23(char_u **arg,
                   ExpressionNode **node,
                   ExpressionParserError *error,
                   uint8_t level)
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

/// Handle third level expression: logical AND
///
/// @param[in,out]  arg    Parsed string. Must point to the first non-white 
///                        character. Advanced to the next non-white after the 
///                        recognized expression.
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
///
/// @return FAIL if parsing failed, OK otherwise.
static int parse3(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error)
{
  return parse23(arg, node, error, 3);
}

/// Handle second level expression: logical OR
///
/// @param[in,out]  arg    Parsed string. Must point to the first non-white 
///                        character. Advanced to the next non-white after the 
///                        recognized expression.
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
///
/// @return FAIL if parsing failed, OK otherwise.
static int parse2(char_u **arg,
                  ExpressionNode **node,
                  ExpressionParserError *error)
{
  return parse23(arg, node, error, 2);
}

/// Handle first (top) level expression: ternary conditional operator
///
/// Handles expr2 ? expr1 : expr1
///
/// @param[in,out]  arg    Parsed string. Must point to the first non-white 
///                        character. Advanced to the next non-white after the 
///                        recognized expression.
/// @param[out]     node   Location where parsing results are saved.
/// @param[out]     error  Structure where errors are saved.
///
/// @return FAIL if parsing failed, OK otherwise.
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

/// Parse expression
///
/// @param[in,out]  arg    Parsed string. May point to whitespace character. Is 
///                        advanced to the next non-white after the recognized 
///                        expression.
/// @param[out]     error  Structure where errors are saved.
/// @param[in]      multi  Determines whether it should parse a sequence of 
///                        expressions (e.g. for ":echo").
///
/// @return NULL if parsing failed or memory was exhausted, pointer to the 
///         allocated expression node otherwise.
ExpressionNode *parse0_err(char_u **arg, ExpressionParserError *error,
                           bool multi)
{
  ExpressionNode *result = NULL;

  error->message = NULL;
  error->position = NULL;

  *arg = skipwhite(*arg);
  if (parse1(arg, &result, error) == FAIL) {
    free_expr(result);
    return NULL;
  }

  if (multi) {
    ExpressionNode **next = &(result->next);
    while (**arg && **arg != '\n' && **arg != '|') {
      *arg = skipwhite(*arg);
      if (parse1(arg, next, error) == FAIL) {
        free_expr(result);
        return NULL;
      }
      next = &((*next)->next);
    }
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
  result->node = parse0_err(&(result->end), &(result->error), FALSE);

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

static size_t node_dump_len(ExpressionNode *node)
{
  size_t len = 0;

  switch (node->type) {
    case kTypeGreater:
    case kTypeGreaterThanOrEqualTo:
    case kTypeLess:
    case kTypeLessThanOrEqualTo:
    case kTypeEquals:
    case kTypeNotEquals:
    case kTypeIdentical:
    case kTypeNotIdentical:
    case kTypeMatches:
    case kTypeNotMatches: {
      len += STRLEN(case_compare_strategy_string[node->ignore_case]);
      assert(node->children != NULL);
      assert(node->children->next != NULL);
      assert(node->children->next->next == NULL);
      // fallthrough
    }
    case kTypeLogicalOr:
    case kTypeLogicalAnd:
    case kTypeAdd:
    case kTypeSubtract:
    case kTypeMultiply:
    case kTypeDivide:
    case kTypeModulo:
    case kTypeStringConcat: {
      ExpressionNode *child = node->children;
      size_t operator_len;

      if (node->type == kTypeStringConcat)
        operator_len = 1 + 2;
      else
        operator_len = STRLEN(expression_type_string[node->type]) + 2;

      assert(node->children != NULL);
      assert(node->children->next != NULL);
      child = node->children;
      do {
        len += node_dump_len(child);
        child = child->next;
        if (child != NULL)
          len += operator_len;
      } while (child != NULL);

      break;
    }
    case kTypeTernaryConditional: {
      assert(node->children != NULL);
      assert(node->children->next != NULL);
      assert(node->children->next->next != NULL);
      assert(node->children->next->next->next == NULL);
      len += node_dump_len(node->children);
      len += 3;
      len += node_dump_len(node->children->next);
      len += 3;
      len += node_dump_len(node->children->next->next);
      break;
    }
    case kTypeNot:
    case kTypeMinus:
    case kTypePlus: {
      assert(node->children != NULL);
      assert(node->children->next == NULL);
      len += 1;
      len += node_dump_len(node->children);
      break;
    }
    case kTypeDecimalNumber:
    case kTypeOctalNumber:
    case kTypeHexNumber:
    case kTypeFloat:
    case kTypeDoubleQuotedString:
    case kTypeSingleQuotedString:
    case kTypeOption:
    case kTypeRegister:
    case kTypeEnvironmentVariable:
    case kTypeSimpleVariableName:
    case kTypeIdentifier: {
      assert(node->position != NULL);
      assert(node->end_position != NULL);
      assert(node->children == NULL);
      len += node->end_position - node->position + 1;
      break;
    }
    case kTypeVariableName: {
      ExpressionNode *child = node->children;
      assert(child != NULL);
      do {
        len += node_dump_len(child);
        child = child->next;
      } while (child != NULL);
      break;
    }
    case kTypeCurlyName:
    case kTypeExpression: {
      assert(node->children != NULL);
      assert(node->children->next == NULL);
      len += 1;
      len += node_dump_len(node->children);
      len += 1;
      break;
    }
    case kTypeList: {
      ExpressionNode *child = node->children;
      len += 1;
      while (child != NULL) {
        len += node_dump_len(child);
        child = child->next;
        if (child != NULL)
          len += 2;
      }
      len += 1;
      break;
    }
    case kTypeDictionary: {
      ExpressionNode *child = node->children;
      len += 1;
      while (child != NULL) {
        len += node_dump_len(child);
        child = child->next;
        assert(child != NULL);
        len += 3;
        len += node_dump_len(child);
        child = child->next;
        if (child != NULL)
          len += 2;
      }
      len += 1;
      break;
    }
    case kTypeSubscript: {
      assert(node->children != NULL);
      assert(node->children->next != NULL);
      len += node_dump_len(node->children);
      len += 1;
      len += node_dump_len(node->children->next);
      if (node->children->next->next != NULL) {
        assert(node->children->next->next->next == NULL);
        len += 3;
        len += node_dump_len(node->children->next->next);
      }
      len += 1;
      break;
    }
    case kTypeConcatOrSubscript: {
      assert(node->children != NULL);
      assert(node->children->next == NULL);
      len += node_dump_len(node->children);
      len += 1;
      len += node->end_position - node->position + 1;
      break;
    }
    case kTypeCall: {
      ExpressionNode *child;

      assert(node->children != NULL);
      len += node_dump_len(node->children);
      len += 1;
      child = node->children->next;
      while (child != NULL) {
        len += node_dump_len(child);
        child = child->next;
        if (child != NULL)
          len += 2;
      }
      len += 1;
      break;
    }
    case kTypeEmptySubscript: {
      break;
    }
    default: {
      assert(FALSE);
    }
  }
  return len;
}

size_t expr_node_dump_len(ExpressionNode *node)
{
  size_t len = node_dump_len(node);
  ExpressionNode *next = node->next;

  while (next != NULL) {
    len++;
    len += node_dump_len(next);
    next = next->next;
  }

  return len;
}

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

static void node_dump(ExpressionNode *node, char **pp)
{
  char *p = *pp;
  bool add_ccs = FALSE;

  switch (node->type) {
    case kTypeGreater:
    case kTypeGreaterThanOrEqualTo:
    case kTypeLess:
    case kTypeLessThanOrEqualTo:
    case kTypeEquals:
    case kTypeNotEquals:
    case kTypeIdentical:
    case kTypeNotIdentical:
    case kTypeMatches:
    case kTypeNotMatches: {
      add_ccs = TRUE;
      // fallthrough
    }
    case kTypeLogicalOr:
    case kTypeLogicalAnd:
    case kTypeAdd:
    case kTypeSubtract:
    case kTypeMultiply:
    case kTypeDivide:
    case kTypeModulo:
    case kTypeStringConcat: {
      ExpressionNode *child = node->children;
      size_t operator_len;
      char *operator;

      if (node->type == kTypeStringConcat)
        operator = ".";
      else
        operator = expression_type_string[node->type];
      operator_len = STRLEN(operator);

      child = node->children;
      do {
        node_dump(child, &p);
        child = child->next;
        if (child != NULL) {
          *p++ = ' ';
          memcpy(p, operator, operator_len);
          p += operator_len;
          *p++ = ' ';
          if (add_ccs)
            if (node->ignore_case)
              *p++ = *(case_compare_strategy_string[node->type]);
        }
      } while (child != NULL);

      break;
    }
    case kTypeTernaryConditional: {
      node_dump(node->children, &p);
      *p++ = ' ';
      *p++ = '?';
      *p++ = ' ';
      node_dump(node->children->next, &p);
      *p++ = ' ';
      *p++ = ':';
      *p++ = ' ';
      node_dump(node->children->next->next, &p);
      break;
    }
    case kTypeNot:
    case kTypeMinus:
    case kTypePlus: {
      *p++ = *(expression_type_string[node->type]);
      node_dump(node->children, &p);
      break;
    }
    case kTypeDecimalNumber:
    case kTypeOctalNumber:
    case kTypeHexNumber:
    case kTypeFloat:
    case kTypeDoubleQuotedString:
    case kTypeSingleQuotedString:
    case kTypeOption:
    case kTypeRegister:
    case kTypeEnvironmentVariable:
    case kTypeSimpleVariableName:
    case kTypeIdentifier: {
      size_t len = node->end_position - node->position + 1;
      memcpy(p, node->position, len);
      p += len;
      break;
    }
    case kTypeVariableName: {
      ExpressionNode *child = node->children;
      do {
        node_dump(child, &p);
        child = child->next;
      } while (child != NULL);
      break;
    }
    case kTypeCurlyName:
    case kTypeExpression: {
      *p++ = (node->type == kTypeExpression ? '(' : '{');
      node_dump(node->children, &p);
      *p++ = (node->type == kTypeExpression ? ')' : '}');
      break;
    }
    case kTypeList: {
      ExpressionNode *child = node->children;
      *p++ = '[';
      while (child != NULL) {
        node_dump(child, &p);
        child = child->next;
        if (child != NULL) {
          *p++ = ',';
          *p++ = ' ';
        }
      }
      *p++ = ']';
      break;
    }
    case kTypeDictionary: {
      ExpressionNode *child = node->children;
      *p++ = '{';
      while (child != NULL) {
        node_dump(child, &p);
        child = child->next;
        *p++ = ' ';
        *p++ = ':';
        *p++ = ' ';
        node_dump(child, &p);
        child = child->next;
        if (child != NULL) {
          *p++ = ',';
          *p++ = ' ';
        }
      }
      *p++ = '}';
      break;
    }
    case kTypeSubscript: {
      node_dump(node->children, &p);
      *p++ = '[';
      node_dump(node->children->next, &p);
      if (node->children->next->next != NULL) {
        *p++ = ' ';
        *p++ = ':';
        *p++ = ' ';
        node_dump(node->children->next->next, &p);
      }
      *p++ = ']';
      break;
    }
    case kTypeConcatOrSubscript: {
      size_t len = node->end_position - node->position + 1;
      node_dump(node->children, &p);
      *p++ = '.';
      memcpy(p, node->position, len);
      p += len;
      break;
    }
    case kTypeCall: {
      ExpressionNode *child;

      node_dump(node->children, &p);
      *p++ = '(';
      child = node->children->next;
      while (child != NULL) {
        node_dump(child, &p);
        child = child->next;
        if (child != NULL) {
          *p++ = ',';
          *p++ = ' ';
        }
      }
      *p++ = ')';
      break;
    }
    case kTypeEmptySubscript: {
      break;
    }
    default: {
      assert(FALSE);
    }
  }

  *pp = p;
}

void expr_node_dump(ExpressionNode *node, char **pp)
{
  ExpressionNode *next = node->next;

  node_dump(node, pp);

  while (next != NULL) {
    *(*pp)++ = ' ';
    node_dump(next, pp);
    next = next->next;
  }
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

char *parse0_repr(char_u *arg, bool dump_as_expr)
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
    len = (dump_as_expr ? expr_node_dump_len : node_repr_len)(p0_result->node);

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
  } while (--i);

  *p++ = ':';

  if (p0_result->error.message != NULL) {
    memcpy(p, "error:", 6);
    p += 6;
    STRCPY(p, p0_result->error.message);
  } else {
    (dump_as_expr ? expr_node_dump : node_repr)(p0_result->node, &p);
  }

theend:
  free_test_expr_result(p0_result);
  return result;
}
