//FIXME!!!
#ifdef COMPILE_TEST_VERSION

#include "nvim/vim.h"
#include "nvim/strings.h"
#include "nvim/misc2.h"
#include "nvim/types.h"
#include "nvim/ascii.h"
#include "nvim/charset.h"
#include "nvim/viml/parser/expressions.h"
#include "nvim/viml/parser/ex_commands.h"
#include "nvim/viml/printer/printer.h"
#include "nvim/viml/testhelpers/parser.h"
#include "nvim/viml/dumpers/dumpers.h"
#include <stdio.h>

#ifdef COMPILE_KLEE
#include "klee/klee.h"
#endif

static Writer write_file = (Writer) &fwrite;
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  ExpressionNode *node;
  char *result;
#ifdef COMPILE_KLEE
  char input[12];
  klee_make_symbolic(input, sizeof(input), "input");
  klee_assume(input[11] == '\0');
  if ((result = srepresent_parse0(input, true)) == NULL)
    return 1;
#else
  if (represent_parse0(argv[1], false) == FAIL)
    return 1;
  putc('\n', stdout);
  if (represent_parse0(argv[1], true) == FAIL)
    return 2;
  putc('\n', stdout);
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

void *xcalloc(size_t nmemb, size_t size)
{
  return calloc(nmemb, size);
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

char *xstrndup(char *string, int len)
{
  return strndup(string, len);
}

int parse_one_cmd(const char **pp,
                  CommandNode **node,
                  CommandParserOptions o,
                  CommandPosition position,
                  VimlLineGetter fgetline,
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
                                VimlLineGetter fgetline,
                                void *cookie, bool can_free)
{
  return NULL;
}

void print_cmd(const StyleOptions *const po, const CommandNode *node,
               Writer write, void *cookie)
{
  return;
}

void sprint_cmd(const StyleOptions *const po, const CommandNode *node,
              char **pp)
{
  return;
}

size_t sprint_cmd_len(const StyleOptions *const po, const CommandNode *node)
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

/// Duplicates a chunk of memory using xmalloc
///
/// @see {xmalloc}
/// @param data pointer to the chunk
/// @param len size of the chunk
/// @return a pointer
void *xmemdup(const void *data, size_t len)
  FUNC_ATTR_MALLOC FUNC_ATTR_ALLOC_SIZE(2) FUNC_ATTR_NONNULL_RET
  FUNC_ATTR_WARN_UNUSED_RESULT FUNC_ATTR_NONNULL_ALL
{
  return memcpy(malloc(len), data, len);
}

_Bool do_log(int log_level, const char *func_name, int line_num, const char *fmt, ...)
{
  return true;
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
