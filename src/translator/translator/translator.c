#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "vim.h"
#include "memory.h"
#include "misc2.h"
#include "mbyte.h"
#include "charset.h"
#include "keymap.h"

#include "translator/translator/translator.h"
#include "translator/parser/expressions.h"
#include "translator/parser/ex_commands.h"
#include "translator/parser/command_definitions.h"
#include "translator/printer/ex_commands.h"

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
    CALL(write_string_len, (char *) s, len)
/// Write string with given length, goto error_label in case of failure
#define W_LEN_ERR(error_label, s, len) \
    CALL_ERR(error_label, write_string_len, (char *) s, len)
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

typedef enum {
  kTransUser = 0,
  kTransScript,
  kTransFunc,
} TranslationOptions;

typedef WFDEC((*AssignmentValueDump), void *dump_cookie);
typedef struct {
  const CommandNode *node;
  const size_t indent;
} TranslateFuncArgs;

// {{{ Function declarations
static WFDEC(write_string_len, const char *const s, size_t len);
static WFDEC(dump_number, long number);
static WFDEC(dump_char, char_u c);
static WFDEC(dump_string_len, const char_u *const s, size_t len);
static WFDEC(dump_string, const char_u *const s);
static WFDEC(dump_bool, bool b);
static WFDEC(translate_regex, const Regex *const regex);
static WFDEC(translate_address_followup, const AddressFollowup *const followup);
static WFDEC(translate_range, const Range *const range);
static WFDEC(translate_ex_flags, uint_least8_t exflags);
static WFDEC(translate_number, ExpressionType type, const char_u *const s,
             const char_u *const e);
static WFDEC(translate_scope, char_u **start, const ExpressionNode *const expr,
             uint_least8_t flags);
static WFDEC(translate_expr, const ExpressionNode *const expr);
static WFDEC(translate_exprs, ExpressionNode *const expr);
static WFDEC(translate_function, const TranslateFuncArgs *const args);
static WFDEC(translate_lval, ExpressionNode *expr, bool is_funccall,
                             bool unique,
                             AssignmentValueDump dump, void *dump_cookie);
static WFDEC(translate_node, const CommandNode *const node, size_t indent);
static WFDEC(translate_nodes, const CommandNode *const node, size_t indent);
static int translate_script_stdout(const CommandNode *const node);
static char_u *fgetline_file(int c, FILE *file, int indent);
// }}}

/// Write string with the given length
///
/// @param[in]  s    String that will be written.
/// @param[in]  len  Length of this string.
static WFDEC(write_string_len, const char *const s, size_t len)
{
  size_t written;
  if (len) {
    written = write(s, 1, len, cookie);
    if (written < len) {
      // TODO: generate error message
      return FAIL;
    }
  }
  return OK;
}

static Writer write_file = (Writer) &fwrite;

/// Dump number that is not a vim Number
///
/// Use translate_number to dump vim Numbers (kExpr*Number)
///
/// @param[in]  number  Number that will be written.
static WFDEC(dump_number, long number)
{
  char buf[NUMBUFLEN];

  if (sprintf(buf, "%ld", number) == 0)
    return FAIL;

  W(buf)

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
      CALL(dump_number, (long) followup->data.shift)
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
      assert(FALSE);
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
        CALL(dump_number, (long) current_range->address.data.lnr);
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
        char_u mark[2] = {current_range->address.data.mark, NUL};

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
        assert(FALSE);
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
static WFDEC(translate_number, ExpressionType type, const char_u *const s,
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
      unsigned long number;
      char *num_s;
      int n;

      // FIXME Remove unneeded allocation
      if ((num_s = (char *) vim_strnsave((char_u *) s, (e - s) + 1)) == NULL)
        return FAIL;

      if ((n = sscanf(num_s, "%lo", &number)) != (int) (e - s + 1)) {
        // TODO: check errno
        // TODO: give error message
        return FAIL;
      }
      CALL(dump_number, (long) number)
      break;
    }
    default: {
      assert(FALSE);
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
  bool can_dump_as_is = TRUE;
  switch (type) {
    case kExprSingleQuotedString: {
      for (const char_u *p = s + 1; p < e; p++) {
        if (*p == '\'' || *p < 0x20) {
          can_dump_as_is = FALSE;
          break;
        }
      }
      if (can_dump_as_is) {
        W_END(s, e)
      } else {
        assert(e > s);

        const size_t len = (e - s) + 1;
        char_u *const new = XMALLOC_NEW(char_u, len);

        memcpy(new, s, len);

        for (char_u *p = new + 1; p < new + len - 1; p++) {
          if (*p == '\'') {
            *p = '\\';
            p++;
          }
        }
        W_LEN_ERR(translate_string_sq_cant_err, new, len)
        vim_free(new);
        break;
translate_string_sq_cant_err:
        vim_free(new);
        return FAIL;
      }
      break;
    }
    case kExprDoubleQuotedString: {
      for (const char_u *p = s + 1; p < e; p++) {
        if (*p < 0x20) {
          can_dump_as_is = FALSE;
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
              can_dump_as_is = FALSE;
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

                  len = trans_special((char_u **) &p, buf, FALSE);

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
      assert(FALSE);
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
///       the current scope, if it is not it may be a construct like "a{':abc'}" 
///       which refers to "a:abc" variable.
///
///   TS_ONLY_SEGMENT
///   :   Determines whether this segment is the only one. Affects cases when 
///       "state.user_functions" is preferred over "state.current_scope".
///
///   TS_FUNCCALL
///   :   Use "state.functions" in place of "state.current_scope" in some cases. 
///       Note that vim.call implementation should still be able to use 
///       "state.user_functions" if appropriate.
///
///   TS_FUNCASSIGN
///   :   Use "state.user_functions" in place of "state.current_scope" in some 
///       cases.
/// @endparblock
///
/// @return FAIL in case of unrecoverable error, OK otherwise.
static WFDEC(translate_scope, char_u **start, const ExpressionNode *const expr,
             uint_least8_t flags)
{
  assert(expr->type == kExprSimpleVariableName);
  if (expr->end_position == expr->position) {
    if (!(flags & (TS_LAST_SEGMENT|TS_ONLY_SEGMENT))) {
      *start = NULL;
    } else {
      *start = expr->position;
      if ((flags & TS_FUNCCALL) && ASCII_ISLOWER(*expr->position))
        WS("state.functions")
      else if (flags & (TS_FUNCASSIGN|TS_ONLY_SEGMENT))
        WS("state.user_functions")
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
        W_LEN(expr->position, 1)
        break;
      }
      case 't': {
        *start = expr->position + 2;
        WS("state.tabpage.t")
        break;
      }
      case 'w': {
        *start = expr->position + 2;
        WS("state.window.w")
        break;
      }
      case 'b': {
        *start = expr->position + 2;
        WS("state.buffer.b")
        break;
      }
      default: {
        *start = expr->position;
        if (flags & (TS_FUNCASSIGN|TS_ONLY_SEGMENT))
          WS("state.user_functions")
        else
          WS("state.current_scope")
        break;
      }
    }
  } else {
    bool isfunc = FALSE;

    *start = expr->position;
    if ((flags & TS_FUNCCALL) && ASCII_ISLOWER(*expr->position)) {
      char_u *s;
      for (s = expr->position + 1; s != expr->end_position; s++) {
        if (!(ASCII_ISLOWER(*s) || VIM_ISDIGIT(*s))) {
          isfunc = TRUE;
          break;
        }
      }
    }
    if (isfunc && !(flags & TS_FUNCASSIGN))
      WS("state.functions")
    else if (flags & (TS_FUNCASSIGN|TS_ONLY_SEGMENT))
      WS("state.user_functions")
    else
      WS("state.current_scope")
  }
  return OK;
}

/// Dump parsed VimL expression
///
/// @param[in]  expr  Expression being dumped.
static WFDEC(translate_expr, const ExpressionNode *const expr)
{
  switch (expr->type) {
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
      // TODO
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
      char_u *start;
      WS("vim.subscript(state, ")
      CALL(translate_scope, &start, expr, TS_ONLY_SEGMENT)
      assert(start != NULL);
      WS(", '")
      W_END(start, expr->end_position)
      WS("')")
      break;
    }
    case kExprVariableName: {
      // TODO
      break;
    }
    case kExprCurlyName: {
      // TODO
      break;
    }
    case kExprIdentifier: {
      // TODO
      break;
    }
    case kExprConcatOrSubscript: {
      WS("vim.concat_or_subscript(state, ")
      W_EXPR_POS_ESCAPED(expr)
      WS(", ")
      CALL(translate_expr, expr->children)
      WS(")")
      break;
    }
    case kExprEmptySubscript: {
      WS("nil")
      break;
    }
    case kExprExpression: {
      WS("(")
      CALL(translate_expr, expr->children)
      WS(")")
      break;
    }
    default: {
      ExpressionNode *current_expr;
      bool reversed = FALSE;

      assert(expr->children != NULL
             || expr->type == kExprDictionary
             || expr->type == kExprList);
      switch (expr->type) {
        case kExprDictionary: {
          WS("vim.dict.new(state, ")
          break;
        }
        case kExprList: {
          WS("vim.list.new(state, ")
          break;
        }
        case kExprSubscript: {
          if (expr->children->next->next == NULL)
            WS("vim.subscript(state, ")
          else
            WS("vim.slice(state, ")
        }

#define OPERATOR(op_type, op) \
        case op_type: { \
          WS("vim." op "(state, ") \
          break; \
        }

        OPERATOR(kExprAdd,          "add")
        OPERATOR(kExprSubtract,     "subtract")
        OPERATOR(kExprDivide,       "divide")
        OPERATOR(kExprMultiply,     "multiply")
        OPERATOR(kExprModulo,       "modulo")
        OPERATOR(kExprCall,         "call")
        OPERATOR(kExprMinus,        "negate")
        OPERATOR(kExprNot,          "negate_logical")
        OPERATOR(kExprPlus,         "promote_integer")
        OPERATOR(kExprStringConcat, "concat")
#undef OPERATOR

#define COMPARISON(forward_type, rev_type, op) \
        case forward_type: \
        case rev_type: { \
          if (expr->type == rev_type) { \
            reversed = TRUE; \
            WS("(") \
          } \
          WS("vim." op "(state, ") \
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
          assert(FALSE);
        }
      }

      if (reversed)
        WS(" == 1 and 0 or 1)")

      current_expr = expr->children;
      for (;;) {
        CALL(translate_expr, current_expr)
        current_expr = current_expr->next;
        if (current_expr == NULL)
          break;
        else
          WS(", ")
      }
      WS(")")
      break;
    }
  }
  return OK;
}

/// Dump a sequence of VimL expressions (e.g. for :echo 1 2 3)
///
/// @param[in]  expr  Pointer to the first expression that will be dumped.
static WFDEC(translate_exprs, ExpressionNode *const expr)
{
  ExpressionNode *current_expr = expr;

  for (;;) {
    CALL(translate_expr, current_expr)
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
  char_u **data = (char_u **) args->node->args[ARG_FUNC_ARGS].arg.strs.ga_data;
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
    WS("state = state:enter_function({})\n")
    CALL_FUNC(translate_nodes, args->node->children, args->indent + 1)
  } else {
    // Empty function: do not bother creating scope dictionaries, just return 
    // zero
    WINDENT(args->indent + 1)
    WS("return vim.number.new(state, 0)\n")
  }
  WINDENT(args->indent)
  WS("end")
  return OK;
}

/// Translate lvalue into one of vim.assign_* calls
///
/// @param[in]  expr         Translated expression.
/// @param[in]  is_funccall  True if it translates :function definition.
/// @param[in]  unique       True if assignment must be unique.
/// @param[in]  dump         Function used to dump value that will be assigned.
/// @param[in]  dump_cookie  First argument to the above function.
///
/// @return FAIL in case of unrecoverable error, OK otherwise.
static WFDEC(translate_lval, ExpressionNode *expr, bool is_funccall,
                             bool unique,
                             AssignmentValueDump dump, void *dump_cookie)
{
#define ADD_ASSIGN(what) \
  { \
    if (is_funccall) { \
      WS("vim.assign_" what "_function(state, ") \
      CALL(dump_bool, unique) \
      WS(", ") \
    } else { \
      WS("vim.assign_" what "(state, ") \
    } \
  }
  switch (expr->type) {
    case kExprSimpleVariableName: {
      char_u *start;
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
      ExpressionNode *current_expr = expr->children;
      bool add_concat;

      assert(current_expr != NULL);

      ADD_ASSIGN("scope")
      CALL(dump, dump_cookie)
      WS(", ")

      if (current_expr->type == kExprIdentifier) {
        char_u *start;
        CALL(translate_scope, &start, expr, TS_ONLY_SEGMENT
                                            |(is_funccall
                                              ? TS_FUNCASSIGN
                                              : 0))
        if (start == NULL) {
          WS("vim.get_scope_and_key(state, '")
          W_EXPR_POS(expr)
          WS("'")
        } else {
          WS(", '")
          W_END(start, expr->end_position)
          WS("'")
        }
        add_concat = TRUE;
      } else {
        WS("vim.get_scope_and_key(state, ")
        add_concat = FALSE;
      }

      for (current_expr = current_expr->next; current_expr != NULL;
           current_expr = current_expr->next) {
        if (add_concat)
          WS(" .. ")
        else
          add_concat = TRUE;
        switch (current_expr->type) {
          case kExprIdentifier: {
            WS("'")
            W_EXPR_POS(current_expr)
            WS("'")
            break;
          }
          case kExprCurlyName: {
            WS("(")
            CALL(translate_expr, current_expr->children)
            WS(")")
            break;
          }
          default: {
            assert(FALSE);
          }
        }
      }
      WS(")")
      break;
    }
    case kExprConcatOrSubscript: {
      ADD_ASSIGN("dict")
      CALL(dump, dump_cookie)
      WS(", ")
      CALL(translate_expr, expr->children)
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
      CALL(translate_expr, expr->children)
      WS(", ")
      CALL(translate_expr, expr->children->next)
      if (expr->children->next->next != NULL) {
        WS(", ")
        CALL(translate_expr, expr->children->next->next)
      }
      WS(")")
      break;
    }
    default: {
      assert(FALSE);
    }
  }
  WS("\n")
  return OK;
#undef ADD_ASSIGN
}

/// Dump VimL Ex command
///
/// @param[in]  node    Node to translate.
/// @param[in]  indent  Indentation of the result.
static WFDEC(translate_node, const CommandNode *const node, size_t indent)
{
  char_u *name;
  size_t start_from_arg = 0;
  bool do_arg_dump = TRUE;
  bool add_comma = FALSE;

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
    WS("--")
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
    CALL(translate_lval, node->args[ARG_FUNC_NAME].arg.expr, TRUE, !node->bang,
                         (AssignmentValueDump) &translate_function,
                         (void *) &args)
    return OK;
    return OK;
  } else if (node->type == kCmdFor) {
#define INDENTVAR(var, prefix) \
    char var[NUMBUFLEN + sizeof(prefix) - 1]; \
    size_t var##_len; \
    if ((var##_len = sprintf(var, prefix "%ld", (long) indent)) <= 1) \
      return FAIL;
#define ADDINDENTVAR(var) \
    W_LEN(var, var##_len)

    INDENTVAR(iter_var, "i")
    WS("local ")
    ADDINDENTVAR(iter_var)
    WS("\n")

    WINDENT(indent)
    WS("for _, ")
    ADDINDENTVAR(iter_var)
    WS(" in vim.list.iterator(state, ")
    CALL(translate_expr, node->args[ARG_FOR_RHS].arg.expr)
    WS(") do\n")

    // TODO assign variables

    CALL(translate_nodes, node->children, indent + 1)

    WINDENT(indent)
    WS("end\n")
    return OK;
  } else if (node->type == kCmdWhile) {
    WS("while (")
    CALL(translate_expr, node->args[ARG_EXPR_EXPR].arg.expr)
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
        CALL(translate_expr, node->args[ARG_EXPR_EXPR].arg.expr)
        WS(") then\n")
        break;
      }
      default: {
        assert(FALSE);
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
          assert(FALSE);
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
    WS("end, state:trying())\n")

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
      bool did_first_if = FALSE;

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
          did_first_if = TRUE;
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
#undef INDENTVAR
#undef ADDINDENTVAR

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
    add_comma = TRUE;
  }

  if (CMDDEF(node->type).flags & BANG) {
    if (add_comma)
      WS(", ")
    CALL(dump_bool, node->bang)
    add_comma = TRUE;
  }

  if (CMDDEF(node->type).flags & EXFLAGS) {
    if (add_comma)
      WS(", ")
    CALL(translate_ex_flags, node->exflags)
    add_comma = TRUE;
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
      add_comma = FALSE;
      switch (CMDDEF(node->type).arg_types[i]) {
        case kArgExpression: {
          CALL(translate_expr, node->args[i].arg.expr)
          add_comma = TRUE;
          break;
        }
        case kArgExpressions: {
          CALL(translate_exprs, node->args[i].arg.expr)
          add_comma = TRUE;
          break;
        }
        case kArgString: {
          CALL(dump_string, node->args[i].arg.str)
          add_comma = TRUE;
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
        switch (to) {
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
        switch (to) {
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
            CALL(translate_expr, node->args[ARG_EXPR_EXPR].arg.expr)
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

  if (current_node == NULL && to == kTransFunc) {
    WINDENT(indent)
    WS("return vim.number.new(state, 0)\n")
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

  WS("vim = require 'vim'\n"
     "s = vim.new_scope(false)\n"
     "return {\n"
     "  run=function(state)\n"
     "    state = state:set_script_locals(s)\n")

  CALL_SCRIPT(translate_nodes, node, 2)

  WS("  end\n"
    "}\n")
  return OK;
}

/// Translate given sequence of nodes as a .vim script and dump result to stdout
///
/// @param[in]  node  Pointer to the first command inside this script.
///
/// @return OK in case of success, FAIL otherwise.
static int translate_script_stdout(const CommandNode *const node)
{
  return translate_script(node, write_file, (void *) stdout);
}

/// Get line from file
///
/// @param[in]  file  File from which line will be obtained.
///
/// @return String in allocated memory or NULL in case of error or when there 
///         are no more lines.
static char_u *fgetline_file(int c, FILE *file, int indent)
{
  char *res;
  size_t len;

  res = XCALLOC_NEW(char, 1024);

  if (fgets(res, 1024, file) == NULL)
    return NULL;

  len = STRLEN(res);

  if (res[len - 1] == '\n')
    res[len - 1] = NUL;

  return (char_u *) res;
}

/// Translate script passed through stdin to stdout
///
/// @return OK in case of success, FAIL otherwise.
int translate_script_std(void)
{
  CommandNode *node;
  CommandParserOptions o = {0, FALSE};
  CommandPosition position = {1, 1, (char_u *) "<test input>"};
  int ret;

  if ((node = parse_cmd_sequence(o, position, (LineGetter) &fgetline_file,
                                 stdin)) == NULL)
    return FAIL;

  ret = translate_script_stdout(node);

  free_cmd(node);
  return ret;
}

/// Translate script passed as a single string to given file
///
/// @param[in]  str    Translated script.
/// @param[in]  fname  Target filename.
///
/// @return OK in case of success, FAIL otherwise.
int translate_script_str_to_file(const char_u *const str,
                                 const char *const fname)
{
  CommandNode *node;
  CommandParserOptions o = {0, FALSE};
  CommandPosition position = {1, 1, (char_u *) "<test input>"};
  int ret;
  char_u **pp;
  FILE *f;

  pp = (char_u **) &str;

  if ((node = parse_cmd_sequence(o, position, (LineGetter) &fgetline_string,
                                 pp)) == NULL)
    return FAIL;

  if ((f = fopen(fname, "w")) == NULL)
    return FAIL;

  ret = translate_script(node, write_file, (void *) f);

  fclose(f);

  return ret;
}
