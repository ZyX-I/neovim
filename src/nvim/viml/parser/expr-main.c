//FIXME!!!
#ifdef COMPILE_TEST_VERSION

#include "nvim/vim.h"
#include "nvim/strings.h"
#include "nvim/misc2.h"
#include "nvim/types.h"
#include "nvim/charset.h"
#include "nvim/viml/parser/expressions.h"
#include "nvim/viml/parser/ex_commands.h"
#include "nvim/viml/printer/printer.h"
#include "nvim/viml/testhelpers/parser.h"
#include <stdio.h>

#ifdef COMPILE_KLEE
#include "klee/klee.h"
#endif
static void print_node(int indent, ExpressionNode *node)
{
  char *name = NULL;
  switch (node->type) {
    case kExprUnknown: {
      printf("%*sUnknown\n", indent, "");
      break;
    }

    case kExprConcatOrSubscript: {
      if (name == NULL) name = "kExprConcatOrSubscript";
    }
    case kExprDecimalNumber: {
      if (name == NULL) name = "kExprDecimalNumber";
    }
    case kExprOctalNumber: {
      if (name == NULL) name = "kExprOctalNumber";
    }
    case kExprHexNumber: {
      if (name == NULL) name = "kExprHexNumber";
    }
    case kExprFloat: {
      if (name == NULL) name = "kExprFloat";
    }
    case kExprDoubleQuotedString: {
      if (name == NULL) name = "kExprDoubleQuotedString";
    }
    case kExprSingleQuotedString: {
      if (name == NULL) name = "kExprSingleQuotedString";
    }
    case kExprOption: {
      if (name == NULL) name = "kExprOption";
    }
    case kExprRegister: {
      if (name == NULL) name = "kExprRegister";
    }
    case kExprEnvironmentVariable: {
      if (name == NULL) name = "kExprEnvironmentVariable";
    }
    case kExprExpression: {
      if (name == NULL) name = "kExprExpression";
    }
    case kExprList: {
      if (name == NULL) name = "kExprList";
    }
    case kExprDictionary: {
      if (name == NULL) name = "kExprDictionary";
    }
    case kExprSubscript: {
      if (name == NULL) name = "kExprSubscript";
    }
    case kExprCall: {
      if (name == NULL) name = "kExprCall";
    }
    case kExprTernaryConditional: {
      if (name == NULL) name = "kExprTernaryConditional";
    }
    case kExprNot: {
      if (name == NULL) name = "kExprNot";
    }
    case kExprMinus: {
      if (name == NULL) name = "kExprMinus";
    }
    case kExprPlus: {
      if (name == NULL) name = "kExprPlus";
    }
    case kExprVariableName: {
      if (name == NULL) name = "kExprVariableName";
    }
    case kExprCurlyName: {
      if (name == NULL) name = "kExprCurlyName";
    }
    case kExprLogicalOr: {
      if (name == NULL) name = "kExprLogicalOr";
    }
    case kExprLogicalAnd: {
      if (name == NULL) name = "kExprLogicalAnd";
    }
    case kExprGreater: {
      if (name == NULL) name = "kExprGreater";
    }
    case kExprGreaterThanOrEqualTo: {
      if (name == NULL) name = "kExprGreaterThanOrEqualTo";
    }
    case kExprLess: {
      if (name == NULL) name = "kExprLess";
    }
    case kExprLessThanOrEqualTo: {
      if (name == NULL) name = "kExprLessThanOrEqualTo";
    }
    case kExprEquals: {
      if (name == NULL) name = "kExprEquals";
    }
    case kExprNotEquals: {
      if (name == NULL) name = "kExprNotEquals";
    }
    case kExprIdentical: {
      if (name == NULL) name = "kExprIdentical";
    }
    case kExprNotIdentical: {
      if (name == NULL) name = "kExprNotIdentical";
    }
    case kExprMatches: {
      if (name == NULL) name = "kExprMatches";
    }
    case kExprNotMatches: {
      if (name == NULL) name = "kExprNotMatches";
    }
    case kExprAdd: {
      if (name == NULL) name = "kExprAdd";
    }
    case kExprSubtract: {
      if (name == NULL) name = "kExprSubtract";
    }
    case kExprStringConcat: {
      if (name == NULL) name = "kExprStringConcat";
    }
    case kExprMultiply: {
      if (name == NULL) name = "kExprMultiply";
    }
    case kExprDivide: {
      if (name == NULL) name = "kExprDivide";
    }
    case kExprSimpleVariableName: {
      if (name == NULL) name = "kExprSimpleVariableName";
    }
    case kExprIdentifier: {
      if (name == NULL) name = "kExprIdentifier";
    }
    case kExprEmptySubscript: {
      if (name == NULL) name = "kExprEmptySubscript";
    }
    case kExprListRest: {
      if (name == NULL) name = "kExprListRest";
    }
    case kExprModulo: {
      if (name == NULL) name = "kExprModulo";
      printf("%*s%s\n", indent, "", name);
      if (node->position == NULL)
        printf("%*sNo position\n", indent + 1, "");
      else if (node->end_position == NULL)
        printf("%*sOne character: $%c$\n", indent + 1, "", *(node->position));
      else
        printf("%*sString: $%.*s$\n",
               indent + 1,
               "",
               (int) (node->end_position - node->position + 1),
               node->position);
      if (node->children != NULL)
      {
        printf("%*sSubnodes:\n", indent + 2, "");
        print_node(indent + 4, node->children);
      }
      if (node->next != NULL)
        print_node(indent, node->next);
      break;
    }
  }
}
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  ExpressionNode *node;
  char *result;
  /*
   * node = parse0((char_u *) argv[1]);
   * if (node != NULL)
   *   print_node(0, node);
   */
#ifdef COMPILE_KLEE
  char input[12];
  klee_make_symbolic(input, sizeof(input), "input");
  klee_assume(input[11] == '\0');
  if ((result = parse0_repr(input, true)) == NULL)
    return 1;
  return 0;
#else
  if ((result = parse0_repr(argv[1], true)) == NULL)
    return 1;
  puts(result);
  vim_free(result);
  if ((result = parse0_repr(argv[1], false)) == NULL)
    return 1;
  puts(result);
  free(result);
#endif
  return 0;
}

/*
 * skipwhite: skip over ' ' and '\t'.
 */
char_u *skipwhite(char_u *q)
{
  char_u      *p = q;

  while (vim_iswhite(*p))   /* skip to next non-white */
    ++p;
  return p;
}

char_u *alloc_clear(unsigned size)
{
  void *res = (char_u *) malloc(size);
  memset(res, 0, size);
  return res;
}

void *xrealloc(void *ptr, size_t size)
{
  return realloc(ptr, size);
}

void sort_strings(char_u **a, int b)
{
  return;
}

int vim_fnamecmp(char_u *a, char_u *b)
{
  return 0;
}

char *xstrdup(const char *s)
{
  return strdup(s);
}

void *xmallocz(size_t size)
{
  size_t total_size = size + 1;
  void *ret;

  ret = malloc(total_size);
  ((char*)ret)[size] = 0;

  return ret;
}

char *xstpcpy(char *restrict dst, const char *restrict src)
{
  const size_t len = strlen(src);
  return (char *)memcpy(dst, src, len + 1) + len;
}

char_u *vim_strnsave(char_u *string, int len)
{
  char_u      *p;

  p = xmallocz((unsigned)(len + 1));
  STRNCPY(p, string, len);
  return p;
}

int parse_one_cmd(const char_u **pp,
                  CommandNode **node,
                  CommandParserOptions o,
                  CommandPosition *position,
                  LineGetter fgetline,
                  void *cookie)
{
  return 0;
}

void free_cmd(CommandNode *cmd)
{
  return;
}

CommandNode *parse_cmd_sequence(CommandParserOptions o,
                                CommandPosition position,
                                LineGetter fgetline,
                                void *cookie)
{
  return NULL;
}

void cmd_repr(const PrinterOptions *const po, const CommandNode *node,
              char **pp)
{
  return;
}

size_t cmd_repr_len(const PrinterOptions *const po, const CommandNode *node)
{
  return 0;
}

/*
 * Return true if 'c' is a normal identifier character:
 * Letters and characters from the 'isident' option.
 */
int vim_isIDc(int c)
{
  return ASCII_ISALNUM(c);
  //return c > 0 && c < 0x100 && (chartab[c] & CT_ID_CHAR);
}

/*
 * skip over digits
 */
char_u *skipdigits(char_u *q)
{
  char_u      *p = q;

  while (VIM_ISDIGIT(*p))       /* skip to next non-digit */
    ++p;
  return p;
}

/*
 * Compare two strings, for length "len", ignoring case, using current locale.
 * Doesn't work for multi-byte characters.
 * return 0 for match, < 0 for smaller, > 0 for bigger
 */
int vim_strnicmp(char *s1, char *s2, size_t len)
{
  int i;

  while (len > 0) {
    i = (int)TOLOWER_LOC(*s1) - (int)TOLOWER_LOC(*s2);
    if (i != 0)
      return i;                             /* this character different */
    if (*s1 == NUL)
      break;                                /* strings match until NUL */
    ++s1;
    ++s2;
    --len;
  }
  return 0;                                 /* strings match */
}

/*
 * Variant of isxdigit() that can handle characters > 0x100.
 * We don't use isxdigit() here, because on some systems it also considers
 * superscript 1 to be a digit.
 */
int vim_isxdigit(int c)
{
  return (c >= '0' && c <= '9')
         || (c >= 'a' && c <= 'f')
         || (c >= 'A' && c <= 'F');
}

char_u *vim_strchr(char_u *s, int c)
{
  return (char_u *) strchr((char *) s, c);
}
#endif  // COMPILE_TEST_VERSION
int abc() {
  return 0;
}
