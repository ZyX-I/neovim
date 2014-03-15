//FIXME!!!
#ifdef COMPILE_TEST_VERSION

#include "cmd.h"
#include <stdlib.h>
#include <string.h>

#define enc_utf8 TRUE
#define enc_dbcs FALSE
#define has_mbyte TRUE

char_u *p_cpo = (char_u *)"";
char_u e_nobang[] = "no bang";
char_u e_norange[] = "no range";
char_u e_backslash[] = "backslash";

int (*mb_ptr2len)(char_u *p);

int main(int argc, char **argv)
{
  unsigned flags = 0;
  mb_ptr2len = &utfc_ptr2len;

  if (argc == 3)
    sscanf(argv[2], "%u", &flags);

  parse_one_cmd_test((char_u *) argv[1], (uint_least8_t) flags);
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

/*
 * Copy up to "len" bytes of "string" into newly allocated memory and
 * terminate with a NUL.
 * The allocated memory always has size "len + 1", also when "string" is
 * shorter.
 */
char_u *vim_strnsave(char_u *string, int len)
{
  char_u      *p;

  p = malloc((unsigned)(len + 1));
  if (p != NULL) {
    STRNCPY(p, string, len);
    p[len] = NUL;
  }
  return p;
}

/*
 * Version of strchr() and strrchr() that handle unsigned char strings
 * with characters from 128 to 255 correctly.  It also doesn't return a
 * pointer to the NUL at the end of the string.
 */
char_u *vim_strchr(char_u *string, int c)
{
  char_u      *p;
  int b;

  p = string;
  if (enc_utf8 && c >= 0x80) {
    while (*p != NUL) {
      if (utf_ptr2char(p) == c)
        return p;
      p += (*mb_ptr2len)(p);
    }
    return NULL;
  }
  if (enc_dbcs != 0 && c > 255) {
    int n2 = c & 0xff;

    c = ((unsigned)c >> 8) & 0xff;
    while ((b = *p) != NUL) {
      if (b == c && p[1] == n2)
        return p;
      p += (*mb_ptr2len)(p);
    }
    return NULL;
  }
  if (has_mbyte) {
    while ((b = *p) != NUL) {
      if (b == c)
        return p;
      p += (*mb_ptr2len)(p);
    }
    return NULL;
  }
  while ((b = *p) != NUL) {
    if (b == c)
      return p;
    ++p;
  }
  return NULL;
}

/*
 * Getdigits: Get a number from a string and skip over it.
 * Note: the argument is a pointer to a char_u pointer!
 */
long getdigits(char_u **pp)
{
  char_u      *p;
  long retval;

  p = *pp;
  retval = atol((char *)p);
  if (*p == '-')                /* skip negative sign */
    ++p;
  p = skipdigits(p);            /* skip to next non-digit */
  *pp = p;
  return retval;
}

/*
 * Copy "string" into newly allocated memory.
 */
char_u *vim_strsave(char_u *string)
{
  char_u      *p;
  unsigned len;

  len = (unsigned)STRLEN(string) + 1;
  p = malloc(len);
  if (p != NULL)
    memmove(p, string, (size_t)len);
  return p;
}

/*
 * Like utf8len_tab above, but using a zero for illegal lead bytes.
 */
static char utf8len_tab_zero[256] =
{
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,0,0,
};

/*
 * Convert a UTF-8 byte sequence to a wide character.
 * If the sequence is illegal or truncated by a NUL the first byte is
 * returned.
 * Does not include composing characters, of course.
 */
int utf_ptr2char(char_u *p)
{
  int len;

  if (p[0] < 0x80)      /* be quick for ASCII */
    return p[0];

  len = utf8len_tab_zero[p[0]];
  if (len > 1 && (p[1] & 0xc0) == 0x80) {
    if (len == 2)
      return ((p[0] & 0x1f) << 6) + (p[1] & 0x3f);
    if ((p[2] & 0xc0) == 0x80) {
      if (len == 3)
        return ((p[0] & 0x0f) << 12) + ((p[1] & 0x3f) << 6)
          + (p[2] & 0x3f);
      if ((p[3] & 0xc0) == 0x80) {
        if (len == 4)
          return ((p[0] & 0x07) << 18) + ((p[1] & 0x3f) << 12)
            + ((p[2] & 0x3f) << 6) + (p[3] & 0x3f);
        if ((p[4] & 0xc0) == 0x80) {
          if (len == 5)
            return ((p[0] & 0x03) << 24) + ((p[1] & 0x3f) << 18)
              + ((p[2] & 0x3f) << 12) + ((p[3] & 0x3f) << 6)
              + (p[4] & 0x3f);
          if ((p[5] & 0xc0) == 0x80 && len == 6)
            return ((p[0] & 0x01) << 30) + ((p[1] & 0x3f) << 24)
              + ((p[2] & 0x3f) << 18) + ((p[3] & 0x3f) << 12)
              + ((p[4] & 0x3f) << 6) + (p[5] & 0x3f);
        }
      }
    }
  }
  /* Illegal value, just return the first byte */
  return p[0];
}

/*
 * Return the number of bytes the UTF-8 encoding of the character at "p" takes.
 * This includes following composing characters.
 */
int utfc_ptr2len(char_u *p)
{
  int len;
  int b0 = *p;
  int prevlen;

  if (b0 == NUL)
    return 0;
  if (b0 < 0x80 && p[1] < 0x80)         /* be quick for ASCII */
    return 1;

  /* Skip over first UTF-8 char, stopping at a NUL byte. */
  len = utf_ptr2len(p);

  /* Check for illegal byte. */
  if (len == 1 && b0 >= 0x80)
    return 1;

  /*
   * Check for composing characters.  We can handle only the first six, but
   * skip all of them (otherwise the cursor would get stuck).
   */
  prevlen = 0;
  for (;; ) {
    if (p[len] < 0x80 || !UTF_COMPOSINGLIKE(p + prevlen, p + len))
      return len;

    /* Skip over composing char */
    prevlen = len;
    len += utf_ptr2len(p + len);
  }
}

/*
 * Lookup table to quickly get the length in bytes of a UTF-8 character from
 * the first byte of a UTF-8 string.
 * Bytes which are illegal when used as the first byte have a 1.
 * The NUL byte has length 1.
 */
static char utf8len_tab[256] =
{
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1,
};

/*
 * Get the length of a UTF-8 byte sequence, not including any following
 * composing characters.
 * Returns 0 for "".
 * Returns 1 for an illegal byte sequence.
 */
int utf_ptr2len(char_u *p)
{
  int len;
  int i;

  if (*p == NUL)
    return 0;
  len = utf8len_tab[*p];
  for (i = 1; i < len; ++i)
    if ((p[i] & 0xc0) != 0x80)
      return 1;
  return len;
}

/*
 * Check if the character pointed to by "p2" is a composing character when it
 * comes after "p1".  For Arabic sometimes "ab" is replaced with "c", which
 * behaves like a composing character.
 */
int utf_composinglike(char_u *p1, char_u *p2)
{
  int c2;

  c2 = utf_ptr2char(p2);
  if (utf_iscomposing(c2))
    return TRUE;
  if (!arabic_maycombine(c2))
    return FALSE;
  return arabic_combine(utf_ptr2char(p1), c2);
}

/*
 * Check whether we are dealing with Arabic combining characters.
 * Note: these are NOT really composing characters!
 */
int arabic_combine(
    int one,                    /* first character */
    int two                     /* character just after "one" */
    )
{
  if (one == a_LAM)
    return arabic_maycombine(two);
  return FALSE;
}

/*
 * Check whether we are dealing with a character that could be regarded as an
 * Arabic combining character, need to check the character before this.
 */
int arabic_maycombine(int two)
{
  if (TRUE && !FALSE)
    return two == a_ALEF_MADDA
      || two == a_ALEF_HAMZA_ABOVE
      || two == a_ALEF_HAMZA_BELOW
      || two == a_ALEF;
  return FALSE;
}

struct interval {
  long first;
  long last;
};
static int intable(struct interval *table, size_t size, int c);

/*
 * Return TRUE if "c" is in "table[size / sizeof(struct interval)]".
 */
static int intable(struct interval *table, size_t size, int c)
{
  int mid, bot, top;

  /* first quick check for Latin1 etc. characters */
  if (c < table[0].first)
    return FALSE;

  /* binary search in table */
  bot = 0;
  top = (int)(size / sizeof(struct interval) - 1);
  while (top >= bot) {
    mid = (bot + top) / 2;
    if (table[mid].last < c)
      bot = mid + 1;
    else if (table[mid].first > c)
      top = mid - 1;
    else
      return TRUE;
  }
  return FALSE;
}

/*
 * Return TRUE if "c" is a composing UTF-8 character.  This means it will be
 * drawn on top of the preceding character.
 * Based on code from Markus Kuhn.
 */
int utf_iscomposing(int c)
{
  /* Sorted list of non-overlapping intervals.
   * Generated by ../runtime/tools/unicode.vim. */
  static struct interval combining[] =
  {
    {0x0300, 0x036f},
    {0x0483, 0x0489},
    {0x0591, 0x05bd},
    {0x05bf, 0x05bf},
    {0x05c1, 0x05c2},
    {0x05c4, 0x05c5},
    {0x05c7, 0x05c7},
    {0x0610, 0x061a},
    {0x064b, 0x065e},
    {0x0670, 0x0670},
    {0x06d6, 0x06dc},
    {0x06de, 0x06e4},
    {0x06e7, 0x06e8},
    {0x06ea, 0x06ed},
    {0x0711, 0x0711},
    {0x0730, 0x074a},
    {0x07a6, 0x07b0},
    {0x07eb, 0x07f3},
    {0x0816, 0x0819},
    {0x081b, 0x0823},
    {0x0825, 0x0827},
    {0x0829, 0x082d},
    {0x0900, 0x0903},
    {0x093c, 0x093c},
    {0x093e, 0x094e},
    {0x0951, 0x0955},
    {0x0962, 0x0963},
    {0x0981, 0x0983},
    {0x09bc, 0x09bc},
    {0x09be, 0x09c4},
    {0x09c7, 0x09c8},
    {0x09cb, 0x09cd},
    {0x09d7, 0x09d7},
    {0x09e2, 0x09e3},
    {0x0a01, 0x0a03},
    {0x0a3c, 0x0a3c},
    {0x0a3e, 0x0a42},
    {0x0a47, 0x0a48},
    {0x0a4b, 0x0a4d},
    {0x0a51, 0x0a51},
    {0x0a70, 0x0a71},
    {0x0a75, 0x0a75},
    {0x0a81, 0x0a83},
    {0x0abc, 0x0abc},
    {0x0abe, 0x0ac5},
    {0x0ac7, 0x0ac9},
    {0x0acb, 0x0acd},
    {0x0ae2, 0x0ae3},
    {0x0b01, 0x0b03},
    {0x0b3c, 0x0b3c},
    {0x0b3e, 0x0b44},
    {0x0b47, 0x0b48},
    {0x0b4b, 0x0b4d},
    {0x0b56, 0x0b57},
    {0x0b62, 0x0b63},
    {0x0b82, 0x0b82},
    {0x0bbe, 0x0bc2},
    {0x0bc6, 0x0bc8},
    {0x0bca, 0x0bcd},
    {0x0bd7, 0x0bd7},
    {0x0c01, 0x0c03},
    {0x0c3e, 0x0c44},
    {0x0c46, 0x0c48},
    {0x0c4a, 0x0c4d},
    {0x0c55, 0x0c56},
    {0x0c62, 0x0c63},
    {0x0c82, 0x0c83},
    {0x0cbc, 0x0cbc},
    {0x0cbe, 0x0cc4},
    {0x0cc6, 0x0cc8},
    {0x0cca, 0x0ccd},
    {0x0cd5, 0x0cd6},
    {0x0ce2, 0x0ce3},
    {0x0d02, 0x0d03},
    {0x0d3e, 0x0d44},
    {0x0d46, 0x0d48},
    {0x0d4a, 0x0d4d},
    {0x0d57, 0x0d57},
    {0x0d62, 0x0d63},
    {0x0d82, 0x0d83},
    {0x0dca, 0x0dca},
    {0x0dcf, 0x0dd4},
    {0x0dd6, 0x0dd6},
    {0x0dd8, 0x0ddf},
    {0x0df2, 0x0df3},
    {0x0e31, 0x0e31},
    {0x0e34, 0x0e3a},
    {0x0e47, 0x0e4e},
    {0x0eb1, 0x0eb1},
    {0x0eb4, 0x0eb9},
    {0x0ebb, 0x0ebc},
    {0x0ec8, 0x0ecd},
    {0x0f18, 0x0f19},
    {0x0f35, 0x0f35},
    {0x0f37, 0x0f37},
    {0x0f39, 0x0f39},
    {0x0f3e, 0x0f3f},
    {0x0f71, 0x0f84},
    {0x0f86, 0x0f87},
    {0x0f90, 0x0f97},
    {0x0f99, 0x0fbc},
    {0x0fc6, 0x0fc6},
    {0x102b, 0x103e},
    {0x1056, 0x1059},
    {0x105e, 0x1060},
    {0x1062, 0x1064},
    {0x1067, 0x106d},
    {0x1071, 0x1074},
    {0x1082, 0x108d},
    {0x108f, 0x108f},
    {0x109a, 0x109d},
    {0x135f, 0x135f},
    {0x1712, 0x1714},
    {0x1732, 0x1734},
    {0x1752, 0x1753},
    {0x1772, 0x1773},
    {0x17b6, 0x17d3},
    {0x17dd, 0x17dd},
    {0x180b, 0x180d},
    {0x18a9, 0x18a9},
    {0x1920, 0x192b},
    {0x1930, 0x193b},
    {0x19b0, 0x19c0},
    {0x19c8, 0x19c9},
    {0x1a17, 0x1a1b},
    {0x1a55, 0x1a5e},
    {0x1a60, 0x1a7c},
    {0x1a7f, 0x1a7f},
    {0x1b00, 0x1b04},
    {0x1b34, 0x1b44},
    {0x1b6b, 0x1b73},
    {0x1b80, 0x1b82},
    {0x1ba1, 0x1baa},
    {0x1c24, 0x1c37},
    {0x1cd0, 0x1cd2},
    {0x1cd4, 0x1ce8},
    {0x1ced, 0x1ced},
    {0x1cf2, 0x1cf2},
    {0x1dc0, 0x1de6},
    {0x1dfd, 0x1dff},
    {0x20d0, 0x20f0},
    {0x2cef, 0x2cf1},
    {0x2de0, 0x2dff},
    {0x302a, 0x302f},
    {0x3099, 0x309a},
    {0xa66f, 0xa672},
    {0xa67c, 0xa67d},
    {0xa6f0, 0xa6f1},
    {0xa802, 0xa802},
    {0xa806, 0xa806},
    {0xa80b, 0xa80b},
    {0xa823, 0xa827},
    {0xa880, 0xa881},
    {0xa8b4, 0xa8c4},
    {0xa8e0, 0xa8f1},
    {0xa926, 0xa92d},
    {0xa947, 0xa953},
    {0xa980, 0xa983},
    {0xa9b3, 0xa9c0},
    {0xaa29, 0xaa36},
    {0xaa43, 0xaa43},
    {0xaa4c, 0xaa4d},
    {0xaa7b, 0xaa7b},
    {0xaab0, 0xaab0},
    {0xaab2, 0xaab4},
    {0xaab7, 0xaab8},
    {0xaabe, 0xaabf},
    {0xaac1, 0xaac1},
    {0xabe3, 0xabea},
    {0xabec, 0xabed},
    {0xfb1e, 0xfb1e},
    {0xfe00, 0xfe0f},
    {0xfe20, 0xfe26},
    {0x101fd, 0x101fd},
    {0x10a01, 0x10a03},
    {0x10a05, 0x10a06},
    {0x10a0c, 0x10a0f},
    {0x10a38, 0x10a3a},
    {0x10a3f, 0x10a3f},
    {0x11080, 0x11082},
    {0x110b0, 0x110ba},
    {0x1d165, 0x1d169},
    {0x1d16d, 0x1d172},
    {0x1d17b, 0x1d182},
    {0x1d185, 0x1d18b},
    {0x1d1aa, 0x1d1ad},
    {0x1d242, 0x1d244},
    {0xe0100, 0xe01ef}
  };

  return intable(combining, sizeof(combining), c);
}

#endif
int abc2() {
  return 0;
}
