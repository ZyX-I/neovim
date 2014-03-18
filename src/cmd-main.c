//FIXME!!!
#ifdef COMPILE_TEST_VERSION

#include "cmd.h"
#include <stdlib.h>
#include <string.h>
#include "term.h"

#define enc_utf8 TRUE
#define enc_dbcs FALSE
#define has_mbyte TRUE
#define current_SID 1
#define mb_ptr2char utf_ptr2char
#define mb_char2bytes utf_char2bytes

char_u e_nobang[] = "no bang";
char_u e_norange[] = "no range";
char_u e_backslash[] = "backslash";
char_u e_trailing[] = "trailing";

int (*mb_ptr2len)(char_u *p);

int main(int argc, char **argv)
{
  unsigned flags = 0;
  mb_ptr2len = &utfc_ptr2len;
  char *repr;

  if (argc == 3)
    sscanf(argv[2], "%u", &flags);

  if ((repr = parse_one_cmd_test((char_u *) argv[1], (uint_least8_t) flags))
      == NULL)
    return 1;

  puts(repr);

  free(repr);

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

void *vim_memset(void *src, int c, size_t n)
{
  return memset(src, c, n);
}

void mch_memmove(void *dest, void *src, size_t n)
{
  memmove(dest, src, n);
}

char_u *alloc(unsigned size)
{
  return malloc(size);
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

static struct termcode {
  char_u name[2];           /* termcap name of entry */
  char_u  *code;            /* terminal code (in allocated memory) */
  int len;                  /* STRLEN(code) */
  int modlen;               /* length of part before ";*~". */
} *termcodes = NULL;

static int tc_max_len = 0;  /* number of entries that termcodes[] can hold */
static int tc_len = 0;      /* current number of entries in termcodes[] */

/*
 * Return the value of a single hex character.
 * Only valid when the argument is '0' - '9', 'A' - 'F' or 'a' - 'f'.
 */
int hex2nr(int c)
{
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return c - '0';
}

static struct modmasktable {
  short mod_mask;               /* Bit-mask for particular key modifier */
  short mod_flag;               /* Bit(s) for particular key modifier */
  char_u name;                  /* Single letter name of modifier */
} mod_mask_table[] =
{
  {MOD_MASK_ALT,              MOD_MASK_ALT,           (char_u)'M'},
  {MOD_MASK_META,             MOD_MASK_META,          (char_u)'T'},
  {MOD_MASK_CTRL,             MOD_MASK_CTRL,          (char_u)'C'},
  {MOD_MASK_SHIFT,            MOD_MASK_SHIFT,         (char_u)'S'},
  {MOD_MASK_MULTI_CLICK,      MOD_MASK_2CLICK,        (char_u)'2'},
  {MOD_MASK_MULTI_CLICK,      MOD_MASK_3CLICK,        (char_u)'3'},
  {MOD_MASK_MULTI_CLICK,      MOD_MASK_4CLICK,        (char_u)'4'},
  /* 'A' must be the last one */
  {MOD_MASK_ALT,              MOD_MASK_ALT,           (char_u)'A'},
  {0, 0, NUL}
};

static char_u modifier_keys_table[] =
{
  /*  mod mask	    with modifier		without modifier */
  MOD_MASK_SHIFT, '&', '9',                   '@', '1',         /* begin */
  MOD_MASK_SHIFT, '&', '0',                   '@', '2',         /* cancel */
  MOD_MASK_SHIFT, '*', '1',                   '@', '4',         /* command */
  MOD_MASK_SHIFT, '*', '2',                   '@', '5',         /* copy */
  MOD_MASK_SHIFT, '*', '3',                   '@', '6',         /* create */
  MOD_MASK_SHIFT, '*', '4',                   'k', 'D',         /* delete char */
  MOD_MASK_SHIFT, '*', '5',                   'k', 'L',         /* delete line */
  MOD_MASK_SHIFT, '*', '7',                   '@', '7',         /* end */
  MOD_MASK_CTRL,  KS_EXTRA, (int)KE_C_END,    '@', '7',         /* end */
  MOD_MASK_SHIFT, '*', '9',                   '@', '9',         /* exit */
  MOD_MASK_SHIFT, '*', '0',                   '@', '0',         /* find */
  MOD_MASK_SHIFT, '#', '1',                   '%', '1',         /* help */
  MOD_MASK_SHIFT, '#', '2',                   'k', 'h',         /* home */
  MOD_MASK_CTRL,  KS_EXTRA, (int)KE_C_HOME,   'k', 'h',         /* home */
  MOD_MASK_SHIFT, '#', '3',                   'k', 'I',         /* insert */
  MOD_MASK_SHIFT, '#', '4',                   'k', 'l',         /* left arrow */
  MOD_MASK_CTRL,  KS_EXTRA, (int)KE_C_LEFT,   'k', 'l',         /* left arrow */
  MOD_MASK_SHIFT, '%', 'a',                   '%', '3',         /* message */
  MOD_MASK_SHIFT, '%', 'b',                   '%', '4',         /* move */
  MOD_MASK_SHIFT, '%', 'c',                   '%', '5',         /* next */
  MOD_MASK_SHIFT, '%', 'd',                   '%', '7',         /* options */
  MOD_MASK_SHIFT, '%', 'e',                   '%', '8',         /* previous */
  MOD_MASK_SHIFT, '%', 'f',                   '%', '9',         /* print */
  MOD_MASK_SHIFT, '%', 'g',                   '%', '0',         /* redo */
  MOD_MASK_SHIFT, '%', 'h',                   '&', '3',         /* replace */
  MOD_MASK_SHIFT, '%', 'i',                   'k', 'r',         /* right arr. */
  MOD_MASK_CTRL,  KS_EXTRA, (int)KE_C_RIGHT,  'k', 'r',         /* right arr. */
  MOD_MASK_SHIFT, '%', 'j',                   '&', '5',         /* resume */
  MOD_MASK_SHIFT, '!', '1',                   '&', '6',         /* save */
  MOD_MASK_SHIFT, '!', '2',                   '&', '7',         /* suspend */
  MOD_MASK_SHIFT, '!', '3',                   '&', '8',         /* undo */
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_UP,     'k', 'u',         /* up arrow */
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_DOWN,   'k', 'd',         /* down arrow */

  /* vt100 F1 */
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_XF1,    KS_EXTRA, (int)KE_XF1,
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_XF2,    KS_EXTRA, (int)KE_XF2,
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_XF3,    KS_EXTRA, (int)KE_XF3,
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_XF4,    KS_EXTRA, (int)KE_XF4,

  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F1,     'k', '1',         /* F1 */
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F2,     'k', '2',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F3,     'k', '3',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F4,     'k', '4',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F5,     'k', '5',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F6,     'k', '6',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F7,     'k', '7',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F8,     'k', '8',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F9,     'k', '9',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F10,    'k', ';',         /* F10 */

  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F11,    'F', '1',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F12,    'F', '2',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F13,    'F', '3',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F14,    'F', '4',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F15,    'F', '5',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F16,    'F', '6',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F17,    'F', '7',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F18,    'F', '8',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F19,    'F', '9',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F20,    'F', 'A',

  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F21,    'F', 'B',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F22,    'F', 'C',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F23,    'F', 'D',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F24,    'F', 'E',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F25,    'F', 'F',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F26,    'F', 'G',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F27,    'F', 'H',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F28,    'F', 'I',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F29,    'F', 'J',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F30,    'F', 'K',

  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F31,    'F', 'L',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F32,    'F', 'M',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F33,    'F', 'N',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F34,    'F', 'O',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F35,    'F', 'P',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F36,    'F', 'Q',
  MOD_MASK_SHIFT, KS_EXTRA, (int)KE_S_F37,    'F', 'R',

  /* TAB pseudo code*/
  MOD_MASK_SHIFT, 'k', 'B',                   KS_EXTRA, (int)KE_TAB,

  NUL
};

/*
 * Convert a string into a long and/or unsigned long, taking care of
 * hexadecimal and octal numbers.  Accepts a '-' sign.
 * If "hexp" is not NULL, returns a flag to indicate the type of the number:
 *  0	    decimal
 *  '0'	    octal
 *  'X'	    hex
 *  'x'	    hex
 * If "len" is not NULL, the length of the number in characters is returned.
 * If "nptr" is not NULL, the signed result is returned in it.
 * If "unptr" is not NULL, the unsigned result is returned in it.
 * If "dooct" is non-zero recognize octal numbers, when > 1 always assume
 * octal number.
 * If "dohex" is non-zero recognize hex numbers, when > 1 always assume
 * hex number.
 */
void 
vim_str2nr (
    char_u *start,
    int *hexp,          /* return: type of number 0 = decimal, 'x'
                                       or 'X' is hex, '0' = octal */
    int *len,           /* return: detected length of number */
    int dooct,                          /* recognize octal number */
    int dohex,                          /* recognize hex number */
    long *nptr,          /* return: signed result */
    unsigned long *unptr         /* return: unsigned result */
)
{
  char_u          *ptr = start;
  int hex = 0;                          /* default is decimal */
  int negative = FALSE;
  unsigned long un = 0;
  int n;

  if (ptr[0] == '-') {
    negative = TRUE;
    ++ptr;
  }

  /* Recognize hex and octal. */
  if (ptr[0] == '0' && ptr[1] != '8' && ptr[1] != '9') {
    hex = ptr[1];
    if (dohex && (hex == 'X' || hex == 'x') && vim_isxdigit(ptr[2]))
      ptr += 2;                         /* hexadecimal */
    else {
      hex = 0;                          /* default is decimal */
      if (dooct) {
        /* Don't interpret "0", "08" or "0129" as octal. */
        for (n = 1; VIM_ISDIGIT(ptr[n]); ++n) {
          if (ptr[n] > '7') {
            hex = 0;                    /* can't be octal */
            break;
          }
          if (ptr[n] >= '0')
            hex = '0';                  /* assume octal */
        }
      }
    }
  }

  /*
   * Do the string-to-numeric conversion "manually" to avoid sscanf quirks.
   */
  if (hex == '0' || dooct > 1) {
    /* octal */
    while ('0' <= *ptr && *ptr <= '7') {
      un = 8 * un + (unsigned long)(*ptr - '0');
      ++ptr;
    }
  } else if (hex != 0 || dohex > 1)   {
    /* hex */
    while (vim_isxdigit(*ptr)) {
      un = 16 * un + (unsigned long)hex2nr(*ptr);
      ++ptr;
    }
  } else   {
    /* decimal */
    while (VIM_ISDIGIT(*ptr)) {
      un = 10 * un + (unsigned long)(*ptr - '0');
      ++ptr;
    }
  }

  if (hexp != NULL)
    *hexp = hex;
  if (len != NULL)
    *len = (int)(ptr - start);
  if (nptr != NULL) {
    if (negative)       /* account for leading '-' for decimal numbers */
      *nptr = -(long)un;
    else
      *nptr = (long)un;
  }
  if (unptr != NULL)
    *unptr = un;
}

/*
 * Return the modifier mask bit (MOD_MASK_*) which corresponds to the given
 * modifier name ('S' for Shift, 'C' for Ctrl etc).
 */
int name_to_mod_mask(int c)
{
  int i;

  c = TOUPPER_ASC(c);
  for (i = 0; mod_mask_table[i].mod_mask != 0; i++)
    if (c == mod_mask_table[i].name)
      return mod_mask_table[i].mod_flag;
  return 0;
}

static struct key_name_entry {
  int key;              /* Special key code or ascii value */
  char_u  *name;        /* Name of key */
} key_names_table[] =
{
  {' ',               (char_u *)"Space"},
  {TAB,               (char_u *)"Tab"},
  {K_TAB,             (char_u *)"Tab"},
  {NL,                (char_u *)"NL"},
  {NL,                (char_u *)"NewLine"},     /* Alternative name */
  {NL,                (char_u *)"LineFeed"},    /* Alternative name */
  {NL,                (char_u *)"LF"},          /* Alternative name */
  {CAR,               (char_u *)"CR"},
  {CAR,               (char_u *)"Return"},      /* Alternative name */
  {CAR,               (char_u *)"Enter"},       /* Alternative name */
  {K_BS,              (char_u *)"BS"},
  {K_BS,              (char_u *)"BackSpace"},   /* Alternative name */
  {ESC,               (char_u *)"Esc"},
  {CSI,               (char_u *)"CSI"},
  {K_CSI,             (char_u *)"xCSI"},
  {'|',               (char_u *)"Bar"},
  {'\\',              (char_u *)"Bslash"},
  {K_DEL,             (char_u *)"Del"},
  {K_DEL,             (char_u *)"Delete"},      /* Alternative name */
  {K_KDEL,            (char_u *)"kDel"},
  {K_UP,              (char_u *)"Up"},
  {K_DOWN,            (char_u *)"Down"},
  {K_LEFT,            (char_u *)"Left"},
  {K_RIGHT,           (char_u *)"Right"},
  {K_XUP,             (char_u *)"xUp"},
  {K_XDOWN,           (char_u *)"xDown"},
  {K_XLEFT,           (char_u *)"xLeft"},
  {K_XRIGHT,          (char_u *)"xRight"},

  {K_F1,              (char_u *)"F1"},
  {K_F2,              (char_u *)"F2"},
  {K_F3,              (char_u *)"F3"},
  {K_F4,              (char_u *)"F4"},
  {K_F5,              (char_u *)"F5"},
  {K_F6,              (char_u *)"F6"},
  {K_F7,              (char_u *)"F7"},
  {K_F8,              (char_u *)"F8"},
  {K_F9,              (char_u *)"F9"},
  {K_F10,             (char_u *)"F10"},

  {K_F11,             (char_u *)"F11"},
  {K_F12,             (char_u *)"F12"},
  {K_F13,             (char_u *)"F13"},
  {K_F14,             (char_u *)"F14"},
  {K_F15,             (char_u *)"F15"},
  {K_F16,             (char_u *)"F16"},
  {K_F17,             (char_u *)"F17"},
  {K_F18,             (char_u *)"F18"},
  {K_F19,             (char_u *)"F19"},
  {K_F20,             (char_u *)"F20"},

  {K_F21,             (char_u *)"F21"},
  {K_F22,             (char_u *)"F22"},
  {K_F23,             (char_u *)"F23"},
  {K_F24,             (char_u *)"F24"},
  {K_F25,             (char_u *)"F25"},
  {K_F26,             (char_u *)"F26"},
  {K_F27,             (char_u *)"F27"},
  {K_F28,             (char_u *)"F28"},
  {K_F29,             (char_u *)"F29"},
  {K_F30,             (char_u *)"F30"},

  {K_F31,             (char_u *)"F31"},
  {K_F32,             (char_u *)"F32"},
  {K_F33,             (char_u *)"F33"},
  {K_F34,             (char_u *)"F34"},
  {K_F35,             (char_u *)"F35"},
  {K_F36,             (char_u *)"F36"},
  {K_F37,             (char_u *)"F37"},

  {K_XF1,             (char_u *)"xF1"},
  {K_XF2,             (char_u *)"xF2"},
  {K_XF3,             (char_u *)"xF3"},
  {K_XF4,             (char_u *)"xF4"},

  {K_HELP,            (char_u *)"Help"},
  {K_UNDO,            (char_u *)"Undo"},
  {K_INS,             (char_u *)"Insert"},
  {K_INS,             (char_u *)"Ins"},         /* Alternative name */
  {K_KINS,            (char_u *)"kInsert"},
  {K_HOME,            (char_u *)"Home"},
  {K_KHOME,           (char_u *)"kHome"},
  {K_XHOME,           (char_u *)"xHome"},
  {K_ZHOME,           (char_u *)"zHome"},
  {K_END,             (char_u *)"End"},
  {K_KEND,            (char_u *)"kEnd"},
  {K_XEND,            (char_u *)"xEnd"},
  {K_ZEND,            (char_u *)"zEnd"},
  {K_PAGEUP,          (char_u *)"PageUp"},
  {K_PAGEDOWN,        (char_u *)"PageDown"},
  {K_KPAGEUP,         (char_u *)"kPageUp"},
  {K_KPAGEDOWN,       (char_u *)"kPageDown"},

  {K_KPLUS,           (char_u *)"kPlus"},
  {K_KMINUS,          (char_u *)"kMinus"},
  {K_KDIVIDE,         (char_u *)"kDivide"},
  {K_KMULTIPLY,       (char_u *)"kMultiply"},
  {K_KENTER,          (char_u *)"kEnter"},
  {K_KPOINT,          (char_u *)"kPoint"},

  {K_K0,              (char_u *)"k0"},
  {K_K1,              (char_u *)"k1"},
  {K_K2,              (char_u *)"k2"},
  {K_K3,              (char_u *)"k3"},
  {K_K4,              (char_u *)"k4"},
  {K_K5,              (char_u *)"k5"},
  {K_K6,              (char_u *)"k6"},
  {K_K7,              (char_u *)"k7"},
  {K_K8,              (char_u *)"k8"},
  {K_K9,              (char_u *)"k9"},

  {'<',               (char_u *)"lt"},

  {K_MOUSE,           (char_u *)"Mouse"},
  {K_NETTERM_MOUSE,   (char_u *)"NetMouse"},
  {K_DEC_MOUSE,       (char_u *)"DecMouse"},
#ifdef FEAT_MOUSE_JSB
  {K_JSBTERM_MOUSE,   (char_u *)"JsbMouse"},
#endif
  {K_URXVT_MOUSE,     (char_u *)"UrxvtMouse"},
  {K_SGR_MOUSE,       (char_u *)"SgrMouse"},
  {K_LEFTMOUSE,       (char_u *)"LeftMouse"},
  {K_LEFTMOUSE_NM,    (char_u *)"LeftMouseNM"},
  {K_LEFTDRAG,        (char_u *)"LeftDrag"},
  {K_LEFTRELEASE,     (char_u *)"LeftRelease"},
  {K_LEFTRELEASE_NM,  (char_u *)"LeftReleaseNM"},
  {K_MIDDLEMOUSE,     (char_u *)"MiddleMouse"},
  {K_MIDDLEDRAG,      (char_u *)"MiddleDrag"},
  {K_MIDDLERELEASE,   (char_u *)"MiddleRelease"},
  {K_RIGHTMOUSE,      (char_u *)"RightMouse"},
  {K_RIGHTDRAG,       (char_u *)"RightDrag"},
  {K_RIGHTRELEASE,    (char_u *)"RightRelease"},
  {K_MOUSEDOWN,       (char_u *)"ScrollWheelUp"},
  {K_MOUSEUP,         (char_u *)"ScrollWheelDown"},
  {K_MOUSELEFT,       (char_u *)"ScrollWheelRight"},
  {K_MOUSERIGHT,      (char_u *)"ScrollWheelLeft"},
  {K_MOUSEDOWN,       (char_u *)"MouseDown"},   /* OBSOLETE: Use	  */
  {K_MOUSEUP,         (char_u *)"MouseUp"},     /* ScrollWheelXXX instead */
  {K_X1MOUSE,         (char_u *)"X1Mouse"},
  {K_X1DRAG,          (char_u *)"X1Drag"},
  {K_X1RELEASE,               (char_u *)"X1Release"},
  {K_X2MOUSE,         (char_u *)"X2Mouse"},
  {K_X2DRAG,          (char_u *)"X2Drag"},
  {K_X2RELEASE,               (char_u *)"X2Release"},
  {K_DROP,            (char_u *)"Drop"},
  {K_ZERO,            (char_u *)"Nul"},
  {K_SNR,             (char_u *)"SNR"},
  {K_PLUG,            (char_u *)"Plug"},
  {0,                 NULL}
};

#define KEY_NAMES_TABLE_LEN (sizeof(key_names_table) / \
                             sizeof(struct key_name_entry))

/*
 * Find the special key with the given name (the given string does not have to
 * end with NUL, the name is assumed to end before the first non-idchar).
 * If the name starts with "t_" the next two characters are interpreted as a
 * termcap name.
 * Return the key code, or 0 if not found.
 */
int get_special_key_code(char_u *name)
{
  char_u  *table_name;
  char_u string[3];
  int i, j;

  /*
   * If it's <t_xx> we get the code for xx from the termcap
   */
  if (name[0] == 't' && name[1] == '_' && name[2] != NUL && name[3] != NUL) {
    string[0] = name[2];
    string[1] = name[3];
    string[2] = NUL;
    /*
     * if (add_termcap_entry(string, FALSE) == OK)
     *   return TERMCAP2KEY(name[2], name[3]);
     */
  } else
    for (i = 0; key_names_table[i].name != NULL; i++) {
      table_name = key_names_table[i].name;
      for (j = 0; vim_isIDc(name[j]) && table_name[j] != NUL; j++)
        if (TOLOWER_ASC(table_name[j]) != TOLOWER_ASC(name[j]))
          break;
      if (!vim_isIDc(name[j]) && table_name[j] == NUL)
        return key_names_table[i].key;
    }
  return 0;
}

/*
 * Change <xHome> to <Home>, <xUp> to <Up>, etc.
 */
int handle_x_keys(int key)
{
  switch (key) {
  case K_XUP:     return K_UP;
  case K_XDOWN:   return K_DOWN;
  case K_XLEFT:   return K_LEFT;
  case K_XRIGHT:  return K_RIGHT;
  case K_XHOME:   return K_HOME;
  case K_ZHOME:   return K_HOME;
  case K_XEND:    return K_END;
  case K_ZEND:    return K_END;
  case K_XF1:     return K_F1;
  case K_XF2:     return K_F2;
  case K_XF3:     return K_F3;
  case K_XF4:     return K_F4;
  case K_S_XF1:   return K_S_F1;
  case K_S_XF2:   return K_S_F2;
  case K_S_XF3:   return K_S_F3;
  case K_S_XF4:   return K_S_F4;
  }
  return key;
}
#define MOD_KEYS_ENTRY_SIZE 5

/*
 * Try to include modifiers in the key.
 * Changes "Shift-a" to 'A', "Alt-A" to 0xc0, etc.
 */
int extract_modifiers(int key, int *modp)
{
  int modifiers = *modp;

  if ((modifiers & MOD_MASK_SHIFT) && ASCII_ISALPHA(key)) {
    key = TOUPPER_ASC(key);
    modifiers &= ~MOD_MASK_SHIFT;
  }
  if ((modifiers & MOD_MASK_CTRL)
      && ((key >= '?' && key <= '_') || ASCII_ISALPHA(key))
      ) {
    key = Ctrl_chr(key);
    modifiers &= ~MOD_MASK_CTRL;
    /* <C-@> is <Nul> */
    if (key == 0)
      key = K_ZERO;
  }
  if ((modifiers & MOD_MASK_ALT) && key < 0x80
      && !enc_dbcs                      /* avoid creating a lead byte */
      ) {
    key |= 0x80;
    modifiers &= ~MOD_MASK_ALT;         /* remove the META modifier */
  }

  *modp = modifiers;
  return key;
}

/*
 * Check if if there is a special key code for "key" that includes the
 * modifiers specified.
 */
int simplify_key(int key, int *modifiers)
{
  int i;
  int key0;
  int key1;

  if (*modifiers & (MOD_MASK_SHIFT | MOD_MASK_CTRL | MOD_MASK_ALT)) {
    /* TAB is a special case */
    if (key == TAB && (*modifiers & MOD_MASK_SHIFT)) {
      *modifiers &= ~MOD_MASK_SHIFT;
      return K_S_TAB;
    }
    key0 = KEY2TERMCAP0(key);
    key1 = KEY2TERMCAP1(key);
    for (i = 0; modifier_keys_table[i] != NUL; i += MOD_KEYS_ENTRY_SIZE)
      if (key0 == modifier_keys_table[i + 3]
          && key1 == modifier_keys_table[i + 4]
          && (*modifiers & modifier_keys_table[i])) {
        *modifiers &= ~modifier_keys_table[i];
        return TERMCAP2KEY(modifier_keys_table[i + 1],
            modifier_keys_table[i + 2]);
      }
  }
  return key;
}

/*
 * Try translating a <> name at (*srcp)[], return the key and modifiers.
 * srcp is advanced to after the <> name.
 * returns 0 if there is no match.
 */
int 
find_special_key (
    char_u **srcp,
    int *modp,
    int keycode,                 /* prefer key code, e.g. K_DEL instead of DEL */
    int keep_x_key              /* don't translate xHome to Home key */
)
{
  char_u      *last_dash;
  char_u      *end_of_name;
  char_u      *src;
  char_u      *bp;
  int modifiers;
  int bit;
  int key;
  unsigned long n;
  int l;

  src = *srcp;
  if (src[0] != '<')
    return 0;

  /* Find end of modifier list */
  last_dash = src;
  for (bp = src + 1; *bp == '-' || vim_isIDc(*bp); bp++) {
    if (*bp == '-') {
      last_dash = bp;
      if (bp[1] != NUL) {
        if (has_mbyte)
          l = mb_ptr2len(bp + 1);
        else
          l = 1;
        if (bp[l + 1] == '>')
          bp += l;              /* anything accepted, like <C-?> */
      }
    }
    if (bp[0] == 't' && bp[1] == '_' && bp[2] && bp[3])
      bp += 3;          /* skip t_xx, xx may be '-' or '>' */
    else if (STRNICMP(bp, "char-", 5) == 0) {
      vim_str2nr(bp + 5, NULL, &l, TRUE, TRUE, NULL, NULL);
      bp += l + 5;
      break;
    }
  }

  if (*bp == '>') {     /* found matching '>' */
    end_of_name = bp + 1;

    /* Which modifiers are given? */
    modifiers = 0x0;
    for (bp = src + 1; bp < last_dash; bp++) {
      if (*bp != '-') {
        bit = name_to_mod_mask(*bp);
        if (bit == 0x0)
          break;                /* Illegal modifier name */
        modifiers |= bit;
      }
    }

    /*
     * Legal modifier name.
     */
    if (bp >= last_dash) {
      if (STRNICMP(last_dash + 1, "char-", 5) == 0
          && VIM_ISDIGIT(last_dash[6])) {
        /* <Char-123> or <Char-033> or <Char-0x33> */
        vim_str2nr(last_dash + 6, NULL, NULL, TRUE, TRUE, NULL, &n);
        key = (int)n;
      } else   {
        /*
         * Modifier with single letter, or special key name.
         */
        if (has_mbyte)
          l = mb_ptr2len(last_dash + 1);
        else
          l = 1;
        if (modifiers != 0 && last_dash[l + 1] == '>')
          key = PTR2CHAR(last_dash + 1);
        else {
          key = get_special_key_code(last_dash + 1);
          if (!keep_x_key)
            key = handle_x_keys(key);
        }
      }

      /*
       * get_special_key_code() may return NUL for invalid
       * special key name.
       */
      if (key != NUL) {
        /*
         * Only use a modifier when there is no special key code that
         * includes the modifier.
         */
        key = simplify_key(key, &modifiers);

        if (!keycode) {
          /* don't want keycode, use single byte code */
          if (key == K_BS)
            key = BS;
          else if (key == K_DEL || key == K_KDEL)
            key = DEL;
        }

        /*
         * Normal Key with modifier: Try to make a single byte code.
         */
        if (!IS_SPECIAL(key))
          key = extract_modifiers(key, &modifiers);

        *modp = modifiers;
        *srcp = end_of_name;
        return key;
      }
    }
  }
  return 0;
}

/*
 * Find a termcode with keys 'src' (must be NUL terminated).
 * Return the index in termcodes[], or -1 if not found.
 */
int find_term_bykeys(char_u *src)
{
  int i;
  int slen = (int)STRLEN(src);

  for (i = 0; i < tc_len; ++i) {
    if (slen == termcodes[i].len
        && STRNCMP(termcodes[i].code, src, (size_t)slen) == 0)
      return i;
  }
  return -1;
}

/*
 * Replace any terminal code strings in from[] with the equivalent internal
 * vim representation.	This is used for the "from" and "to" part of a
 * mapping, and the "to" part of a menu command.
 * Any strings like "<C-UP>" are also replaced, unless 'cpoptions' contains
 * '<'.
 * K_SPECIAL by itself is replaced by K_SPECIAL KS_SPECIAL KE_FILLER.
 *
 * The replacement is done in result[] and finally copied into allocated
 * memory. If this all works well *bufp is set to the allocated memory and a
 * pointer to it is returned. If something fails *bufp is set to NULL and from
 * is returned.
 *
 * CTRL-V characters are removed.  When "from_part" is TRUE, a trailing CTRL-V
 * is included, otherwise it is removed (for ":map xx ^V", maps xx to
 * nothing).  When 'cpoptions' does not contain 'B', a backslash can be used
 * instead of a CTRL-V.
 */
char_u *
replace_termcodes (
    char_u *from,
    char_u **bufp,
    int from_part,
    int do_lt,                     /* also translate <lt> */
    int special,                   /* always accept <key> notation */
    int cpo_flags                  /* if true do not use p_cpo option */
)
{
  int i;
  int slen;
  int key;
  int dlen = 0;
  char_u      *src;
  int do_backslash;             /* backslash is a special character */
  int do_special;               /* recognize <> key codes */
  int do_key_code;              /* recognize raw key codes */
  char_u      *result;          /* buffer for resulting string */

  if (cpo_flags) {
    do_backslash = !(cpo_flags&FLAG_CPO_BSLASH);
    do_special = !(cpo_flags&FLAG_CPO_SPECI) || special;
    do_key_code = !(cpo_flags&FLAG_CPO_KEYCODE);
  } else {
    ;
  }

  /*
   * Allocate space for the translation.  Worst case a single character is
   * replaced by 6 bytes (shifted special key), plus a NUL at the end.
   */
  result = alloc((unsigned)STRLEN(from) * 6 + 1);
  if (result == NULL) {         /* out of memory */
    *bufp = NULL;
    return from;
  }

  src = from;

  /*
   * Check for #n at start only: function key n
   */
  if (from_part && src[0] == '#' && VIM_ISDIGIT(src[1])) {  /* function key */
    result[dlen++] = K_SPECIAL;
    result[dlen++] = 'k';
    if (src[1] == '0')
      result[dlen++] = ';';             /* #0 is F10 is "k;" */
    else
      result[dlen++] = src[1];          /* #3 is F3 is "k3" */
    src += 2;
  }

  /*
   * Copy each byte from *from to result[dlen]
   */
  while (*src != NUL) {
    /*
     * If 'cpoptions' does not contain '<', check for special key codes,
     * like "<C-S-LeftMouse>"
     */
    if (do_special && (do_lt || STRNCMP(src, "<lt>", 4) != 0)) {
      /*
       * Replace <SID> by K_SNR <script-nr> _.
       * (room: 5 * 6 = 30 bytes; needed: 3 + <nr> + 1 <= 14)
       */
      if (STRNICMP(src, "<SID>", 5) == 0) {
        if (current_SID <= 0)
          ; // EMSG(_(e_usingsid));
        else {
          src += 5;
          result[dlen++] = K_SPECIAL;
          result[dlen++] = (int)KS_EXTRA;
          result[dlen++] = (int)KE_SNR;
          sprintf((char *)result + dlen, "%ld", (long)current_SID);
          dlen += (int)STRLEN(result + dlen);
          result[dlen++] = '_';
          continue;
        }
      }

      slen = trans_special(&src, result + dlen, TRUE);
      if (slen) {
        dlen += slen;
        continue;
      }
    }

    /*
     * If 'cpoptions' does not contain 'k', see if it's an actual key-code.
     * Note that this is also checked after replacing the <> form.
     * Single character codes are NOT replaced (e.g. ^H or DEL), because
     * it could be a character in the file.
     */
    if (do_key_code) {
      i = find_term_bykeys(src);
      if (i >= 0) {
        result[dlen++] = K_SPECIAL;
        result[dlen++] = termcodes[i].name[0];
        result[dlen++] = termcodes[i].name[1];
        src += termcodes[i].len;
        /* If terminal code matched, continue after it. */
        continue;
      }
    }

    if (do_special) {
      char_u      *p, *s, len;

      /*
       * Replace <Leader> by the value of "mapleader".
       * Replace <LocalLeader> by the value of "maplocalleader".
       * If "mapleader" or "maplocalleader" isn't set use a backslash.
       */
      if (STRNICMP(src, "<Leader>", 8) == 0) {
        len = 8;
        /* p = get_var_value((char_u *)"g:mapleader"); */
      } else if (STRNICMP(src, "<LocalLeader>", 13) == 0)   {
        len = 13;
        /* p = get_var_value((char_u *)"g:maplocalleader"); */
      } else   {
        len = 0;
        p = NULL;
      }
      if (len != 0) {
        /* Allow up to 8 * 6 characters for "mapleader". */
        if (p == NULL || *p == NUL || STRLEN(p) > 8 * 6)
          s = (char_u *)"\\";
        else
          s = p;
        while (*s != NUL)
          result[dlen++] = *s++;
        src += len;
        continue;
      }
    }

    /*
     * Remove CTRL-V and ignore the next character.
     * For "from" side the CTRL-V at the end is included, for the "to"
     * part it is removed.
     * If 'cpoptions' does not contain 'B', also accept a backslash.
     */
    key = *src;
    if (key == Ctrl_V || (do_backslash && key == '\\')) {
      ++src;                                    /* skip CTRL-V or backslash */
      if (*src == NUL) {
        if (from_part)
          result[dlen++] = key;
        break;
      }
    }

    /* skip multibyte char correctly */
    for (i = (*mb_ptr2len)(src); i > 0; --i) {
      /*
       * If the character is K_SPECIAL, replace it with K_SPECIAL
       * KS_SPECIAL KE_FILLER.
       * If compiled with the GUI replace CSI with K_CSI.
       */
      if (*src == K_SPECIAL) {
        result[dlen++] = K_SPECIAL;
        result[dlen++] = KS_SPECIAL;
        result[dlen++] = KE_FILLER;
      } else
        result[dlen++] = *src;
      ++src;
    }
  }
  result[dlen] = NUL;

  /*
   * Copy the new string to allocated memory.
   * If this fails, just return from.
   */
  if ((*bufp = vim_strsave(result)) != NULL)
    from = *bufp;
  vim_free(result);
  return from;
}
/*
 * Compare two strings, ignoring case, using current locale.
 * Doesn't work for multi-byte characters.
 * return 0 for match, < 0 for smaller, > 0 for bigger
 */
int vim_stricmp(char *s1, char *s2)
{
  int i;

  for (;; ) {
    i = (int)TOLOWER_LOC(*s1) - (int)TOLOWER_LOC(*s2);
    if (i != 0)
      return i;                             /* this character different */
    if (*s1 == NUL)
      break;                                /* strings match until NUL */
    ++s1;
    ++s2;
  }
  return 0;                                 /* strings match */
}

/*
 * Convert Unicode character "c" to UTF-8 string in "buf[]".
 * Returns the number of bytes.
 * This does not include composing characters.
 */
int utf_char2bytes(int c, char_u *buf)
{
  if (c < 0x80) {               /* 7 bits */
    buf[0] = c;
    return 1;
  }
  if (c < 0x800) {              /* 11 bits */
    buf[0] = 0xc0 + ((unsigned)c >> 6);
    buf[1] = 0x80 + (c & 0x3f);
    return 2;
  }
  if (c < 0x10000) {            /* 16 bits */
    buf[0] = 0xe0 + ((unsigned)c >> 12);
    buf[1] = 0x80 + (((unsigned)c >> 6) & 0x3f);
    buf[2] = 0x80 + (c & 0x3f);
    return 3;
  }
  if (c < 0x200000) {           /* 21 bits */
    buf[0] = 0xf0 + ((unsigned)c >> 18);
    buf[1] = 0x80 + (((unsigned)c >> 12) & 0x3f);
    buf[2] = 0x80 + (((unsigned)c >> 6) & 0x3f);
    buf[3] = 0x80 + (c & 0x3f);
    return 4;
  }
  if (c < 0x4000000) {          /* 26 bits */
    buf[0] = 0xf8 + ((unsigned)c >> 24);
    buf[1] = 0x80 + (((unsigned)c >> 18) & 0x3f);
    buf[2] = 0x80 + (((unsigned)c >> 12) & 0x3f);
    buf[3] = 0x80 + (((unsigned)c >> 6) & 0x3f);
    buf[4] = 0x80 + (c & 0x3f);
    return 5;
  }
  /* 31 bits */
  buf[0] = 0xfc + ((unsigned)c >> 30);
  buf[1] = 0x80 + (((unsigned)c >> 24) & 0x3f);
  buf[2] = 0x80 + (((unsigned)c >> 18) & 0x3f);
  buf[3] = 0x80 + (((unsigned)c >> 12) & 0x3f);
  buf[4] = 0x80 + (((unsigned)c >> 6) & 0x3f);
  buf[5] = 0x80 + (c & 0x3f);
  return 6;
}

/*
 * Add character "c" to buffer "s".  Escape the special meaning of K_SPECIAL
 * and CSI.  Handle multi-byte characters.
 * Returns a pointer to after the added bytes.
 */
char_u *add_char2buf(int c, char_u *s)
{
  char_u temp[MB_MAXBYTES + 1];
  int i;
  int len;

  len = (*mb_char2bytes)(c, temp);
  for (i = 0; i < len; ++i) {
    c = temp[i];
    /* Need to escape K_SPECIAL and CSI like in the typeahead buffer. */
    if (c == K_SPECIAL) {
      *s++ = K_SPECIAL;
      *s++ = KS_SPECIAL;
      *s++ = KE_FILLER;
    } else
      *s++ = c;
  }
  return s;
}

/*
 * Try translating a <> name at (*srcp)[] to dst[].
 * Return the number of characters added to dst[], zero for no match.
 * If there is a match, srcp is advanced to after the <> name.
 * dst[] must be big enough to hold the result (up to six characters)!
 */
int 
trans_special (
    char_u **srcp,
    char_u *dst,
    int keycode             /* prefer key code, e.g. K_DEL instead of DEL */
)
{
  int modifiers = 0;
  int key;
  int dlen = 0;

  key = find_special_key(srcp, &modifiers, keycode, FALSE);
  if (key == 0)
    return 0;

  /* Put the appropriate modifier in a string */
  if (modifiers != 0) {
    dst[dlen++] = K_SPECIAL;
    dst[dlen++] = KS_MODIFIER;
    dst[dlen++] = modifiers;
  }

  if (IS_SPECIAL(key)) {
    dst[dlen++] = K_SPECIAL;
    dst[dlen++] = KEY2TERMCAP0(key);
    dst[dlen++] = KEY2TERMCAP1(key);
  } else if (has_mbyte && !keycode)
    dlen += (*mb_char2bytes)(key, dst + dlen);
  else if (keycode)
    dlen = (int)(add_char2buf(key, dst + dlen) - dst);
  else
    dst[dlen++] = key;

  return dlen;
}

/*
 * left-right swap the characters in buf[len].
 */
static void lrswapbuf(char_u *buf, int len)
{
  char_u *s, *e;
  int c;

  s = buf;
  e = buf + len - 1;
  while (e > s) {
    c = *s;
    *s = *e;
    *e = c;
    ++s;
    --e;
  }
}

/*
 * swap all the characters in reverse direction
 */
char_u* lrswap(char_u *ibuf)
{
  if ((ibuf != NULL) && (*ibuf != NUL)) {
    lrswapbuf(ibuf, (int)STRLEN(ibuf));
  }
  return ibuf;
}

// Catch 22: chartab[] can't be initialized before the options are
// initialized, and initializing options may cause transchar() to be called!
// When chartab_initialized == FALSE don't use chartab[].
// Does NOT work for multi-byte characters, c must be <= 255.
// Also doesn't work for the first byte of a multi-byte, "c" must be a
// character!
static char_u transchar_buf[7];

/// Translates a character
///
/// @param c
///
/// @return translated character.
char_u* transchar(int c)
{
  int i = 0;
  if (IS_SPECIAL(c)) {
    // special key code, display as ~@ char
    transchar_buf[0] = '~';
    transchar_buf[1] = '@';
    i = 2;
    c = K_SECOND(c);
  }

  if ((!chartab_initialized && (((c >= ' ') && (c <= '~')) || F_ischar(c)))
      || ((c < 256) && vim_isprintc_strict(c))) {
    // printable character
    transchar_buf[i] = c;
    transchar_buf[i + 1] = NUL;
  } else {
    transchar_nonprint(transchar_buf + i, c);
  }
  return transchar_buf;
}

/// return TRUE if 'c' is a printable character
/// Assume characters above 0x100 are printable (multi-byte), except for
/// Unicode.
///
/// @param c
///
/// @return TRUE if 'c' a printable character.
int vim_isprintc(int c)
{
  if (enc_utf8 && (c >= 0x100)) {
    return utf_printable(c);
  }
  return c >= 0x100 || (c > 0 && (chartab[c] & CT_PRINT_CHAR));
}

/*
 * Return a string which contains the name of the given key when the given
 * modifiers are down.
 */
char_u *get_special_key_name(int c, int modifiers)
{
  static char_u string[MAX_KEY_NAME_LEN + 1];

  int i, idx;
  int table_idx;
  char_u  *s;

  string[0] = '<';
  idx = 1;

  /* Key that stands for a normal character. */
  if (IS_SPECIAL(c) && KEY2TERMCAP0(c) == KS_KEY)
    c = KEY2TERMCAP1(c);

  /*
   * Translate shifted special keys into unshifted keys and set modifier.
   * Same for CTRL and ALT modifiers.
   */
  if (IS_SPECIAL(c)) {
    for (i = 0; modifier_keys_table[i] != 0; i += MOD_KEYS_ENTRY_SIZE)
      if (       KEY2TERMCAP0(c) == (int)modifier_keys_table[i + 1]
                 && (int)KEY2TERMCAP1(c) == (int)modifier_keys_table[i + 2]) {
        modifiers |= modifier_keys_table[i];
        c = TERMCAP2KEY(modifier_keys_table[i + 3],
            modifier_keys_table[i + 4]);
        break;
      }
  }

  /* try to find the key in the special key table */
  table_idx = find_special_key_in_table(c);

  /*
   * When not a known special key, and not a printable character, try to
   * extract modifiers.
   */
  if (c > 0
      && (*mb_char2len)(c) == 1
      ) {
    if (table_idx < 0
        && (!vim_isprintc(c) || (c & 0x7f) == ' ')
        && (c & 0x80)) {
      c &= 0x7f;
      modifiers |= MOD_MASK_ALT;
      /* try again, to find the un-alted key in the special key table */
      table_idx = find_special_key_in_table(c);
    }
    if (table_idx < 0 && !vim_isprintc(c) && c < ' ') {
      c += '@';
      modifiers |= MOD_MASK_CTRL;
    }
  }

  /* translate the modifier into a string */
  for (i = 0; mod_mask_table[i].name != 'A'; i++)
    if ((modifiers & mod_mask_table[i].mod_mask)
        == mod_mask_table[i].mod_flag) {
      string[idx++] = mod_mask_table[i].name;
      string[idx++] = (char_u)'-';
    }

  if (table_idx < 0) {          /* unknown special key, may output t_xx */
    if (IS_SPECIAL(c)) {
      string[idx++] = 't';
      string[idx++] = '_';
      string[idx++] = KEY2TERMCAP0(c);
      string[idx++] = KEY2TERMCAP1(c);
    }
    /* Not a special key, only modifiers, output directly */
    else {
      if (has_mbyte && (*mb_char2len)(c) > 1)
        idx += (*mb_char2bytes)(c, string + idx);
      else if (vim_isprintc(c))
        string[idx++] = c;
      else {
        s = transchar(c);
        while (*s)
          string[idx++] = *s++;
      }
    }
  } else   {            /* use name of special key */
    STRCPY(string + idx, key_names_table[table_idx].name);
    idx = (int)STRLEN(string);
  }
  string[idx++] = '>';
  string[idx] = NUL;
  return string;
}

/*
 * Translate an internal mapping/abbreviation representation into the
 * corresponding external one recognized by :map/:abbrev commands;
 * respects the current B/k/< settings of 'cpoption'.
 *
 * This function is called when expanding mappings/abbreviations on the
 * command-line, and for building the "Ambiguous mapping..." error message.
 *
 * It uses a growarray to build the translation string since the
 * latter can be wider than the original description. The caller has to
 * free the string afterwards.
 *
 * Returns NULL when there is a problem.
 */
char_u *
translate_mapping (
    char_u *str,
    int expmap,             /* TRUE when expanding mappings on command-line */
    int cpo_flags           /* if true do not use p_cpo option */
)
{
  garray_T ga;
  int c;
  int modifiers;
  int cpo_bslash;
  int cpo_special;
  int cpo_keycode;

  ga_init(&ga);
  ga.ga_itemsize = 1;
  ga.ga_growsize = 40;

  cpo_bslash = !(cpo_flags&FLAG_CPO_BSLASH);
  cpo_special = !(cpo_flags&FLAG_CPO_SPECI);
  cpo_keycode = !(cpo_flags&FLAG_CPO_KEYCODE);

  for (; *str; ++str) {
    c = *str;
    if (c == K_SPECIAL && str[1] != NUL && str[2] != NUL) {
      modifiers = 0;
      if (str[1] == KS_MODIFIER) {
        str++;
        modifiers = *++str;
        c = *++str;
      }
      if (cpo_special && cpo_keycode && c == K_SPECIAL && !modifiers) {
        int i;

        /* try to find special key in termcodes */
        for (i = 0; i < tc_len; ++i)
          if (termcodes[i].name[0] == str[1]
              && termcodes[i].name[1] == str[2])
            break;
        if (i < tc_len) {
          ga_concat(&ga, termcodes[i].code);
          str += 2;
          continue;           /* for (str) */
        }
      }
      if (c == K_SPECIAL && str[1] != NUL && str[2] != NUL) {
        if (expmap && cpo_special) {
          ga_clear(&ga);
          return NULL;
        }
        c = TO_SPECIAL(str[1], str[2]);
        if (c == K_ZERO)                /* display <Nul> as ^@ */
          c = NUL;
        str += 2;
      }
      if (IS_SPECIAL(c) || modifiers) {         /* special key */
        if (expmap && cpo_special) {
          ga_clear(&ga);
          return NULL;
        }
        ga_concat(&ga, get_special_key_name(c, modifiers));
        continue;         /* for (str) */
      }
    }
    if (c == ' ' || c == '\t' || c == Ctrl_J || c == Ctrl_V
        || (c == '<' && !cpo_special) || (c == '\\' && !cpo_bslash))
      ga_append(&ga, cpo_bslash ? Ctrl_V : '\\');
    if (c)
      ga_append(&ga, c);
  }
  ga_append(&ga, NUL);
  return (char_u *)(ga.ga_data);
}

#endif
int abc2() {
  return 0;
}
