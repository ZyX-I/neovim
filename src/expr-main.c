//FIXME!!!
#ifdef COMPILE_TEST_VERSION

#include "nvim/vim.h"
#include "nvim/misc2.h"
#include "nvim/types.h"
#include "nvim/expr.h"
#include "nvim/charset.h"
#include "nvim/expr.h"
#include <stdio.h>
static void print_node(int indent, ExpressionNode *node)
{
  char *name = NULL;
  switch (node->type) {
    case kTypeUnknown: {
      printf("%*sUnknown\n", indent, "");
      break;
    }

    case kTypeConcatOrSubscript: {
      if (name == NULL) name = "kTypeConcatOrSubscript";
    }
    case kTypeDecimalNumber: {
      if (name == NULL) name = "kTypeDecimalNumber";
    }
    case kTypeOctalNumber: {
      if (name == NULL) name = "kTypeOctalNumber";
    }
    case kTypeHexNumber: {
      if (name == NULL) name = "kTypeHexNumber";
    }
    case kTypeFloat: {
      if (name == NULL) name = "kTypeFloat";
    }
    case kTypeDoubleQuotedString: {
      if (name == NULL) name = "kTypeDoubleQuotedString";
    }
    case kTypeSingleQuotedString: {
      if (name == NULL) name = "kTypeSingleQuotedString";
    }
    case kTypeOption: {
      if (name == NULL) name = "kTypeOption";
    }
    case kTypeRegister: {
      if (name == NULL) name = "kTypeRegister";
    }
    case kTypeEnvironmentVariable: {
      if (name == NULL) name = "kTypeEnvironmentVariable";
    }
    case kTypeExpression: {
      if (name == NULL) name = "kTypeExpression";
    }
    case kTypeList: {
      if (name == NULL) name = "kTypeList";
    }
    case kTypeDictionary: {
      if (name == NULL) name = "kTypeDictionary";
    }
    case kTypeSubscript: {
      if (name == NULL) name = "kTypeSubscript";
    }
    case kTypeCall: {
      if (name == NULL) name = "kTypeCall";
    }
    case kTypeTernaryConditional: {
      if (name == NULL) name = "kTypeTernaryConditional";
    }
    case kTypeNot: {
      if (name == NULL) name = "kTypeNot";
    }
    case kTypeMinus: {
      if (name == NULL) name = "kTypeMinus";
    }
    case kTypePlus: {
      if (name == NULL) name = "kTypePlus";
    }
    case kTypeVariableName: {
      if (name == NULL) name = "kTypeVariableName";
    }
    case kTypeCurlyName: {
      if (name == NULL) name = "kTypeCurlyName";
    }
    case kTypeLogicalOr: {
      if (name == NULL) name = "kTypeLogicalOr";
    }
    case kTypeLogicalAnd: {
      if (name == NULL) name = "kTypeLogicalAnd";
    }
    case kTypeGreater: {
      if (name == NULL) name = "kTypeGreater";
    }
    case kTypeGreaterThanOrEqualTo: {
      if (name == NULL) name = "kTypeGreaterThanOrEqualTo";
    }
    case kTypeLess: {
      if (name == NULL) name = "kTypeLess";
    }
    case kTypeLessThanOrEqualTo: {
      if (name == NULL) name = "kTypeLessThanOrEqualTo";
    }
    case kTypeEquals: {
      if (name == NULL) name = "kTypeEquals";
    }
    case kTypeNotEquals: {
      if (name == NULL) name = "kTypeNotEquals";
    }
    case kTypeIdentical: {
      if (name == NULL) name = "kTypeIdentical";
    }
    case kTypeNotIdentical: {
      if (name == NULL) name = "kTypeNotIdentical";
    }
    case kTypeMatches: {
      if (name == NULL) name = "kTypeMatches";
    }
    case kTypeNotMatches: {
      if (name == NULL) name = "kTypeNotMatches";
    }
    case kTypeAdd: {
      if (name == NULL) name = "kTypeAdd";
    }
    case kTypeSubstract: {
      if (name == NULL) name = "kTypeSubstract";
    }
    case kTypeStringConcat: {
      if (name == NULL) name = "kTypeStringConcat";
    }
    case kTypeMultiply: {
      if (name == NULL) name = "kTypeMultiply";
    }
    case kTypeDivide: {
      if (name == NULL) name = "kTypeDivide";
    }
    case kTypeSimpleVariableName: {
      if (name == NULL) name = "kTypeSimpleVariableName";
    }
    case kTypeIdentifier: {
      if (name == NULL) name = "kTypeIdentifier";
    }
    case kTypeEmptySubscript: {
      if (name == NULL) name = "kTypeEmptySubscript";
    }
    case kTypeModulo: {
      if (name == NULL) name = "kTypeModulo";
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
  node = parse0((char_u *) argv[1]);
  if (node != NULL)
    print_node(0, node);
  return 1;
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

void vim_free(void *v)
{
  free(v);
}

/*
 * Return TRUE if 'c' is a normal identifier character:
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
#endif  // COMPILE_TEST_VERSION
int abc() {
  return 0;
}
