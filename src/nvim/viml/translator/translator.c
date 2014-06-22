#define __STDC_LIMIT_MACROS
#define __STDC_FORMAT_MACROS
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdint.h>
#undef __STDC_LIMIT_MACROS
#undef __STDC_FORMAT_MACROS

#include "nvim/vim.h"
#include "nvim/strings.h"
#include "nvim/mbyte.h"
#include "nvim/charset.h"
#include "nvim/keymap.h"

#include "nvim/viml/translator/translator.h"
#include "nvim/viml/parser/expressions.h"
#include "nvim/viml/parser/ex_commands.h"
#include "nvim/viml/parser/command_definitions.h"
#include "nvim/viml/dumpers/dumpers.h"

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

/// Write string of the given length
///
/// @return Number of bytes written.
#define RAW_WRITE(string, len) write(string, 1, len, cookie)

/// Returns function declaration with additional arguments
///
/// Additional arguments are writer function, its last argument and translation 
/// options.
#define WFDEC(f, ...) int f(__VA_ARGS__, Writer write, void *cookie, \
                            TranslationOptions to)
/// Call function with additional arguments, propagate FAIL
#define CALL(f, ...) \
    { \
      if (f(__VA_ARGS__, write, cookie, to) == FAIL) \
        return FAIL; \
    }
/// Call function with additional arguments, goto error_label if it failed
#define CALL_ERR(error_label, f, ...) \
    { \
      if (f(__VA_ARGS__, write, cookie, to) == FAIL) \
        goto error_label; \
    }
/// Same as CALL, but with different translation options
///
/// Translation options used define translated function as being :function 
/// definition contents
#define CALL_FUNC(f, ...) \
    { \
      TranslationOptions to = kTransFunc; \
      CALL(f, __VA_ARGS__) \
    }
/// Same as CALL_ERR, but with different translation options
///
/// Translation options used define translated function as being :function 
/// definition contents
#define CALL_FUNC_ERR(error_label, f, ...) \
    { \
      TranslationOptions to = kTransFunc; \
      CALL_ERR(error_label, f, __VA_ARGS__) \
    }
/// Same as CALL, but with different translation options
///
/// Translation options used define translated function as being .vim file 
/// contents
#define CALL_SCRIPT(f, ...) \
    { \
      TranslationOptions to = kTransScript; \
      CALL(f, __VA_ARGS__) \
    }
/// Same as CALL_ERR, but with different translation options
///
/// Translation options used define translated function as being .vim file 
/// contents
#define CALL_SCRIPT_ERR(error_label, f, ...) \
    { \
      TranslationOptions to = kTransScript; \
      CALL_ERR(error_label, f, __VA_ARGS__) \
    }
/// Write string with given length, propagate FAIL
#define W_LEN(s, len) \
    CALL(raw_dump_string_len, (char *) s, len)
/// Write string with given length, goto error_label in case of failure
#define W_LEN_ERR(error_label, s, len) \
    CALL_ERR(error_label, raw_dump_string_len, (char *) s, len)
/// Write NUL-terminated string, propagate FAIL
#define W(s) \
    W_LEN(s, STRLEN(s))
/// Write NUL-terminated string, goto error_label in case of failure
#define W_ERR(error_label, s) \
    W_LEN_ERR(error_label, s, STRLEN(s))
/// Write string given its start and end, propagate FAIL
#define W_END(s, e) \
    W_LEN(s, e - s + 1)
/// Write string given its start and end, goto error_label in case of failure
#define W_END_ERR(error_label, s, e) \
    W_LEN_ERR(error_label, s, e - s + 1)
/// Write ExpressionNode, propagate FAIL
///
/// Data written is string between expr->position and expr->end_position, 
/// inclusive.
#define W_EXPR_POS(expr) \
    W_END(expr->position, expr->end_position)
/// Write ExpressionNode, goto error_label in case of failure
///
/// Data written is string between expr->position and expr->end_position, 
/// inclusive.
#define W_EXPR_POS_ERR(error_label, expr) \
    W_END_ERR(error_label, expr->position, expr->end_position)
/// Same as W_EXPR_POS, but written string is escaped
#define W_EXPR_POS_ESCAPED(expr) \
    CALL(dump_string_len, expr->position, \
         expr->end_position - expr->position + 1)
/// Same as W_EXPR_POS_ERR, but written string is escaped
#define W_EXPR_POS_ESCAPED_ERR(error_label, expr) \
    CALL_ERR(error_label, dump_string_len, expr->position, \
             expr->end_position - expr->position + 1)
/// Write static string
///
/// Same as W, but string length is determined at compile time
#define WS(s) \
    W_LEN(s, sizeof(s) - 1)
/// Write static string
///
/// Same as W_ERR, but string length is determined at compile time
#define WS_ERR(error_label, s) \
    W_LEN_ERR(error_label, s, sizeof(s) - 1)
/// Write indentation, indentation level is indent, propagate FAIL
#define WINDENT(indent) \
    { \
      for (size_t i = 0; i < indent; i++) \
        WS("  ") \
    }
/// Write indentation as with WINDENT, but goto error_label in case of failure
#define WINDENT_ERR(error_label, indent) \
    { \
      for (size_t i = 0; i < indent; i++) \
        WS_ERR(error_label, "  ") \
    }

/// Create variable name based on indentation level
///
/// Created name is prefix concatenated with indentation level.
#define INDENTVAR(var, prefix) \
    char var[SIZETBUFLEN + sizeof(prefix) - 1]; \
    size_t var##_len; \
    if ((var##_len = snprintf(var, SIZETBUFLEN + sizeof(prefix) - 1, \
                              prefix "%zu", indent)) <= 1) \
      return FAIL;
/// Write previously defined indentation variable name
#define ADDINDENTVAR(var) \
    W_LEN(var, var##_len)
/// Emit definitions for holding indentation variable inside a structure
#define INDENTVARDEFS(var) \
    char *var; \
    size_t var##_len;
/// Emit arguments for {} structure initializer
///
/// It is supposed that corresponding fields in structure were defined with the 
/// above INDENTVARDEFS macros.
#define INDENTVARARGS(var) \
    var, var##_len

/// Get translation context
///
/// See documentation for TranslationOptions for more details.
#define TRANSLATION_CONTEXT to


#define VIM_ZERO  "vim.zero_"
#define VIM_FALSE "vim.false_"
#define VIM_TRUE  "vim.true_"
#define VIM_EMPTY_STRING "vim.empty_string"

/// Lists possible translation context
typedef enum {
  kTransUser = 0,  ///< Typed Ex command argument
  kTransScript,    ///< .vim file
  kTransFunc,      ///< :function definition
} TranslationOptions;

typedef WFDEC((*AssignmentValueDump), const void *const dump_cookie);

typedef struct {
  const CommandNode *node;
  const size_t indent;
} TranslateFuncArgs;

typedef struct {
  size_t idx;
  INDENTVARDEFS(var)
} LetListItemAssArgs;

typedef struct {
  INDENTVARDEFS(var)
} IndentVarAssArgs;

typedef enum {
  kOptDefault = 0,
  kOptLocal,
  kOptGlobal,
} OptionType;

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "viml/translator/translator.c.generated.h"
#endif

/// Write string with the given length
///
/// @param[in]  s    String that will be written.
/// @param[in]  len  Length of this string.
static WFDEC(raw_dump_string_len, const char *const s, size_t len)
{
  return write_string_len(s, len, write, cookie);
}

/// Dump number that is not a vim Number
///
/// Use translate_number to dump vim Numbers (kExpr*Number)
///
/// @param[in]  number  Number that will be written.
static WFDEC(dump_number, intmax_t number)
{
  char buf[MAXNUMBUFLEN];
  size_t i;

  if ((i = snprintf(buf, MAXNUMBUFLEN, "%" PRIiMAX, number)) == 0)
    return FAIL;

  W_LEN(buf, i)

  return OK;
}

/// Dump one character
///
/// @param[in]  c  Dumped character
static WFDEC(dump_char, char_u c)
{
  switch (c) {
#define CHAR(c, s) \
    case c: { \
      WS("\\" s) \
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
    CHAR(VTAB, "v")
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
      W_LEN(&c, 1)
      break;
    }
  }
  return OK;
}

/// Dump string that is not a vim String
///
/// Use translate_string to dump vim String (kExpr*String)
///
/// @param[in]  s    String that will be written.
/// @param[in]  len  Length of this string.
static WFDEC(dump_string_len, const char_u *const s, size_t len)
{
  const char_u *p;
  const char_u *const e = s + len;
  WS("'")
  for (p = s; p < e; p++) {
    size_t charlen = mb_ptr2len((char_u *) p);
    if (charlen == 1) {
      CALL(dump_char, *p);
    } else {
      W_LEN(p, charlen);
      p += charlen - 1;
    }
  }
  WS("'")
  return OK;
}

/// Dump string that is not a vim String
///
/// Use translate_string to dump vim String (kExpr*String)
///
/// @param[in]  s  NUL-terminated string that will be written.
static WFDEC(dump_string, const char_u *const s)
{
  CALL(dump_string_len, s, STRLEN(s))
  return OK;
}

/// Dump boolean value
///
/// Writes true or false depending on its argument
///
/// @param[in]  b  Checked value.
static WFDEC(dump_bool, bool b)
{
  if (b)
    WS("true")
  else
    WS("false")

  return OK;
}

/// Dump regular expression
///
/// @param[in]  regex  Regular expression that will be dumped.
static WFDEC(translate_regex, const Regex *const regex)
{
  CALL(dump_string, regex->string)
  return OK;
}

/// Dump address followup
///
/// @param[in]  followup  Address followup that will be dumped.
static WFDEC(translate_address_followup, const AddressFollowup *const followup)
{
  // FIXME Replace magic numbers with constants
  switch (followup->type) {
    case kAddressFollowupShift: {
      WS("0, ")
      CALL(dump_number, (intmax_t) followup->data.shift)
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
      assert(false);
    }
  }
  return OK;
}

/// Dump Ex command range
///
/// @param[in]  range  Range that will be dumped.
static WFDEC(translate_range, const Range *const range)
{
  const Range *current_range = range;

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
        CALL(dump_number, (intmax_t) current_range->address.data.lnr);
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
        const char_u mark[2] = {current_range->address.data.mark, NUL};

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
        assert(false);
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

/// Dump Ex command flags
///
/// @param[in]  exflags  Flags to dump.
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

/// Dump VimL integer number
///
/// @param[in]  type  Type of the number node being dumped.
/// @param[in]  s     Pointer to first character in dumped number.
/// @param[in]  e     Pointer to last character in dumped number.
static WFDEC(translate_number, ExpressionType type, const char_u *s,
             const char_u *const e)
{
  WS("vim.number.new(state, ")
  switch (type) {
    case kExprHexNumber:
    case kExprDecimalNumber: {
      W_END(s, e)
      break;
    }
    case kExprOctalNumber: {
#define MAXOCTALNUMBUFLEN (sizeof(intmax_t) * 8 / 3 + 1)
      intmax_t number;
      char num_s[MAXOCTALNUMBUFLEN + 1];
      int n;

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

      if ((n = sscanf(num_s, "%lo", &number)) != (int) (e - s + 1)) {
        // TODO: check errno
        // TODO: give error message
        return FAIL;
      }
      CALL(dump_number, number)
      break;
#undef MAXOCTALNUMBUFLEN
    }
    default: {
      assert(false);
    }
  }
  WS(")")
  return OK;
}

/// Dump VimL string
///
/// @param[in]  type  Type of the string node being dumped.
/// @param[in]  s     Pointer to first character in dumped string.
/// @param[in]  e     Pointer to last character in dumped string.
static WFDEC(translate_string, ExpressionType type, const char_u *const s,
             const char_u *const e)
{
  WS("vim.string.new(state, ")
  bool can_dump_as_is = true;
  switch (type) {
    case kExprSingleQuotedString: {
      for (const char_u *p = s + 1; p < e; p++) {
        if (*p == '\'' || *p < 0x20) {
          can_dump_as_is = false;
          break;
        }
      }
      if (can_dump_as_is) {
        W_END(s, e)
      } else {
        assert(e > s);

        WS("'")
        for (const char_u *p = s + 1; p < e; p++) {
          if (*p == '\'') {
            WS("\\'")
            p++;
          } else {
            W_LEN(p, 1)
          }
        }
        WS("'")
        break;
      }
      break;
    }
    case kExprDoubleQuotedString: {
      for (const char_u *p = s + 1; p < e; p++) {
        if (*p < 0x20) {
          can_dump_as_is = false;
          break;
        }
        if (*p == '\\') {
          p++;
          switch (*p) {
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
          if (!can_dump_as_is)
            break;
        }
      }
      if (can_dump_as_is) {
        W_END(s, e)
      } else {
        assert(e > s);

        WS("\"")
        for (const char_u *p = s + 1; p < e; p++) {
          switch (*p) {
            case '\\': {
              p++;
              switch (*p) {
                case 'r':
                case 'n':
                case 'f':
                case 'b':
                case '\\':
                case '\"': {
                  W_LEN(p - 1, 2)
                  break;
                }
                case 'e': {
                  WS("\\027")
                  break;
                }
                case 'x':
                case 'X':
                case 'u':
                case 'U': {
                  if (vim_isxdigit(p[1])) {
                    uint_least8_t n;
                    uint_least32_t nr = 0;
                    bool isx = (*p == 'X' || *p == 'x');

                    if (isx)
                      n = 2;
                    else
                      n = 4;

                    while (--n >= 0 && vim_isxdigit(p[1])) {
                      p++;
                      nr = (nr << 4) + hex2nr(*p);
                    }
                    p++;
                    if (isx || nr < 0x7F) {
                      CALL(dump_char, nr)
                    } else {
                      char_u buf[MAX_CHAR_LEN];
                      size_t len;

                      len = (*mb_char2bytes)(nr, buf);
                      W_LEN(buf, len)
                    }
                  } else {
                    CALL(dump_char, *p)
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
                  char_u c;
                  c = *p++ - '0';
                  if ('0' <= *p && *p <= '7') {
                    c = (c << 3) + (*p++ - '0');
                    if ('0' <= *p && *p <= '7')
                      c = (c << 3) + (*p++ - '0');
                  }
                  CALL(dump_char, c)
                  break;
                }
                case '<': {
                  char_u buf[MAX_CHAR_LEN * 6];
                  size_t len;

                  len = trans_special((char_u **) &p, buf, false);

                  for (size_t i = 0; i < len; i++)
                    CALL(dump_char, buf[i]);

                  break;
                }
                default: {
                  CALL(dump_char, *p)
                  break;
                }
              }
              break;
            }
            default: {
              CALL(dump_char, *p)
              break;
            }
          }
        }
        WS("\"")
      }
      break;
    }
    default: {
      assert(false);
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
///
/// @return FAIL in case of unrecoverable error, OK otherwise.
static WFDEC(translate_scope, const char_u **start,
                              const ExpressionNode *const expr,
                              const uint_least8_t flags)
{
  assert(expr->type == kExprSimpleVariableName ||
         (expr->type == kExprIdentifier && !(flags&TS_ONLY_SEGMENT)));
  if (expr->end_position == expr->position) {
    if (!(flags & (TS_LAST_SEGMENT|TS_ONLY_SEGMENT))
        && vim_strchr((char_u *) "svalgtwb", *(expr->position)) != NULL) {
      *start = NULL;
    } else {
      *start = expr->position;
      if ((flags & TS_FUNCCALL) && ASCII_ISLOWER(*expr->position))
        WS("vim.functions")
      else if (flags & (TS_FUNCASSIGN|TS_FUNCCALL))
        WS("state.global.user_functions")
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
        if (*expr->position == 'g'
            || *expr->position == 'v')
          WS("global.")
        W_LEN(expr->position, 1)
        break;
      }
      case 't': {
        *start = expr->position + 2;
        WS("state.global.tabpage.t")
        break;
      }
      case 'w': {
        *start = expr->position + 2;
        WS("state.global.window.w")
        break;
      }
      case 'b': {
        *start = expr->position + 2;
        WS("state.global.buffer.b")
        break;
      }
      default: {
        *start = expr->position;
        if (flags & (TS_FUNCASSIGN|TS_FUNCCALL))
          WS("state.global.user_functions")
        else
          WS("state.current_scope")
        break;
      }
    }
  } else {
    bool isfunc = false;

    *start = expr->position;
    if ((flags & TS_FUNCCALL) && ASCII_ISLOWER(*expr->position)) {
      isfunc = true;
      const char_u *s;
      for (s = expr->position + 1; s != expr->end_position; s++) {
        if (!(ASCII_ISLOWER(*s) || VIM_ISDIGIT(*s))) {
          isfunc = false;
          break;
        }
      }
    }
    if (isfunc && !(flags & TS_FUNCASSIGN))
      WS("vim.functions")
    else if (flags & (TS_FUNCASSIGN|TS_FUNCCALL))
      WS("state.global.user_functions")
    else
      WS("state.current_scope")
  }
  return OK;
}

/// Dump parsed VimL expression
///
/// @param[in]  expr  Expression being dumped.
static WFDEC(translate_expr, const ExpressionNode *const expr,
                             const bool is_funccall)
{
  switch (expr->type) {
    case kExprListRest: {
      // TODO Add this error in expr parser, not here
      WS("vim.err.err(state, nil, true, \"E696: Missing comma in list\")")
      break;
    }
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
    case kExprDoubleQuotedString:
    case kExprSingleQuotedString: {
      CALL(translate_string, expr->type, expr->position, expr->end_position)
      break;
    }
    case kExprOption: {
      const char_u *name_start;
      OptionType type;
      uint_least8_t option_properties;
      const char_u *s = expr->position;
      const char_u *e = expr->end_position;

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
            WS(VIM_FALSE)
            break;
          }
          case GOP_NUMERIC: {
            WS(VIM_ZERO)
            break;
          }
          case GOP_STRING: {
            WS(VIM_EMPTY_STRING)
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
          WS("state.global.options['")
          W_END(name_start, e)
          WS("']")
        } else {
          if (option_properties & GOP_GLOBAL)
            WS("vim.get_local_option(state, ")

          if (option_properties & GOP_BUFFER_LOCAL)
            WS("state.global.buffer")
          else if (option_properties & GOP_WINDOW_LOCAL)
            WS("state.global.window")
          else
            assert(false);

          if (option_properties & GOP_GLOBAL)
            WS(", ")
          else
            WS("[")
          WS("'")
          W_END(name_start, e)
          WS("'")
          if (option_properties & GOP_GLOBAL)
            WS(")")
          else
            WS("]")
        }
      }
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
      const char_u *start;
      WS("vim.subscript.subscript(state, ")
      CALL(translate_scope, &start, expr, TS_ONLY_SEGMENT | (is_funccall
                                                             ? TS_FUNCCALL
                                                             : 0))
      assert(start != NULL);
      WS(", '")
      W_END(start, expr->end_position)
      WS("')")
      break;
    }
    case kExprVariableName: {
      WS("vim.subscript.subscript(state, ")
      CALL(translate_varname, expr, FALSE)
      WS(")")
      break;
    }
    case kExprCurlyName:
    case kExprIdentifier: {
      // Should have been handled by translate_varname above
      assert(FALSE);
    }
    case kExprConcatOrSubscript: {
      WS("vim.concat_or_subscript(state, ")
      W_EXPR_POS_ESCAPED(expr)
      WS(", ")
      CALL(translate_expr, expr->children, false)
      WS(")")
      break;
    }
    case kExprEmptySubscript: {
      WS("nil")
      break;
    }
    case kExprExpression: {
      WS("(")
      CALL(translate_expr, expr->children, false)
      WS(")")
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
          WS("vim.dict.new(state")
          break;
        }
        case kExprList: {
          WS("vim.list.new(state")
          break;
        }
        case kExprSubscript: {
          if (expr->children->next->next == NULL) {
            if (is_funccall)
              WS("vim.subscript.func(state")
            else
              WS("vim.subscript.subscript(state")
          } else {
            WS("vim.subscript.slice(state")
          }
          break;
        }

#define OPERATOR(op_type, op) \
        case op_type: { \
          WS("vim." op "(state") \
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
            WS("vim.negate_logical(state, ") \
          } \
          WS("vim.op." op "(state") \
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
          assert(false);
        }
      }

      current_expr = expr->children;
      if (expr->type == kExprCall) {
        WS(", ")
        CALL(translate_expr, current_expr, true)
        current_expr = current_expr->next;
      }
      for (; current_expr != NULL; current_expr=current_expr->next) {
        WS(", ")
        CALL(translate_expr, current_expr, false)
      }
      WS(")")

      if (reversed)
        WS(")")

      break;
    }
  }
  return OK;
}

/// Dump a sequence of VimL expressions (e.g. for :echo 1 2 3)
///
/// @param[in]  expr  Pointer to the first expression that will be dumped.
static WFDEC(translate_exprs, const ExpressionNode *const expr)
{
  const ExpressionNode *current_expr = expr;

  for (;;) {
    CALL(translate_expr, current_expr, false)
    current_expr = current_expr->next;
    if (current_expr == NULL)
      break;
    else
      WS(", ")
  }

  return OK;
}

/// Dump a VimL function definition
///
/// @param[in]  args  Function node and indentation that was used for function 
///                   definition.
static WFDEC(translate_function, const TranslateFuncArgs *const args)
{
  size_t i;
  const char_u **data =
      (const char_u **) args->node->args[ARG_FUNC_ARGS].arg.strs.ga_data;
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
    WS("state = vim.state.enter_function(state, {})\n")
    CALL_FUNC(translate_nodes, args->node->children, args->indent + 1)
  } else {
    // Empty function: do not bother creating scope dictionaries, just return 
    // zero
    WINDENT(args->indent + 1)
    WS("return " VIM_ZERO "\n")
  }
  WINDENT(args->indent)
  WS("end")
  return OK;
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
static WFDEC(translate_varname, const ExpressionNode *const expr,
                                const bool is_funccall)
{
  const ExpressionNode *current_expr = expr->children;
  bool add_concat;
  bool close_parenthesis = false;

  assert(expr->type == kExprVariableName);
  assert(current_expr != NULL);

  if (current_expr->type == kExprIdentifier) {
    const char_u *start;
    CALL(translate_scope, &start, current_expr, (is_funccall
                                                 ? TS_FUNCASSIGN
                                                 : 0))
    if (start == NULL) {

      WS("vim.get_scope_and_key(state, vim.concat(state, '")
      W_EXPR_POS(current_expr)
      WS("'")
      close_parenthesis = true;
    } else {
      WS(", vim.concat(state, '")
      W_END(start, current_expr->end_position)
      WS("'")
    }
    add_concat = true;
  } else {
    WS("vim.get_scope_and_key(state, vim.concat(state, ")
    add_concat = false;
    close_parenthesis = true;
  }

  for (current_expr = current_expr->next; current_expr != NULL;
        current_expr = current_expr->next) {
    if (add_concat)
      WS(", ")
    else
      add_concat = true;
    switch (current_expr->type) {
      case kExprIdentifier: {
        WS("'")
        W_EXPR_POS(current_expr)
        WS("'")
        break;
      }
      case kExprCurlyName: {
        CALL(translate_expr, current_expr->children, false)
        break;
      }
      default: {
        assert(false);
      }
    }
  }

  WS(")")

  if (close_parenthesis)
    WS(")")

  return OK;
}

/// Translate lvalue into one of vim.assign.* calls
///
/// @note Newline is not written.
///
/// @param[in]  expr         Translated expression.
/// @param[in]  is_funccall  True if it translates :function definition.
/// @param[in]  unique       True if assignment must be unique.
/// @param[in]  dump         Function used to dump value that will be assigned.
/// @param[in]  dump_cookie  First argument to the above function.
///
/// @return FAIL in case of unrecoverable error, OK otherwise.
static WFDEC(translate_lval, const ExpressionNode *const expr,
                             const bool is_funccall, const bool unique,
                             const AssignmentValueDump dump,
                             const void *const dump_cookie)
{
#define ADD_ASSIGN(what) \
  { \
    if (is_funccall) { \
      WS("vim.assign." what "_function(state, ") \
      CALL(dump_bool, unique) \
      WS(", ") \
    } else { \
      WS("vim.assign." what "(state, ") \
    } \
  }
  switch (expr->type) {
    case kExprSimpleVariableName: {
      const char_u *start;
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
      const ExpressionNode *current_expr = expr->children;

      assert(current_expr != NULL);

      ADD_ASSIGN("scope")
      CALL(dump, dump_cookie)
      WS(", ")

      CALL(translate_varname, expr, is_funccall)

      WS(")")
      break;
    }
    case kExprConcatOrSubscript: {
      ADD_ASSIGN("dict")
      CALL(dump, dump_cookie)
      WS(", ")
      CALL(translate_expr, expr->children, false)
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
      CALL(translate_expr, expr->children, false)
      WS(", ")
      CALL(translate_expr, expr->children->next, false)
      if (expr->children->next->next != NULL) {
        WS(", ")
        CALL(translate_expr, expr->children->next->next, false)
      }
      WS(")")
      break;
    }
    default: {
      assert(false);
    }
  }
  return OK;
#undef ADD_ASSIGN
}

/// Helper function that dumps list element
///
/// List is located in indentvar described in args.
///
/// Indentvar is a variable with name based on indentation level. It is used to 
/// make variable name unique in some scope.
///
/// @param[in]  args  Indentvar description and number of the dumped element.
static WFDEC(translate_let_list_item, const LetListItemAssArgs *const args)
{
  WS("vim.list.raw_subscript(")
  ADDINDENTVAR(args->var)
  WS(", ")
  CALL(dump_number, (long) args->idx)
  WS(")")
  return OK;
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
static WFDEC(translate_let_list_rest, const LetListItemAssArgs *const args)
{
  WS("vim.list.slice_to_end(state, ")
  ADDINDENTVAR(args->var)
  WS(", ")
  CALL(dump_number, (long) args->idx)
  WS(")")
  return OK;
}

/// Helper functions that dumps expression
///
/// Proxy to translate_expr without the second argument.
///
/// @param[in]  expr  Dumped expression.
static WFDEC(translate_rval_expr, const ExpressionNode *const expr)
{
  CALL(translate_expr, expr, false)
  return OK;
}

/// Helper function for dumping rvalue indentvar
///
/// Indentvar is a variable with name based on indentation level. It is used to 
/// make variable name unique in some scope.
///
/// @param[in]  args  Indentvar description.
static WFDEC(dump_indentvar, const IndentVarAssArgs *const args)
{
  ADDINDENTVAR(args->var)
  return OK;
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
static WFDEC(translate_assignment, const ExpressionNode *const lval_expr,
                                   const size_t indent,
                                   const char *const err_line,
                                   AssignmentValueDump dump,
                                   const void *const dump_cookie)
{
#define ADD_ASSIGN(expr, err_line, indent, dump, dump_cookie) \
      if (err_line != NULL) \
        WS("if ") \
      CALL(translate_lval, expr, false, false, dump, dump_cookie) \
      if (err_line != NULL) { \
        WS(" == nil then\n") \
        WINDENT(indent + 1) \
        W(err_line) \
        WS("\n") \
        WINDENT(indent) \
        WS("end\n") \
      } else { \
        WS("\n") \
      }
  if (lval_expr->type == kExprList) {
    bool has_rest = false;
    size_t val_num = 0;
    const ExpressionNode *current_expr;

    INDENTVAR(rhs_var, "rhs")
    WS("local ")
    ADDINDENTVAR(rhs_var)
    WS(" = ")
    CALL(dump, dump_cookie)
    WS("\n")

    current_expr = lval_expr->children;
    for (; current_expr != NULL; current_expr = current_expr->next)
      if (current_expr->type == kExprListRest)
        has_rest = true;
      else
        val_num++;

    assert(val_num > 0);

    WINDENT(indent)
    WS("if (vim.type(state, ")
    ADDINDENTVAR(rhs_var)
    WS(") == vim.VIM_LIST) then\n")

    WINDENT(indent + 1)
    WS("if (vim.list.length(state, ")
    ADDINDENTVAR(rhs_var)
    WS(")")
    if (has_rest)
      WS(" >= ")
    else
      WS(" == ")
    CALL(dump_number, (long) val_num)
    WS(") then\n")

    current_expr = lval_expr->children;
    for (size_t i = 0; i < val_num; i++) {
      LetListItemAssArgs args = {
        i,
        INDENTVARARGS(rhs_var)
      };
      WINDENT(indent + 2)
      ADD_ASSIGN(current_expr, err_line, indent + 2,
                 (AssignmentValueDump) (&translate_let_list_item), &args)
      current_expr = current_expr->next;
    }
    if (has_rest) {
      LetListItemAssArgs args = {
        val_num,
        INDENTVARARGS(rhs_var)
      };
      WINDENT(indent + 2)
      ADD_ASSIGN(current_expr->children, err_line, indent + 2,
                 (AssignmentValueDump) (&translate_let_list_rest), &args)
    }

    WINDENT(indent + 1)
    WS("else\n")
    WINDENT(indent + 2)
    if (!has_rest) {
      WS("if (vim.list.length(state, ")
      ADDINDENTVAR(rhs_var)
      WS(") > ")
      CALL(dump_number, (long) val_num)
      WS(") then\n")
      WINDENT(indent + 3)
    }
    // TODO Dump position
    WS("vim.err.err(state, nil, true, "
        "\"E688: More targets than List items\")\n")
    if (!has_rest) {
      WINDENT(indent + 2)
      WS("else\n")
      WINDENT(indent + 3)
      WS("vim.err.err(state, nil, true, "
          "\"E687: Less targets than List items\")\n")
      WINDENT(indent + 2)
      WS("end\n")
    }
    if (err_line != NULL) {
      WINDENT(indent + 2)
      W(err_line)
      WS("\n")
    }
    WINDENT(indent + 1)
    WS("end\n")

    WINDENT(indent)
    WS("else\n")
    WINDENT(indent + 1)
    WS("vim.err.err(state, nil, true, \"E714: List required\")\n")
    if (err_line != NULL) {
      WINDENT(indent + 1)
      W(err_line)
      WS("\n")
    }
    WINDENT(indent)
    WS("end\n")
  } else {
    ADD_ASSIGN(lval_expr, err_line, indent, dump, dump_cookie)
  }
  return OK;
#undef ADD_ASSIGN
}

/// Dump VimL Ex command
///
/// @param[in]  node    Node to translate.
/// @param[in]  indent  Indentation of the result.
static WFDEC(translate_node, const CommandNode *const node,
                             const size_t indent)
{
  const char_u *name;
  size_t start_from_arg = 0;
  bool do_arg_dump = true;
  bool add_comma = false;

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
    WS("-- \"")
    W(node->args[ARG_NAME_NAME].arg.str)
    WS("\n")
    return OK;
  } else if (node->type == kCmdHashbangComment) {
    WS("-- #!")
    W(node->args[ARG_NAME_NAME].arg.str)
    WS("\n")
    return OK;
  } else if (node->type == kCmdSyntaxError) {
    WS("vim.err.err(state, ")
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
    const TranslateFuncArgs args = {node, indent};
    CALL(translate_lval, node->args[ARG_FUNC_NAME].arg.expr, true, !node->bang,
                         (AssignmentValueDump) &translate_function,
                         (void *) &args)
    WS("\n")
    return OK;
  } else if (node->type == kCmdFor) {
    INDENTVAR(iter_var, "i")
    WS("local ")
    ADDINDENTVAR(iter_var)
    WS("\n")

    WINDENT(indent)
    WS("for _, ")
    ADDINDENTVAR(iter_var)
    WS(" in vim.list.iterator(state, ")
    CALL(translate_expr, node->args[ARG_FOR_RHS].arg.expr, false)
    WS(") do\n")

    {
      IndentVarAssArgs args = {
        INDENTVARARGS(iter_var)
      };
      WINDENT(indent + 1)
      CALL(translate_assignment, node->args[ARG_FOR_LHS].arg.expr, indent + 1,
                                 "break",
                                 (AssignmentValueDump) (&dump_indentvar),
                                 &args)
    }

    CALL(translate_nodes, node->children, indent + 1)

    WINDENT(indent)
    WS("end\n")
    return OK;
  } else if (node->type == kCmdWhile) {
    WS("while vim.get_boolean(")
    CALL(translate_expr, node->args[ARG_EXPR_EXPR].arg.expr, false)
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
        CALL(translate_expr, node->args[ARG_EXPR_EXPR].arg.expr, false)
        WS(") then\n")
        break;
      }
      default: {
        assert(false);
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
          assert(false);
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
    WS("end, vim.state.enter_try(state))\n")

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
      bool did_first_if = false;

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
          WS("if (vim.err.matches(state, ")
          ADDINDENTVAR(err_var)
          WS(", ")
          CALL(translate_regex, next->args[ARG_REG_REG].arg.reg)
          WS(")\n")
          did_first_if = true;
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

    return OK;
  } else if (node->type == kCmdLet
             && node->args[ARG_LET_RHS].arg.expr != NULL) {
    const ExpressionNode *lval_expr = node->args[ARG_LET_LHS].arg.expr;
    const ExpressionNode *rval_expr = node->args[ARG_LET_RHS].arg.expr;
    CALL(translate_assignment, lval_expr, indent, NULL,
                               (AssignmentValueDump) &translate_rval_expr,
                               (void *) rval_expr)
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
    add_comma = true;
  }

  if (CMDDEF(node->type).flags & BANG) {
    if (add_comma)
      WS(", ")
    CALL(dump_bool, node->bang)
    add_comma = true;
  }

  if (CMDDEF(node->type).flags & EXFLAGS) {
    if (add_comma)
      WS(", ")
    CALL(translate_ex_flags, node->exflags)
    add_comma = true;
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
      add_comma = false;
      switch (CMDDEF(node->type).arg_types[i]) {
        case kArgExpression: {
          CALL(translate_expr, node->args[i].arg.expr, false)
          add_comma = true;
          break;
        }
        case kArgExpressions: {
          CALL(translate_exprs, node->args[i].arg.expr)
          add_comma = true;
          break;
        }
        case kArgString: {
          CALL(dump_string, node->args[i].arg.str)
          add_comma = true;
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


/// Dump a sequence of Ex commands
///
/// @param[in]  node    Pointer to the first node that will be translated.
/// @param[in]  indent  Indentation of the result.
static WFDEC(translate_nodes, const CommandNode *const node, size_t indent)
{
  const CommandNode *current_node;

  for (current_node = node; current_node != NULL;
       current_node = current_node->next) {
    switch (current_node->type) {
      case kCmdFinish: {
        switch (TRANSLATION_CONTEXT) {
          case kTransFunc:
          case kTransUser: {
            WINDENT(indent)
            // TODO dump error position
            WS("vim.err.err(state, nil, true, "
               "\"E168: :finish used outside of a sourced file\""
               ")\n")
            continue;
          }
          case kTransScript: {
            WINDENT(indent)
            WS("return nil\n")
            break;
          }
        }
        break;
      }
      case kCmdReturn: {
        switch (TRANSLATION_CONTEXT) {
          case kTransScript:
          case kTransUser: {
            WINDENT(indent)
            // TODO dump error position
            WS("vim.err.err(state, nil, true, "
               "\"E133: :return not inside a function\""
               ")\n")
            continue;
          }
          case kTransFunc: {
            WINDENT(indent)
            WS("return ")
            CALL(translate_expr, node->args[ARG_EXPR_EXPR].arg.expr, false)
            WS("\n")
            break;
          }
        }
        break;
      }
      default: {
        CALL(translate_node, current_node, indent)
        continue;
      }
    }
    break;
  }

  if (current_node == NULL && TRANSLATION_CONTEXT == kTransFunc) {
    WINDENT(indent)
    WS("return " VIM_ZERO "\n")
  }

  return OK;
}

/// Dump .vim script as lua module.
///
/// @param[in]  node    Pointer to the first command inside this script.
/// @param[in]  write   Function that will be used to write the result.
/// @param[in]  cookie  Last argument to the above function.
///
/// @return OK in case of success, FAIL otherwise.
int translate_script(const CommandNode *const node, Writer write, void *cookie)
{
  TranslationOptions to = kTransScript;

  // FIXME Add <SID>
  WS("vim = require 'vim'\n"
     "s = vim.new_script_scope(state, false)\n"
     "return {\n"
     "  run=function(state)\n"
     "    state = vim.state.enter_script(state, s)\n")

  CALL_SCRIPT(translate_nodes, node, 2)

  WS("  end\n"
    "}\n")
  return OK;
}

/// Dump command executed from user input as code that runs immediately
///
/// @param[in]  node    Pointer to the first command inside this script.
/// @param[in]  write   Function that will be used to write the result.
/// @param[in]  cookie  Last argument to the above function.
///
/// @return OK in case of success, FAIL otherwise.
int translate_input(const CommandNode *const node, Writer write, void *cookie)
{
  TranslationOptions to = kTransUser;

  WS("state = vim.state\n")
  CALL_SCRIPT(translate_nodes, node, 0)

  return OK;
}
