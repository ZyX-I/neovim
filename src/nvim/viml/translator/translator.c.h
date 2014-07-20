#ifndef NVIM_VIML_TRANSLATOR_TRANSLATOR_C_H
#define __STDC_LIMIT_MACROS
#define __STDC_FORMAT_MACROS
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#undef __STDC_LIMIT_MACROS
#undef __STDC_FORMAT_MACROS

#include "nvim/types.h"
#include "nvim/vim.h"
#include "nvim/mbyte.h"
#include "nvim/charset.h"
#include "nvim/keymap.h"
#include "nvim/ascii.h"

#include "nvim/viml/translator/translator.h"
#include "nvim/viml/parser/expressions.h"
#include "nvim/viml/parser/ex_commands.h"
#include "nvim/viml/parser/command_definitions.h"
#include "nvim/viml/dumpers/dumpers.h"

#if !defined(CH_MACROS_DEFINE_LENGTH) && !defined(CH_MACROS_DEFINE_FWRITE)
# define CH_MACROS_DEFINE_LENGTH
# include "nvim/viml/translator/translator.c.h"
# undef CH_MACROS_DEFINE_LENGTH
#elif !defined(CH_MACROS_DEFINE_FWRITE)
# undef CH_MACROS_DEFINE_LENGTH
# define CH_MACROS_DEFINE_FWRITE
# include "nvim/viml/translator/translator.c.h"
# undef CH_MACROS_DEFINE_FWRITE
# define CH_MACROS_DEFINE_LENGTH
#endif
#define NVIM_VIML_TRANSLATOR_TRANSLATOR_C_H

#ifndef NVIM_VIML_DUMPERS_CH_MACROS
# define CH_MACROS_OPTIONS_TYPE const TranslationOptions
# define CH_MACROS_INDENT_STR "  "
#endif
#include "nvim/viml/dumpers/ch_macros.h"

#ifndef NVIM_VIML_TRANSLATOR_TRANSLATOR_C_H_MACROS
#define NVIM_VIML_TRANSLATOR_TRANSLATOR_C_H_MACROS

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

// XXX -1 removes space for trailing zero. Note: STRINGIFY(INTMAX_MAX) may 
//     return something like "(dddddL)": three or more characters wider then 
//     actually needed.
//     +1 is for minus sign.

/// Length of a buffer capable of holding decimal intmax_t representation
///
/// @note Size of the buffer may be actually a few characters off compared to 
///       minimum required size.
#define MAXNUMBUFLEN (sizeof(STRINGIFY(INTMAX_MAX)) - 1 + 1)

/// Length of a buffer capable of holding decimal size_t representation
///
/// @note Size of the buffer may be actually a few characters off compared to 
///       minimum required size.
#define SIZETBUFLEN  (sizeof(STRINGIFY(SIZE_MAX)) - 1)

/// Same as CALL, but with different translation options
///
/// Translation options used define translated function as being :function 
/// definition contents
#define F_FUNC(f, ...) \
    do { \
      TranslationOptions o = kTransFunc; \
      F(f, __VA_ARGS__); \
    } while (0)

/// Same as CALL, but with different translation options
///
/// Translation options used define translated function as being .vim file 
/// contents
#define F_SCRIPT(f, ...) \
    do { \
      TranslationOptions o = kTransScript; \
      F(f, __VA_ARGS__); \
    } while (0)

/// Same as W_EXPR_POS, but written string is escaped
#define W_EXPR_POS_ESCAPED(expr) \
    F(dump_string_length, expr->position, \
         expr->end_position - expr->position + 1)

/// Get translation context
///
/// See documentation for TranslationOptions for more details.
#define TRANSLATION_CONTEXT o


#define VIM_ZERO  "0"
#define VIM_FALSE "0"
#define VIM_TRUE  "1"
#define VIM_EMPTY_STRING "''"

typedef struct {
  const CommandNode *node;
  const size_t indent;
} TranslateFuncArgs;

typedef struct {
  size_t idx;
  char *var;
} LetListItemAssArgs;

typedef enum {
  kOptDefault = 0,
  kOptLocal,
  kOptGlobal,
} OptionType;

FDEC_TYPEDEF_ALL(AssignmentValueDump, const void *const);

#define trans_special(a, b, c, d) \
    trans_special((const char_u **) a, b, (char_u *) c, d)
#define get_option_properties(a, ...) \
    get_option_properties((const char_u *) a, __VA_ARGS__)
#define mb_ptr2len(s) mb_ptr2len((char_u *) s)
#define mb_char2bytes(n, b) mb_char2bytes(n, (char_u *) b)

#endif  // NVIM_VIML_TRANSLATOR_TRANSLATOR_C_H_MACROS

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "viml/translator/translator.c.h.generated.h"
#endif

/// Dump one character
///
/// @param[in]  c  Dumped character
static FDEC(dump_char, uint8_t c)
{
  FUNCTION_START;
  switch (c) {
#define CHAR(c, s) \
    case c: { \
      WS("\\" s); \
      break; \
    }
    CHAR(NUL,  "000")
    CHAR(1,    "001")
    CHAR(2,    "002")
    CHAR(3,    "003")
    CHAR(4,    "004")
    CHAR(5,    "005")
    CHAR(6,    "006")
    CHAR(BELL, "a")
    CHAR(BS,   "b")
    CHAR(TAB,  "t")
    CHAR(NL,   "n")
    CHAR(11,   "011")
    CHAR(FF,   "f")
    CHAR(CAR,  "r")
    CHAR(14,   "014")
    CHAR(15,   "015")
    CHAR(16,   "016")
    CHAR(17,   "017")
    CHAR(18,   "018")
    CHAR(19,   "019")
    CHAR(20,   "020")
    CHAR(21,   "021")
    CHAR(22,   "022")
    CHAR(23,   "023")
    CHAR(24,   "024")
    CHAR(25,   "025")
    CHAR(26,   "026")
    CHAR(ESC,  "027")
    CHAR(28,   "028")
    CHAR(29,   "029")
    CHAR(30,   "030")
    CHAR(31,   "031")
    CHAR('"',  "\"")
    CHAR('\'', "'")
    CHAR('\\', "\\")
    CHAR(127,   "127")
    CHAR(128,   "128")
    CHAR(129,   "129")
    CHAR(130,   "130")
    CHAR(131,   "131")
    CHAR(132,   "132")
    CHAR(133,   "133")
    CHAR(134,   "134")
    CHAR(135,   "135")
    CHAR(136,   "136")
    CHAR(137,   "137")
    CHAR(138,   "138")
    CHAR(139,   "139")
    CHAR(140,   "140")
    CHAR(141,   "141")
    CHAR(142,   "142")
    CHAR(143,   "143")
    CHAR(144,   "144")
    CHAR(145,   "145")
    CHAR(146,   "146")
    CHAR(147,   "147")
    CHAR(148,   "148")
    CHAR(149,   "149")
    CHAR(150,   "150")
    CHAR(151,   "151")
    CHAR(152,   "152")
    CHAR(153,   "153")
    CHAR(154,   "154")
    CHAR(155,   "155")
    CHAR(156,   "156")
    CHAR(157,   "157")
    CHAR(158,   "158")
    CHAR(159,   "159")
    CHAR(160,   "160")
    CHAR(161,   "161")
    CHAR(162,   "162")
    CHAR(163,   "163")
    CHAR(164,   "164")
    CHAR(165,   "165")
    CHAR(166,   "166")
    CHAR(167,   "167")
    CHAR(168,   "168")
    CHAR(169,   "169")
    CHAR(170,   "170")
    CHAR(171,   "171")
    CHAR(172,   "172")
    CHAR(173,   "173")
    CHAR(174,   "174")
    CHAR(175,   "175")
    CHAR(176,   "176")
    CHAR(177,   "177")
    CHAR(178,   "178")
    CHAR(179,   "179")
    CHAR(180,   "180")
    CHAR(181,   "181")
    CHAR(182,   "182")
    CHAR(183,   "183")
    CHAR(184,   "184")
    CHAR(185,   "185")
    CHAR(186,   "186")
    CHAR(187,   "187")
    CHAR(188,   "188")
    CHAR(189,   "189")
    CHAR(190,   "190")
    CHAR(191,   "191")
    CHAR(192,   "192")
    CHAR(193,   "193")
    CHAR(194,   "194")
    CHAR(195,   "195")
    CHAR(196,   "196")
    CHAR(197,   "197")
    CHAR(198,   "198")
    CHAR(199,   "199")
    CHAR(200,   "200")
    CHAR(201,   "201")
    CHAR(202,   "202")
    CHAR(203,   "203")
    CHAR(204,   "204")
    CHAR(205,   "205")
    CHAR(206,   "206")
    CHAR(207,   "207")
    CHAR(208,   "208")
    CHAR(209,   "209")
    CHAR(210,   "210")
    CHAR(211,   "211")
    CHAR(212,   "212")
    CHAR(213,   "213")
    CHAR(214,   "214")
    CHAR(215,   "215")
    CHAR(216,   "216")
    CHAR(217,   "217")
    CHAR(218,   "218")
    CHAR(219,   "219")
    CHAR(220,   "220")
    CHAR(221,   "221")
    CHAR(222,   "222")
    CHAR(223,   "223")
    CHAR(224,   "224")
    CHAR(225,   "225")
    CHAR(226,   "226")
    CHAR(227,   "227")
    CHAR(228,   "228")
    CHAR(229,   "229")
    CHAR(230,   "230")
    CHAR(231,   "231")
    CHAR(232,   "232")
    CHAR(233,   "233")
    CHAR(234,   "234")
    CHAR(235,   "235")
    CHAR(236,   "236")
    CHAR(237,   "237")
    CHAR(238,   "238")
    CHAR(239,   "239")
    CHAR(240,   "240")
    CHAR(241,   "241")
    CHAR(242,   "242")
    CHAR(243,   "243")
    CHAR(244,   "244")
    CHAR(245,   "245")
    CHAR(246,   "246")
    CHAR(247,   "247")
    CHAR(248,   "248")
    CHAR(249,   "249")
    CHAR(250,   "250")
    CHAR(251,   "251")
    CHAR(252,   "252")
    CHAR(253,   "253")
    CHAR(254,   "254")
    CHAR(255,   "255")
#undef CHAR
    default: {
      W_LEN(&c, 1);
      break;
    }
  }
  FUNCTION_END;
}

/// Dump string that is not a vim String
///
/// Use translate_string to dump vim String (kExpr*String)
///
/// @param[in]  s     String that will be written.
/// @param[in]  size  Length of this string.
static FDEC(dump_string_length, const char *s, size_t size)
{
  FUNCTION_START;
  const char *const e = s + size;
  WS("'");
  for (; s < e; s++) {
    size_t charlen = mb_ptr2len(s);
    if (charlen == 1) {
      F(dump_char, *s);;
    } else {
      W_LEN(s, charlen);;
      s += charlen - 1;
    }
  }
  WS("'");
  FUNCTION_END;
}

/// Dump string that is not a vim String
///
/// Use translate_string to dump vim String (kExpr*String)
///
/// @param[in]  s  NUL-terminated string that will be written.
static FDEC(dump_string, const char *const s)
{
  FUNCTION_START;
  F(dump_string_length, s, STRLEN(s));
  FUNCTION_END;
}

/// Dump boolean value
///
/// Writes true or false depending on its argument
///
/// @param[in]  b  Checked value.
static FDEC(dump_bool, bool b)
{
  FUNCTION_START;
  if (b) {
    WS("true");
  } else {
    WS("false");
  }

  FUNCTION_END;
}

/// Dump regular expression
///
/// @param[in]  regex  Regular expression that will be dumped.
static FDEC(translate_regex, const Regex *const regex)
{
  FUNCTION_START;
  F(dump_string, regex->string);
  FUNCTION_END;
}

/// Dump address followup
///
/// @param[in]  followup  Address followup that will be dumped.
static FDEC(translate_address_followup, const AddressFollowup *const followup)
{
  FUNCTION_START;
  // FIXME Replace magic numbers with constants
  switch (followup->type) {
    case kAddressFollowupShift: {
      WS("0, ");
      F_NOOPT(dump_number, (intmax_t) followup->data.shift);
      break;
    }
    case kAddressFollowupForwardPattern: {
      WS("1, ");
      F(dump_string, followup->data.regex->string);
      break;
    }
    case kAddressFollowupBackwardPattern: {
      WS("2, ");
      F(dump_string, followup->data.regex->string);
      break;
    }
    case kAddressFollowupMissing: {
      assert(false);
    }
  }
  FUNCTION_END;
}

/// Dump Ex command range
///
/// @param[in]  range  Range that will be dumped.
static FDEC(translate_range, const Range *const range)
{
  FUNCTION_START;
  const Range *current_range = range;

  if (current_range->address.type == kAddrMissing) {
    WS("nil");
    FUNCTION_END;
  }

  WS("vim.range.compose(state, ");

  for (;;) {
    AddressFollowup *current_followup = current_range->address.followups;
    size_t followup_number = 0;

    assert(current_range->address.type != kAddrMissing);

    for (;;) {
      WS("vim.range.apply_followup(state, ");
      F(translate_address_followup, current_followup);
      followup_number++;
      current_followup = current_followup->next;
      if (current_followup == NULL) {
        break;
      } else {
        WS(", ");
      }
    }

    switch (current_range->address.type) {
      case kAddrFixed: {
        F_NOOPT(dump_unumber, (uintmax_t) current_range->address.data.lnr);;
        break;
      }
      case kAddrEnd: {
        WS("vim.range.last(state)");
        break;
      }
      case kAddrCurrent: {
        WS("vim.range.current(state)");
        break;
      }
      case kAddrMark: {
        WS("vim.range.mark(state, '");
        WC(current_range->address.data.mark);
        WS("')");
        break;
      }
      case kAddrForwardSearch: {
        WS("vim.range.forward_search(state, ");
        F(translate_regex, current_range->address.data.regex);
        WS(")");
        break;
      }
      case kAddrBackwardSearch: {
        WS("vim.range.backward_search(state, ");
        F(translate_regex, current_range->address.data.regex);
        WS(")");
        break;
      }
      case kAddrSubstituteSearch: {
        WS("vim.range.substitute_search(state)");
        break;
      }
      case kAddrForwardPreviousSearch: {
        WS("vim.range.forward_previous_search(state)");
        break;
      }
      case kAddrBackwardPreviousSearch: {
        WS("vim.range.backward_previous_search(state)");
        break;
      }
      case kAddrMissing: {
        assert(false);
      }
    }

    while (followup_number--) {
      WS(")");
    }
    WS(", ");

    F(dump_bool, current_range->setpos);

    current_range = current_range->next;
    if (current_range == NULL) {
      break;
    } else {
      WS(", ");
    }
  }

  WS(")");
  FUNCTION_END;
}

/// Dump Ex command flags
///
/// @param[in]  exflags  Flags to dump.
static FDEC(translate_ex_flags, uint_least8_t exflags)
{
  FUNCTION_START;
  WS("{");
  if (exflags & FLAG_EX_LIST) {
    WS("list=true, ");
  }
  if (exflags & FLAG_EX_LNR) {
    WS("lnr=true, ");
  }
  if (exflags & FLAG_EX_PRINT) {
    WS("print=true, ");
  }
  WS("}");
  FUNCTION_END;
}

/// Dump VimL integer number
///
/// @param[in]  type  Type of the number node being dumped.
/// @param[in]  s     Pointer to first character in dumped number.
/// @param[in]  e     Pointer to last character in dumped number.
static FDEC(translate_number, ExpressionType type, const char *s,
                              const char *const e)
{
  FUNCTION_START;
  switch (type) {
    case kExprHexNumber:
    case kExprDecimalNumber: {
      W_END(s, e);
      break;
    }
    case kExprOctalNumber: {
#define MAXOCTALNUMBUFLEN (sizeof(intmax_t) * 8 / 3 + 1)
#ifdef CH_MACROS_DEFINE_LENGTH
      return MAXOCTALNUMBUFLEN + 1;
#else
      uintmax_t unumber;
      char num_s[MAXOCTALNUMBUFLEN + 1];

      // Ignore leading zeroes
      for (; *s == '0' && s < e; s++) {
      }

      if (((e - s) + 1) > (ptrdiff_t) MAXOCTALNUMBUFLEN) {
        // Integer overflow is an undefined behavior, but we do not emit any 
        // errors for overflow in other arguments, so just write zero
        memcpy(num_s, "0", 2);
      } else {
        memcpy(num_s, s, (e - s) + 1);
        num_s[e - s + 1] = NUL;
      }
      if (sscanf(num_s, "%" PRIoMAX, &unumber) != 1) {
        // TODO: check errno
        // TODO: check %n output?
        // TODO: give error message
#ifdef CH_MACROS_DEFINE_FWRITE
        return FAIL;
#else
        return;
#endif
      }
      F_NOOPT(dump_unumber, unumber);
      break;
#endif
#undef MAXOCTALNUMBUFLEN
    }
    default: {
      assert(false);
    }
  }
  FUNCTION_END;
}

/// Dump VimL string
///
/// @param[in]  type  Type of the string node being dumped.
/// @param[in]  s     Pointer to first character in dumped string.
/// @param[in]  e     Pointer to last character in dumped string.
static FDEC(translate_string, ExpressionType type, const char *const s,
                              const char *const e)
{
  FUNCTION_START;
  bool can_dump_as_is = true;
  switch (type) {
    case kExprSingleQuotedString: {
      for (const char *curp = s + 1; curp < e; curp++) {
        if (*curp == '\'' || *curp < 0x20 || *curp == '\\') {
          can_dump_as_is = false;
          break;
        }
      }
      if (can_dump_as_is) {
        W_END(s, e);
      } else {
        assert(e > s);

        WS("'");
        for (const char *curp = s + 1; curp < e; curp++) {
          switch (*curp) {
            case '\'': {
              WS("\\'");
              curp++;
              break;
            }
            default: {
              F(dump_char, *curp);
              break;
            }
          }
        }
        WS("'");
        break;
      }
      break;
    }
    case kExprDoubleQuotedString: {
      for (const char *curp = s + 1; curp < e; curp++) {
        if (*curp < 0x20) {
          can_dump_as_is = false;
          break;
        }
        if (*curp == '\\') {
          curp++;
          switch (*curp) {
            case 'r':
            case 'n':
            case 'f':
            case 'b':
            case '\\':
            case '\"':
            // Escaping single quote, "[" and "]" result in the escaped 
            // character both in lua and in VimL.
            case '\'':
            case '[':
            case ']': {
              break;
            }
            default: {
              can_dump_as_is = false;
              break;
            }
          }
          if (!can_dump_as_is) {
            break;
          }
        }
      }
      if (can_dump_as_is) {
        W_END(s, e);
      } else {
        assert(e > s);

        WS("\"");
        for (const char *curp = s + 1; curp < e; curp++) {
          switch (*curp) {
            case '\\': {
              curp++;
              switch (*curp) {
                case 'r':
                case 'n':
                case 'f':
                case 'b':
                case '\\':
                case '\"': {
                  W_LEN(curp - 1, 2);
                  break;
                }
                case 'e': {
                  WS("\\027");
                  break;
                }
                case 'x':
                case 'X':
                case 'u':
                case 'U': {
                  if (vim_isxdigit(curp[1])) {
                    int8_t n;
                    int nr = 0;
                    bool isx = (*curp == 'X' || *curp == 'x');

                    if (isx) {
                      n = 2;
                    } else {
                      n = 4;
                    }

                    while (--n >= 0 && vim_isxdigit(curp[1])) {
                      curp++;
                      nr = (nr << 4) + hex2nr(*curp);
                    }
                    if (isx || nr < 0x7F) {
                      F(dump_char, nr);
                    } else {
                      char buf[MAX_CHAR_LEN];
                      size_t size;

                      size = mb_char2bytes(nr, buf);
                      W_LEN(buf, size);
                    }
                  } else {
                    W_LEN(curp, 1);
                  }
                  break;
                }
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7': {
                  char c;
                  c = *curp - '0';
                  if ('0' <= curp[1] && curp[1] <= '7') {
                    curp++;
                    c = (c << 3) + (*curp - '0');
                    if ('0' <= curp[1] && curp[1] <= '7') {
                      curp++;
                      c = (c << 3) + (*curp - '0');
                    }
                  }
                  F(dump_char, c);
                  break;
                }
                case '<': {
                  char buf[MAX_CHAR_LEN * 6];
                  size_t size;

                  size = trans_special(&curp, STRLEN(curp), buf, false);
                  curp--;

                  for (size_t i = 0; i < size; i++) {
                    F(dump_char, buf[i]);
                  }

                  break;
                }
                default: {
                  F(dump_char, *curp);
                  break;
                }
              }
              break;
            }
            default: {
              F(dump_char, *curp);
              break;
            }
          }
        }
        WS("\"");
      }
      break;
    }
    default: {
      assert(false);
    }
  }
  FUNCTION_END;
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
///       the current scope, if it is not it may be a construct like 
///       "a{':abc'}" which refers to "a:abc" variable.
///
///   TS_ONLY_SEGMENT
///   :   Determines whether this segment is the only one.
///
///   TS_FUNCCALL
///   :   Use "vim.functions" in place of "state.current_scope" in some cases. 
///       Note that vim.call implementation should still be able to use 
///       "state.global.user_functions" if appropriate.
///
///   TS_FUNCASSIGN
///   :   Use "state.global.user_functions" in place of "state.current_scope" in 
///       some cases.
/// @endparblock
static FDEC(translate_scope, const char **start,
                             const ExpressionNode *const expr,
                             const uint_least8_t flags)
{
  FUNCTION_START;
  assert(expr->type == kExprSimpleVariableName ||
         (expr->type == kExprIdentifier && !(flags&TS_ONLY_SEGMENT)));
  if (expr->end_position == expr->position) {
    if (!(flags & (TS_LAST_SEGMENT|TS_ONLY_SEGMENT))
        && strchr("svalgtwb", *(expr->position)) != NULL) {
      *start = NULL;
    } else {
      *start = expr->position;
      if ((flags & TS_FUNCCALL) && ASCII_ISLOWER(*expr->position)) {
        WS("vim.functions");
      } else if (flags & (TS_FUNCASSIGN|TS_FUNCCALL)) {
        WS("state.global.user_functions");
      } else {
        WS("state.current_scope");
      }
    }
  } else if (expr->position[1] == ':') {
    switch (*expr->position) {
      case 's':
      case 'v':
      case 'a':
      case 'l':
      case 'g': {
        *start = expr->position + 2;
        WS("state.");
        if (*expr->position == 'g'
            || *expr->position == 'v')
          WS("global.");
        W_LEN(expr->position, 1);
        break;
      }
      case 't': {
        *start = expr->position + 2;
        WS("state.global.tabpage.t");
        break;
      }
      case 'w': {
        *start = expr->position + 2;
        WS("state.global.window.w");
        break;
      }
      case 'b': {
        *start = expr->position + 2;
        WS("state.global.buffer.b");
        break;
      }
      default: {
        *start = expr->position;
        if (flags & (TS_FUNCASSIGN|TS_FUNCCALL)) {
          WS("state.global.user_functions");
        } else {
          WS("state.current_scope");
        }
        break;
      }
    }
  } else {
    bool isfunc = false;

    *start = expr->position;
    if ((flags & TS_FUNCCALL) && ASCII_ISLOWER(*expr->position)) {
      isfunc = true;
      const char *s;
      for (s = expr->position + 1; s != expr->end_position; s++) {
        if (!(ASCII_ISLOWER(*s) || VIM_ISDIGIT(*s))) {
          isfunc = false;
          break;
        }
      }
    }
    if (isfunc && !(flags & TS_FUNCASSIGN)) {
      WS("vim.functions");
    } else if (flags & (TS_FUNCASSIGN|TS_FUNCCALL)) {
      WS("state.global.user_functions");
    } else {
      WS("state.current_scope");
    }
  }
  FUNCTION_END;
}

/// Dump parsed VimL expression
///
/// @param[in]  expr  Expression being dumped.
static FDEC(translate_expr, const ExpressionNode *const expr,
                            const bool is_funccall)
{
  FUNCTION_START;
  switch (expr->type) {
    case kExprListRest: {
      // TODO Add this error in expr parser, not here
      WS("vim.err.err(state, nil, true, \"E696: Missing comma in list\")");
      break;
    }
    case kExprFloat: {
      WS("vim.float:new(state, ");
      W_END(expr->position, expr->end_position);
      WS(")");
      break;
    }
    case kExprDecimalNumber:
    case kExprOctalNumber:
    case kExprHexNumber: {
      F(translate_number, expr->type, expr->position, expr->end_position);
      break;
    }
    case kExprDoubleQuotedString:
    case kExprSingleQuotedString: {
      F(translate_string, expr->type, expr->position, expr->end_position);
      break;
    }
    case kExprOption: {
      const char *name_start;
      OptionType type;
      uint_least8_t option_properties;
      const char *s = expr->position;
      const char *e = expr->end_position;

      if (e - s > 2 && s[1] == ':') {
        assert(*s == 'g' || *s == 'l');
        type = (*s == 'g' ? kOptGlobal : kOptLocal);
        name_start = s + 2;
      } else {
        type = kOptDefault;
        name_start = s;
      }

      option_properties = get_option_properties(name_start, e - name_start + 1);
      // If option is not available in this version of neovim …
      if (option_properties & GOP_DISABLED) {
        // … just dump static value
        switch (option_properties & GOP_TYPE_MASK) {
          case GOP_BOOLEAN: {
            WS(VIM_FALSE);
            break;
          }
          case GOP_NUMERIC: {
            WS(VIM_ZERO);
            break;
          }
          case GOP_STRING: {
            WS(VIM_EMPTY_STRING);
            break;
          }
          default: {
            // Option may only be boolean, string or numeric, not a combination 
            // of these
            assert(false);
          }
        }
      } else {
            // Requested global option when there is global value
        if ((type == kOptGlobal && (option_properties & GOP_GLOBAL))
            // Or requested option that has nothing, but global value
            || !((option_properties & GOP_LOCALITY_MASK) ^ GOP_GLOBAL)) {
          WS("state.global.options['");
          W_END(name_start, e);
          WS("']");
        } else {
          if (option_properties & GOP_GLOBAL) {
            WS("vim.get_local_option(state, ");
          }

          if (option_properties & GOP_BUFFER_LOCAL) {
            WS("state.global.buffer");
          } else if (option_properties & GOP_WINDOW_LOCAL) {
            WS("state.global.window");
          } else {
            assert(false);
          }

          if (option_properties & GOP_GLOBAL) {
            WS(", ");
          } else {
            WS("[");
          }
          WS("'");
          W_END(name_start, e);
          WS("'");
          if (option_properties & GOP_GLOBAL) {
            WS(")");
          } else {
            WS("]");
          }
        }
      }
      break;
    }
    case kExprRegister: {
      WS("state.registers[");
      if (expr->end_position < expr->position) {
        WS("nil");
      } else {
        F(dump_string_length, expr->position, 1);
      }
      WS("]");
      break;
    }
    case kExprEnvironmentVariable: {
      WS("state.environment[");
      W_EXPR_POS_ESCAPED(expr);
      WS("]");
      break;
    }
    case kExprSimpleVariableName: {
      const char *start;
      WS("vim.subscript.subscript(state, false, ");
      F(translate_scope, &start, expr, TS_ONLY_SEGMENT | (is_funccall
                                                             ? TS_FUNCCALL
                                                             : 0));
      assert(start != NULL);
      WS(", '");
      W_END(start, expr->end_position);
      WS("')");
      break;
    }
    case kExprVariableName: {
      WS("vim.subscript.subscript(state, false, ");
      F(translate_varname, expr, FALSE);
      WS(")");
      break;
    }
    case kExprCurlyName:
    case kExprIdentifier: {
      // Should have been handled by translate_varname above
      assert(FALSE);
    }
    case kExprConcatOrSubscript: {
      WS("vim.concat_or_subscript(state, ");
      W_EXPR_POS_ESCAPED(expr);
      WS(", ");
      F(translate_expr, expr->children, false);
      WS(")");
      break;
    }
    case kExprEmptySubscript: {
      WS("nil");
      break;
    }
    case kExprExpression: {
      WS("(");
      F(translate_expr, expr->children, false);
      WS(")");
      break;
    }
    default: {
      const ExpressionNode *current_expr;
      bool reversed = false;

      assert(expr->children != NULL
             || expr->type == kExprDictionary
             || expr->type == kExprList);
      switch (expr->type) {
        case kExprDictionary: {
          WS("vim.dict:new(state");
          break;
        }
        case kExprList: {
          WS("vim.list:new(state");
          break;
        }
        case kExprSubscript: {
          if (expr->children->next->next == NULL) {
            WS("vim.subscript.subscript(state, true");
          } else {
            WS("vim.subscript.slice(state");
          }
          break;
        }

#define OPERATOR(op_type, op) \
        case op_type: { \
          WS("vim." op "(state"); \
          break; \
        }

        OPERATOR(kExprAdd,          "op.add")
        OPERATOR(kExprSubtract,     "op.subtract")
        OPERATOR(kExprDivide,       "op.divide")
        OPERATOR(kExprMultiply,     "op.multiply")
        OPERATOR(kExprModulo,       "op.modulo")
        OPERATOR(kExprCall,         "subscript.call")
        OPERATOR(kExprMinus,        "op.negate")
        OPERATOR(kExprNot,          "op.negate_logical")
        OPERATOR(kExprPlus,         "op.promote_integer")
        OPERATOR(kExprStringConcat, "op.concat")
#undef OPERATOR

#define COMPARISON(forward_type, rev_type, op) \
        case forward_type: \
        case rev_type: { \
          if (expr->type == rev_type) { \
            reversed = true; \
            WS("vim.op.negate_logical(state, "); \
          } \
          WS("vim.op." op "(state, "); \
          switch (expr->ignore_case) { \
            case kCCStrategyUseOption: { \
              WS("state.global.options.ignorecase"); \
              break; \
            } \
            case kCCStrategyMatchCase: { \
              WS("false"); \
              break; \
            } \
            case kCCStrategyIgnoreCase: { \
              WS("true"); \
              break; \
            } \
          } \
          break; \
        }

        COMPARISON(kExprEquals,    kExprNotEquals,            "equals")
        COMPARISON(kExprIdentical, kExprNotIdentical,         "identical")
        COMPARISON(kExprMatches,   kExprNotMatches,           "matches")
        COMPARISON(kExprGreater,   kExprLessThanOrEqualTo,    "greater")
        COMPARISON(kExprLess,      kExprGreaterThanOrEqualTo, "less")
#undef COMPARISON

        default: {
          assert(false);
        }
      }

      current_expr = expr->children;
      if (expr->type == kExprCall) {
        WS(", ");
        F(translate_expr, current_expr, true);
        current_expr = current_expr->next;
      }
      for (; current_expr != NULL; current_expr=current_expr->next) {
        WS(", ");
        F(translate_expr, current_expr, false);
      }
      WS(")");

      if (reversed) {
        WS(")");
      }

      break;
    }
  }
  FUNCTION_END;
}

/// Dump a sequence of VimL expressions (e.g. for :echo 1 2 3)
///
/// @param[in]  expr  Pointer to the first expression that will be dumped.
static FDEC(translate_exprs, const ExpressionNode *const expr)
{
  FUNCTION_START;
  const ExpressionNode *current_expr = expr;

  for (;;) {
    F(translate_expr, current_expr, false);
    current_expr = current_expr->next;
    if (current_expr == NULL) {
      break;
    } else {
      WS(", ");
    }
  }

  FUNCTION_END;
}

/// Dump a VimL function definition
///
/// @param[in]  args  Function node and indentation that was used for function 
///                   definition.
static FDEC(translate_function_definition, const TranslateFuncArgs *const args)
{
  FUNCTION_START;
  size_t i;
  const char **data =
      (const char **) args->node->args[ARG_FUNC_ARGS].arg.strs.ga_data;
  size_t size = (size_t) args->node->args[ARG_FUNC_ARGS].arg.strs.ga_len;
  bool varargs = args->node->args[ARG_FUNC_FLAGS].arg.flags & FLAG_FUNC_VARARGS;
  WS("function(state, self");
  for (i = 0; i < size; i++) {
    WS(", ");
    W(data[i]);
  }
  if (varargs) {
    WS(", ...");
  }
  WS(")\n");
  if (args->node->children != NULL) {
    WINDENT(args->indent + 1);
    // TODO; dump information about function call
    WS("state = vim.state.enter_function(state, self, {})\n");
    for (i = 0; i < size; i++) {
      WINDENT(args->indent + 1);
      WS("state.a['");
      W(data[i]);
      WS("'] = ");
      W(data[i]);
      WS("\n");
    }
    if (varargs) {
      WINDENT(args->indent + 1);
      WS("state.a['000'] = vim.list:new(state, ...)\n");
      WINDENT(args->indent + 1);
      WS("state.a['0'] = select('#', ...)\n");
      WINDENT(args->indent + 1);
      WS("for i = 1,select('#', ...) do\n");
      WINDENT(args->indent + 2);
      WS("state.a[tostring(i)] = select(i, ...)\n");
      WINDENT(args->indent + 1);
      WS("end\n");
    }
    // TODO Assign a:firstline and a:lastline
    // These variables are always present even if function is defined without 
    // range modifier.
    F_FUNC(translate_nodes, args->node->children, args->indent + 1);
  } else {
    // Empty function: do not bother creating scope dictionaries, just return 
    // zero
    WINDENT(args->indent + 1);
    WS("return " VIM_ZERO "\n");
  }
  WINDENT(args->indent);
  WS("end");
  FUNCTION_END;
}

/// Translate complex VimL variable name (i.e. name with curly brace expansion)
///
/// Translates to two arguments: scope and variable name in this scope. Must be 
/// the last arguments to the outer function because it may translate to one 
/// argument: a call of get_scope_and_key which will return two values.
///
/// @param[in]  expr         Translated variable name. Must be a node with 
///                          kExprVariableName type.
/// @param[in]  is_funccall  True if translating name of a called function.
static FDEC(translate_varname, const ExpressionNode *const expr,
                               const bool is_funccall)
{
  FUNCTION_START;
  const ExpressionNode *current_expr = expr->children;
  bool add_concat;
  bool close_parenthesis = false;

  assert(expr->type == kExprVariableName);
  assert(current_expr != NULL);

  if (current_expr->type == kExprIdentifier) {
    const char *start;
    F(translate_scope, &start, current_expr, (is_funccall
                                              ? TS_FUNCASSIGN
                                              : 0));
    if (start == NULL) {

      WS("vim.get_scope_and_key(state, vim.concat(state, '");
      W_EXPR_POS(current_expr);
      WS("'");
      close_parenthesis = true;
    } else {
      WS(", vim.concat(state, '");
      W_END(start, current_expr->end_position);
      WS("'");
    }
    add_concat = true;
  } else {
    WS("vim.get_scope_and_key(state, vim.concat(state, ");
    add_concat = false;
    close_parenthesis = true;
  }

  for (current_expr = current_expr->next; current_expr != NULL;
        current_expr = current_expr->next) {
    if (add_concat) {
      WS(", ");
    } else {
      add_concat = true;
    }
    switch (current_expr->type) {
      case kExprIdentifier: {
        WS("'");
        W_EXPR_POS(current_expr);
        WS("'");
        break;
      }
      case kExprCurlyName: {
        F(translate_expr, current_expr->children, false);
        break;
      }
      default: {
        assert(false);
      }
    }
  }

  WS(")");

  if (close_parenthesis) {
    WS(")");
  }

  FUNCTION_END;
}

/// Translate lvalue into one of vim.assign.* calls
///
/// @note Newline is not written.
///
/// @param[in]  expr         Translated expression.
/// @param[in]  is_funccall  True if it translates :function definition.
/// @param[in]  bang         True if function must not be unique when 
///                          is_funccall is set and true if errors about missing 
///                          values are to be ignored.
/// @param[in]  dump         Function used to dump value that will be assigned. 
///                          When NULL use commands for undefining variable and 
///                          function definitions (backs :unlet and 
///                          :delfunction).
/// @param[in]  dump_cookie  First argument to the above function.
static FDEC(translate_lval, const ExpressionNode *const expr,
                            const bool is_funccall, const bool bang,
                            const FTYPE(AssignmentValueDump) FNAME(dump),
                            const void *const dump_cookie)
{
  FUNCTION_START;
#define ADD_CALL(what) \
  do { \
    if (FNAME(dump) == NULL) { \
      if (is_funccall) { \
        WS("vim.assign.del_" what "_function(state, "); \
      } else { \
        WS("vim.assign.del_" what "(state, "); \
      } \
      F(dump_bool, bang); \
      WS(", "); \
    } else { \
      if (is_funccall) { \
        WS("vim.assign.ass_" what "_function(state, "); \
        F(dump_bool, bang); \
        WS(", "); \
      } else { \
        WS("vim.assign.ass_" what "(state, "); \
      } \
      F(dump, dump_cookie); \
      WS(", "); \
    } \
  } while (0)
  switch (expr->type) {
    case kExprSimpleVariableName: {
      const char *start;
      ADD_CALL("dict");
      F(translate_scope, &start, expr, TS_ONLY_SEGMENT|TS_LAST_SEGMENT
                                          |(is_funccall
                                            ? TS_FUNCASSIGN
                                            : 0));
      assert(start != NULL);
      WS(", '");
      W_END(start, expr->end_position);
      WS("')");
      break;
    }
    case kExprVariableName: {
      const ExpressionNode *current_expr = expr->children;

      assert(current_expr != NULL);

      ADD_CALL("dict");

      F(translate_varname, expr, is_funccall);

      WS(")");
      break;
    }
    case kExprConcatOrSubscript: {
      ADD_CALL("dict");
      F(translate_expr, expr->children, false);
      WS(", '");
      W_EXPR_POS(expr);
      WS("')");
      break;
    }
    case kExprSubscript: {
      if (expr->children->next->next == NULL) {
        ADD_CALL("dict");
      } else {
        ADD_CALL("slice");
      }
      F(translate_expr, expr->children, false);
      WS(", ");
      F(translate_expr, expr->children->next, false);
      if (expr->children->next->next != NULL) {
        WS(", ");
        F(translate_expr, expr->children->next->next, false);
      }
      WS(")");
      break;
    }
    default: {
      assert(false);
    }
  }
#undef ADD_CALL
  FUNCTION_END;
}

/// Helper function that dumps list element
///
/// List is located in indentvar described in args.
///
/// @param[in]  args  Variable name and list index.
static FDEC(translate_let_list_item, const LetListItemAssArgs *const args)
{
  FUNCTION_START;
  WS("vim.list.raw_subscript(");
  W(args->var);
  WS(", ");
  F_NOOPT(dump_number, (intmax_t) args->idx);
  WS(")");
  FUNCTION_END;
}

/// Helper function that dumps tail of the list
///
/// List is located in indentvar described in args.
///
/// Indentvar is a variable with name based on indentation level. It is used to 
/// make variable name unique in some scope.
///
/// @param[in]  args  Indentvar description and number of first element in 
///                   dumped list.
static FDEC(translate_let_list_rest, const LetListItemAssArgs *const args)
{
  FUNCTION_START;
  WS("vim.list.raw_slice_to_end(");
  W(args->var);
  WS(", ");
  F_NOOPT(dump_number, (intmax_t) args->idx);
  WS(")");
  FUNCTION_END;
}

/// Helper functions that dumps expression
///
/// Proxy to translate_expr without the second argument.
///
/// @param[in]  expr  Dumped expression.
static FDEC(translate_rval_expr, const ExpressionNode *const expr)
{
  FUNCTION_START;
  F(translate_expr, expr, false);
  FUNCTION_END;
}

/// Helper function for dumping NUL-terminated string
///
/// @param[in]  str  Dumped string
static FDEC(dump_raw_string, const char *const str)
{
  FUNCTION_START;
  W(str);
  FUNCTION_END;
}

/// Translate assignment
///
/// @note It is assumed that this function is called with indent for the first 
///       line already written.
///
/// @param[in]  lval_expr    Value being assigned to.
/// @param[in]  indent       Current level of indentation.
/// @param[in]  err_line     Line that should be run when error occurred. May be 
///                          NULL.
/// @param[in]  dump         Function used to dump value that will be assigned.
/// @param[in]  dump_cookie  First argument to the above function.
static FDEC(translate_assignment, const ExpressionNode *const lval_expr,
                                  const size_t indent,
                                  const char *const err_line,
                                  FTYPE(AssignmentValueDump) FNAME(dump),
                                  const void *const dump_cookie)
{
  FUNCTION_START;
#define ADD_ASSIGN(expr, err_line, indent, dump, dump_cookie) \
  do { \
    if (err_line != NULL) {\
      WS("if "); \
    } \
    F(translate_lval, expr, false, false, dump, dump_cookie); \
    if (err_line != NULL) { \
      WS(" == nil then\n"); \
      WINDENT(indent + 1); \
      W(err_line); \
      WS("\n"); \
      WINDENT(indent); \
      WS("end\n"); \
    } else { \
      WS("\n"); \
    } \
  } while (0)
  if (lval_expr->type == kExprList) {
    bool has_rest = false;
    size_t val_num = 0;
    const ExpressionNode *current_expr;

    WS("do\n");
    WINDENT(indent + 1);
    WS("local rhs = ");
    F(dump, dump_cookie);
    WS("\n");

    current_expr = lval_expr->children;
    for (; current_expr != NULL; current_expr = current_expr->next) {
      if (current_expr->type == kExprListRest) {
        has_rest = true;
      } else {
        val_num++;
      }
    }

    assert(val_num > 0);

    WINDENT(indent + 1);
    WS("if vim.is_list(rhs) then\n");

    WINDENT(indent + 2);
    WS("if (vim.list.length(rhs)");
    if (has_rest) {
      WS(" >= ");
    } else {
      WS(" == ");
    }
    F_NOOPT(dump_unumber, (uintmax_t) val_num);
    WS(") then\n");

    current_expr = lval_expr->children;
    for (size_t i = 0; i < val_num; i++) {
      LetListItemAssArgs args = {
        i,
        "rhs"
      };
      WINDENT(indent + 3);
      ADD_ASSIGN(current_expr, err_line, indent + 3,
                 (FTYPE(AssignmentValueDump)) (&FNAME(translate_let_list_item)),
                 &args);
      current_expr = current_expr->next;
    }
    if (has_rest) {
      LetListItemAssArgs args = {
        val_num + 1,
        "rhs"
      };
      WINDENT(indent + 3);
      ADD_ASSIGN(current_expr->children, err_line, indent + 3,
                 (FTYPE(AssignmentValueDump)) (&FNAME(translate_let_list_rest)),
                 &args);
    }

    WINDENT(indent + 2);
    WS("else\n");
    WINDENT(indent + 3);
    if (!has_rest) {
      WS("if (vim.list.length(rhs) > ");
      F_NOOPT(dump_unumber, (uintmax_t) val_num);
      WS(") then\n");
      WINDENT(indent + 4);
      WS("vim.err.err(state, nil, true, "
          "\"E688: More targets than List items\")\n");
      WINDENT(indent + 3);
      WS("else\n");
      WINDENT(indent + 4);
    }
    // TODO Dump position
    WS("vim.err.err(state, nil, true, "
        "\"E687: Less targets than List items\")\n");
    if (!has_rest) {
      WINDENT(indent + 3);
      WS("end\n");
    }
    if (err_line != NULL) {
      WINDENT(indent + 3);
      W(err_line);
      WS("\n");
    }
    WINDENT(indent + 2);
    WS("end\n");

    WINDENT(indent + 1);
    WS("else\n");
    WINDENT(indent + 2);
    WS("vim.err.err(state, nil, true, \"E714: List required\")\n");
    if (err_line != NULL) {
      WINDENT(indent + 2);
      W(err_line);
      WS("\n");
    }
    WINDENT(indent + 1);
    WS("end\n");
    WINDENT(indent);
    WS("end\n");
  } else {
    ADD_ASSIGN(lval_expr, err_line, indent, FNAME(dump), dump_cookie);
  }
#undef ADD_ASSIGN
  FUNCTION_END;
}

#define CMD_FDEC(f) \
    FDEC(f, const CommandNode *const node, const size_t indent)

static CMD_FDEC(translate_comment)
{
  FUNCTION_START;
  WINDENT(indent);
  WS("-- \"");
  W(node->args[ARG_NAME_NAME].arg.str);
  WS("\n");
  FUNCTION_END;
}

static CMD_FDEC(translate_hashbang_comment)
{
  FUNCTION_START;
  WINDENT(indent);
  WS("-- #!");
  W(node->args[ARG_NAME_NAME].arg.str);
  WS("\n");
  FUNCTION_END;
}

static CMD_FDEC(translate_error)
{
  FUNCTION_START;
  WINDENT(indent);
  WS("vim.err.err(state, nil, true, ");
  // FIXME Dump error position
  F(dump_string, node->args[ARG_ERROR_MESSAGE].arg.str);
  WS(")\n");
  FUNCTION_END;
}

/// Translate missing command
static CMD_FDEC(translate_missing)
{
  FUNCTION_START;
  WS("\n");
  FUNCTION_END;
}

/// Translate user command call
static CMD_FDEC(translate_user)
{
  FUNCTION_START;
  WINDENT(indent);
  WS("vim.run_user_command(state, '");
  W(node->name);
  WS("', ");
  F(translate_range, &(node->range));
  WS(", ");
  F(dump_bool, node->bang);
  WS(", ");
  F(dump_string, node->args[ARG_USER_ARG].arg.str);
  WS(")\n");
  FUNCTION_END;
}

static CMD_FDEC(translate_function)
{
  FUNCTION_START;
  const TranslateFuncArgs args = {node, indent};
  WINDENT(indent);
  F(translate_lval, node->args[ARG_FUNC_NAME].arg.expr, true, node->bang,
                    (FTYPE(AssignmentValueDump))
                      &FNAME(translate_function_definition),
                    (void *) &args);
  WS("\n");
  FUNCTION_END;
}

static CMD_FDEC(translate_for)
{
  FUNCTION_START;
  WINDENT(indent);
  WS("for _, i in vim.list.iterator(state, ");
  F(translate_expr, node->args[ARG_FOR_RHS].arg.expr, false);
  WS(") do\n");

  WINDENT(indent + 1);
  F(translate_assignment, node->args[ARG_FOR_LHS].arg.expr, indent + 1,
                          "break",
                          (FTYPE(AssignmentValueDump))
                            (&FNAME(dump_raw_string)),
                          (void *) "i");

  F(translate_nodes, node->children, indent + 1);

  WINDENT(indent);
  WS("end\n");
  FUNCTION_END;
}

static CMD_FDEC(translate_while)
{
  FUNCTION_START;
  WINDENT(indent);
  WS("while vim.get_boolean(state, ");
  F(translate_expr, node->args[ARG_EXPR_EXPR].arg.expr, false);
  WS(") do\n");

  F(translate_nodes, node->children, indent + 1);

  WINDENT(indent);
  WS("end\n");
  FUNCTION_END;
}

// Translate :if, :elseif and :else
static CMD_FDEC(translate_if_block)
{
  FUNCTION_START;
  WINDENT(indent);
  switch (node->type) {
    case kCmdElse: {
      WS("else\n");
      break;
    }
    case kCmdElseif: {
      WS("else");
      // fallthrough
    }
    case kCmdIf: {
      WS("if vim.get_boolean(state, ");
      F(translate_expr, node->args[ARG_EXPR_EXPR].arg.expr, false);
      WS(") then\n");
      break;
    }
    default: {
      assert(false);
    }
  }

  F(translate_nodes, node->children, indent + 1);

  if (node->next == NULL
      || (node->next->type != kCmdElseif
          && node->next->type != kCmdElse)) {
    WINDENT(indent);
    WS("end\n");
  }
  FUNCTION_END;
}

static CMD_FDEC(translate_try_block)
{
  FUNCTION_START;
#define CHECK_NEW_RET(indent) \
  do { \
    WINDENT(indent); \
    WS("if (new_ret ~= nil) then\n"); \
    WINDENT(indent + 1); \
    WS("ret = new_ret\n"); \
    WINDENT(indent); \
    WS("end\n"); \
  } while (0)
  CommandNode *first_catch = NULL;
  CommandNode *finally = NULL;
  CommandNode *next = node->next;

  for (next = node->next;
       next->type == kCmdCatch || next->type == kCmdFinally;
       next = next->next) {
    switch (next->type) {
      case kCmdCatch: {
        if (first_catch == NULL) {
          first_catch = next;
        }
        continue;
      }
      case kCmdFinally: {
        finally = next;
        break;
      }
      default: {
        assert(false);
      }
    }
    break;
  }

  WINDENT(indent);
  WS("do\n");

  WINDENT(indent + 1);
  WS("local ok, err, ret\n");
  WINDENT(indent + 1);
  WS("ok, err, ret = pcall(function(state)\n");
  F(translate_nodes, node->children, indent + 2);
  WINDENT(indent + 1);
  WS("end, vim.state.enter_try(state))\n");

  if (finally != NULL) {
    WINDENT(indent + 1);
    WS("local fin = function(state)\n");
    F(translate_nodes, finally->children, indent + 2);
    WINDENT(indent + 1);
    WS("end\n");
  }

  if (first_catch != NULL) {
    WINDENT(indent + 1);
    WS("local catch\n");
  }

  if (first_catch != NULL) {
    bool did_first_if = false;

    WINDENT(indent + 1);
    WS("if (not ok) then\n");

    for (next = first_catch; next->type == kCmdCatch; next = next->next) {
      size_t current_indent;

      if (did_first_if) {
        WINDENT(indent + 1);
        WS("else");
      }

      if (next->args[ARG_REG_REG].arg.reg == NULL) {
        if (did_first_if) {
          WS("\n");
        }
      } else {
        WINDENT(indent + 2);
        WS("if (vim.err.matches(state, err, ");
        F(translate_regex, next->args[ARG_REG_REG].arg.reg);
        WS(") then\n");
        did_first_if = true;
      }
      current_indent = indent + 2 + (did_first_if ? 1 : 0);
      WINDENT(current_indent);
      WS("catch = function(state)\n");
      F(translate_nodes, next->children, current_indent + 1);
      WINDENT(current_indent);
      WS("end\n");
      WINDENT(current_indent);
      WS("ok = 'caught'\n");  // String "'caught'" is true

      if (next->args[ARG_REG_REG].arg.reg == NULL) {
        break;
      }
    }

    if (did_first_if) {
      WINDENT(indent + 2);
      WS("end\n");
    }

    WINDENT(indent + 1);
    WS("end\n");
  }

  if (first_catch != NULL) {
    WINDENT(indent + 1);
    WS("if (catch) then\n");
    WINDENT(indent + 2);
    WS("local new_ret = catch(vim.state.enter_catch(state, err))\n");
    CHECK_NEW_RET(indent + 2);
    WINDENT(indent + 1);
    WS("end\n");
  }

  if (finally != NULL) {
    WINDENT(indent + 1);
    WS("local new_ret = fin(state)\n");
    CHECK_NEW_RET(indent + 1);
  }

  WINDENT(indent + 1);
  WS("if (not ok) then\n");
  WINDENT(indent + 2);
  WS("vim.err.propagate(state, err)\n");
  WINDENT(indent + 1);
  WS("end\n");

  WINDENT(indent + 1);
  WS("if (ret ~= nil) then\n");
  WINDENT(indent + 2);
  WS("return ret\n");
  WINDENT(indent + 1);
  WS("end\n");
  WINDENT(indent);
  WS("end\n");
#undef CHECK_NEW_RET

  FUNCTION_END;
}

static CMD_FDEC(translate_unlet)
{
  FUNCTION_START;
  const ExpressionNode *lval_expr = node->args[ARG_EXPR_EXPR].arg.expr;
  for (; lval_expr != NULL; lval_expr = lval_expr->next) {
    F(translate_lval, lval_expr, false, node->bang, NULL, NULL);
  }
  FUNCTION_END;
}

static CMD_FDEC(translate_delfunction)
{
  FUNCTION_START;
  const ExpressionNode *lval_expr = node->args[ARG_EXPR_EXPR].arg.expr;
  for (; lval_expr != NULL; lval_expr = lval_expr->next) {
    F(translate_lval, lval_expr, true, node->bang, NULL, NULL);
  }
  FUNCTION_END;
}

static CMD_FDEC(translate_let)
{
  FUNCTION_START;
  if (node->args[ARG_LET_RHS].arg.expr != NULL) {
    const ExpressionNode *lval_expr = node->args[ARG_LET_LHS].arg.expr;
    const ExpressionNode *rval_expr = node->args[ARG_LET_RHS].arg.expr;
    LetAssignmentType ass_type = node->args[ARG_LET_ASS_TYPE].arg.unumber;
    WINDENT(indent);
    switch (ass_type) {
      case VAL_LET_NO_ASS: {
        assert(false);
      }
      case VAL_LET_ASSIGN: {
        F(translate_assignment, lval_expr, indent, NULL,
                                   (FTYPE(AssignmentValueDump))
                                      &FNAME(translate_rval_expr),
                                   (void *) rval_expr);
        break;
      }
      case VAL_LET_ADD:
      case VAL_LET_SUBTRACT:
      case VAL_LET_APPEND: {
        // FIXME Use STATIC_ASSERT
        assert(VAL_LET_ADD < VAL_LET_SUBTRACT);
        assert(VAL_LET_ADD < VAL_LET_APPEND);
        static const ExpressionType op_types[] = {
          [VAL_LET_ADD - VAL_LET_ADD] = kExprAdd,
          [VAL_LET_SUBTRACT - VAL_LET_ADD] = kExprSubtract,
          [VAL_LET_APPEND - VAL_LET_ADD] = kExprStringConcat,
        };
        const ExpressionNode rhs = {
          .type = kExprExpression,
          .next = NULL,
          .children = (ExpressionNode *) rval_expr
        };
        const ExpressionNode lhs = {
          .type = kExprExpression,
          .next = (ExpressionNode *) &rhs,
          .children = (ExpressionNode *) lval_expr
        };
        const ExpressionNode top = {
          .type = op_types[ass_type - VAL_LET_ADD],
          .next = NULL,
          .children = (ExpressionNode *) &lhs
        };
        F(translate_assignment, lval_expr, indent, NULL,
                                   (FTYPE(AssignmentValueDump))
                                      &FNAME(translate_rval_expr),
                                   (void *) &top);
        break;
      }
    }
  } else {
    F(translate_node, node, indent);
  }
  FUNCTION_END;
}

#undef CMD_FDEC

/// Generic VimL Ex commands dumper
///
/// @param[in]  node    Node to translate.
/// @param[in]  indent  Indentation of the result.
static FDEC(translate_node, const CommandNode *const node,
                            const size_t indent)
{
  FUNCTION_START;
  const char *name;
  size_t start_from_arg = 0;
  bool do_arg_dump = true;
  bool add_comma = false;

  WINDENT(indent);

  name = CMDDEF(node->type).name;
  assert(name != NULL);

  WS("vim.commands");
  if (ASCII_ISALPHA(*name)) {
    WS(".");
    W(name);
  } else {
    WS("['");
    W(name);
    WS("']");
  }
  WS("(state, ");

  if (CMDDEF(node->type).flags & RANGE) {
    if (add_comma) {
      WS(", ");
    }
    F(translate_range, &(node->range));
    add_comma = true;
  }

  if (CMDDEF(node->type).flags & BANG) {
    if (add_comma) {
      WS(", ");
    }
    F(dump_bool, node->bang);
    add_comma = true;
  }

  if (CMDDEF(node->type).flags & EXFLAGS) {
    if (add_comma) {
      WS(", ");
    }
    F(translate_ex_flags, node->exflags);
    add_comma = true;
  }

  if (CMDDEF(node->type).parse == CMDDEF(kCmdAbclear).parse) {
    if (add_comma) {
      WS(", ");
    }
    F(dump_bool, (bool) node->args[ARG_CLEAR_BUFFER].arg.flags);
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdEcho).parse) {
    start_from_arg = 1;
  } else if (CMDDEF(node->type).parse == CMDDEF(kCmdCaddexpr).parse) {
    start_from_arg = 1;
  }

  if (do_arg_dump) {
    size_t i;
    size_t num_args = CMDDEF(node->type).num_args;

    for (i = start_from_arg; i < num_args; i++) {
      if (add_comma) {
        WS(", ");
      }
      add_comma = false;
      switch (CMDDEF(node->type).arg_types[i]) {
        case kArgExpression: {
          F(translate_expr, node->args[i].arg.expr, false);
          add_comma = true;
          break;
        }
        case kArgExpressions: {
          F(translate_exprs, node->args[i].arg.expr);
          add_comma = true;
          break;
        }
        case kArgString: {
          F(dump_string, node->args[i].arg.str);
          add_comma = true;
          break;
        }
        default: {
          break;
        }
      }
    }
  }

  WS(")\n");

  FUNCTION_END;
}


/// Dump a sequence of Ex commands
///
/// @param[in]  node    Pointer to the first node that will be translated.
/// @param[in]  indent  Indentation of the result.
static FDEC(translate_nodes, const CommandNode *const node, size_t indent)
{
  FUNCTION_START;
  const CommandNode *current_node;

  for (current_node = node; current_node != NULL;
       current_node = current_node->next) {
#define CMD_F(f) \
  F(f, current_node, indent)
    switch (current_node->type) {
      case kCmdFinish: {
        switch (TRANSLATION_CONTEXT) {
          case kTransFunc:
          case kTransUser: {
            WINDENT(indent);
            // TODO dump error position
            WS("vim.err.err(state, nil, true, "
               "\"E168: :finish used outside of a sourced file\""
               ")\n");
            continue;
          }
          case kTransScript: {
            WINDENT(indent);
            WS("return nil\n");
            break;
          }
        }
        break;
      }
      case kCmdReturn: {
        switch (TRANSLATION_CONTEXT) {
          case kTransScript:
          case kTransUser: {
            WINDENT(indent);
            // TODO dump error position
            WS("vim.err.err(state, nil, true, "
               "\"E133: :return not inside a function\""
               ")\n");
            continue;
          }
          case kTransFunc: {
            WINDENT(indent);
            WS("return ");
            F(translate_expr, node->args[ARG_EXPR_EXPR].arg.expr, false);
            WS("\n");
            break;
          }
        }
        break;
      }
      case kCmdEndwhile:
      case kCmdEndfor:
      case kCmdEndif:
      case kCmdEndfunction:
      case kCmdEndtry: {
        // Handled by :while/:for/:if/:function/:try handlers
        continue;
      }
      case kCmdCatch:
      case kCmdFinally: {
        // Handled by :try handler
        continue;
      }
#define SET_HANDLER(cmd_type, handler) \
      case cmd_type: { \
        CMD_F(handler); \
        continue; \
      }
      SET_HANDLER(kCmdComment, translate_comment)
      SET_HANDLER(kCmdHashbangComment, translate_hashbang_comment)
      SET_HANDLER(kCmdSyntaxError, translate_error)
      SET_HANDLER(kCmdMissing, translate_missing)
      SET_HANDLER(kCmdUSER, translate_user)
      SET_HANDLER(kCmdFunction, translate_function)
      SET_HANDLER(kCmdFor, translate_for)
      SET_HANDLER(kCmdWhile, translate_while)
      case kCmdElse:
      case kCmdElseif:
      SET_HANDLER(kCmdIf, translate_if_block)
      SET_HANDLER(kCmdTry, translate_try_block)
      SET_HANDLER(kCmdLet, translate_let)
      SET_HANDLER(kCmdUnlet, translate_unlet)
      SET_HANDLER(kCmdDelfunction, translate_delfunction)
#undef SET_HANDLER
      default: {
        F(translate_node, current_node, indent);
        continue;
      }
    }
    break;
#undef CMD_F
  }

  if (current_node == NULL && TRANSLATION_CONTEXT == kTransFunc) {
    WINDENT(indent);
    WS("return " VIM_ZERO "\n");
  }

  FUNCTION_END;
}

/// Dump .vim script as lua module.
///
/// @param[in]  node    Pointer to the first command inside this script.
/// @param[in]  write   Function that will be used to write the result.
/// @param[in]  cookie  Last argument to the above function.
static FDEC(translate_script, const CommandNode *const node)
{
  FUNCTION_START;
  do {
    const TranslationOptions o = kTransScript;

    // FIXME Add <SID>
    WS("vim = require 'vim'\n"
       "s = vim.new_script_scope(state, false)\n"
       "return {\n"
       "  run=function(state)\n"
       "    state = vim.state.enter_script(state, s)\n");

    F(translate_nodes, node, 2);

    WS("  end\n"
      "}\n");
  } while (0);
  FUNCTION_END;
}

/// Dump command executed from user input as code that runs immediately
///
/// @param[in]  node    Pointer to the first command inside this script.
/// @param[in]  write   Function that will be used to write the result.
/// @param[in]  cookie  Last argument to the above function.
static FDEC(translate_input, const CommandNode *const node)
{
  FUNCTION_START;
  do {
    const TranslationOptions o = kTransUser;

    WS("local state = vim.state.get_top()\n");
    F(translate_nodes, node, 0);
  } while (0);
  FUNCTION_END;
}
#endif  // NVIM_VIML_TRANSLATOR_TRANSLATOR_C_H
