// vim: ts=8 sts=2 sw=2 tw=80
//
// Copyright 2014 Nikolay Pavlov

#ifndef NEOVIM_TRANSLATOR_PARSER_COMMAND_ARGUMENTS_H_
#define NEOVIM_TRANSLATOR_PARSER_COMMAND_ARGUMENTS_H_

typedef enum {
  kArgExpression,    // :let a={expr}
  kArgExpressions,   // :echo {expr1}[ {expr2}]
  kArgFlags,         // :map {<nowait><expr><buffer><silent><unique>}
  kArgNumber,        // :resize -1
  kArgUNumber,       // :sign place {id} line={lnum} name=name buffer={nr}
  kArgNumbers,       // :dig e: {num1} a: {num2}
  kArgString,        // :sign place 10 line=11 name={name} buffer=1
  // Note the difference: you cannot use backtics in :autocmd, but can in :e
  kArgPattern,       // :autocmd BufEnter {pattern} :echo 'HERE'
  kArgGlob,          // :e {glob}
  kArgRegex,         // :s/{reg}/\="abc"/g
  kArgReplacement,   // :s/.*/{repl}/g
  kArgLines,         // :py << EOF\n{str}\n{str2}EOF, :append\n{str}\n.
  kArgStrings,       // :cscope add cscope.out /usr/local/vim {str1}[ {str2}]
  kArgAssignLhs,     // :let {lhs} = [1, 2]
  kArgMenuName,      // :amenu File.Edit :browse edit<CR>
  kArgAuEvent,       // :doau {event}
  kArgAuEvents,      // :au {event1}[,{event2}] * :echo 'HERE'
  kArgAddress,       // :copy {address}
  kArgCmdComplete,   // :command -complete={complete}
  kArgArgs,          // for commands with subcommands
  kArgChar,          // :mark {char}
  kArgColumn,        // column (in syntax error)
} CommandArgType;

#define ARGS_NO       {0}
#define ARGS_MODIFIER ARGS_NO
#define ARGS_DO       ARGS_NO
#define ARGS_APPEND   {kArgLines}
#define ARGS_MAP      {kArgFlags, kArgString, kArgString, kArgExpression}
#define ARGS_MENU     {kArgFlags, kArgString, kArgNumbers, kArgMenuName, \
                       kArgString, kArgString}
#define ARGS_CLEAR    {kArgFlags}
#define ARGS_E        {kArgGlob, kArgFlags, kArgString}
#define ARGS_SO       {kArgGlob}
#define ARGS_AU       {kArgString, kArgAuEvents, kArgPattern, kArgFlags}
#define ARGS_NAME     {kArgString}
#define ARGS_UNMAP    {kArgFlags, kArgString}
#define ARGS_UNMENU   {kArgMenuName}
#define ARGS_BREAK    {kArgFlags, kArgPattern}
#define ARGS_EXPR     {kArgString, kArgExpression}
#define ARGS_REG      {kArgRegex}
#define ARGS_CENTER   {kArgNumber}
#define ARGS_CLIST    {kArgNumber, kArgNumber}
#define ARGS_ADDR     {kArgAddress}
#define ARGS_CMD      {kArgFlags, kArgCmdComplete, kArgString}
#define ARGS_SUBCMD   {kArgArgs}
#define ARGS_CSTAG    {kArgString}
#define ARGS_DIG      {kArgStrings, kArgNumbers}
#define ARGS_DOAU     {kArgFlags, kArgString, kArgAuEvent, kArgString}
#define ARGS_EXPRS    {kArgString, kArgExpressions}
#define ARGS_LOCKVAR  {kArgString, kArgExpressions, kArgUNumber}
#define ARGS_EXIT     {kArgGlob, kArgFlags, kArgString}
#define ARGS_FIRST    {kArgFlags, kArgString}
#define ARGS_WN       {kArgGlob, kArgFlags, kArgString}
#define ARGS_ASSIGN   {kArgString, kArgAssignLhs, kArgExpression}
#define ARGS_FUNC     {kArgString, kArgRegex, kArgAssignLhs, kArgStrings, \
                       kArgFlags}
#define ARGS_G        {kArgRegex}
#define ARGS_SHELL    {kArgString}
#define ARGS_HELPG    {kArgRegex, kArgString}
#define ARGS_HI       {kArgFlags,  kArgString, kArgFlags,   kArgString,  \
                       kArgString, kArgFlags,  kArgUNumber, kArgUNumber, \
                       kArgFlags,  kArgString, kArgUNumber, kArgUNumber, \
                       kArgUNumber}
#define ARGS_HIST     {kArgFlags, kArgNumber, kArgNumber}
#define ARGS_LANG     {kArgFlags, kArgString}
#define ARGS_VIMG     {kArgFlags, kArgRegex, kArgGlob}
#define ARGS_MARK     {kArgChar}
#define ARGS_SIMALT   {kArgChar}
#define ARGS_MATCH    {kArgString, kArgRegex}
#define ARGS_MT       {kArgString, kArgString}
#define ARGS_NORMAL   {kArgString}
#define ARGS_PROFILE  {kArgFlags, kArgGlob, kArgPattern}
#define ARGS_W        {kArgGlob, kArgFlags, kArgString, kArgString}
#define ARGS_REDIR    {kArgFlags, kArgGlob, kArgAssignLhs}
#define ARGS_S        {kArgRegex, kArgReplacement, kArgFlags}
#define ARGS_SET      {kArgStrings, kArgNumbers, kArgStrings}
#define ARGS_FT       {kArgFlags}
#define ARGS_SLEEP    {kArgUNumber}
#define ARGS_SNIFF    {kArgString}
#define ARGS_SORT     {kArgFlags, kArgRegex}
#define ARGS_SYNTIME  {kArgFlags}
#define ARGS_WINSIZE  {kArgUNumber, kArgUNumber}
#define ARGS_WINCMD   {kArgChar}
#define ARGS_WQA      {kArgFlags, kArgString}
#define ARGS_ERROR    {kArgString, kArgString, kArgColumn}
#define ARGS_USER     {kArgString}

// ++opt flags
#define FLAG_OPT_FF_MASK    0x03
#define VAL_OPT_FF_USEOPT   0x00
#define VAL_OPT_FF_DOS      0x01
#define VAL_OPT_FF_UNIX     0x02
#define VAL_OPT_FF_MAC      0x03
#define FLAG_OPT_BIN_USEOPT 0x04
#define FLAG_OPT_BIN        0x08
#define FLAG_OPT_EDIT       0x10
#define SHIFT_OPT_BAD       5
#define FLAG_OPT_BAD_MASK   (0x1FF << SHIFT_E_BAD)
#define VAL_OPT_BAD_KEEP    (0x000 << SHIFT_E_BAD)
#define VAL_OPT_BAD_DROP    (0x1FF << SHIFT_E_BAD)
// :gui/:gvim: -g/-b flag
#define FLAG_OPT_GUI_FORK   0x20
// :write/:update
#define FLAG_OPT_W_APPEND   0x20

#define DEFAULT_OPT_FLAGS   (VAL_OPT_FF_UNKNOWN \
                             | (FLAG_OPT_BIN_USEOPT) \
                             | (((unsigned)'?')<<SHIFT_OPT_BAD))

// Constants to index arguments in CommandNode
#define ARG_NO_ARGS -1

// :append/:insert
#define ARG_APPEND_LINES   0

// :*map/:*abbrev
#define ARG_MAP_FLAGS      0
#define ARG_MAP_LHS        1
#define ARG_MAP_RHS        2
#define ARG_MAP_EXPR       3

#define FLAG_MAP_BUFFER    0x01
#define FLAG_MAP_NOWAIT    0x02
#define FLAG_MAP_SILENT    0x04
#define FLAG_MAP_SPECIAL   0x08
#define FLAG_MAP_SCRIPT    0x10
#define FLAG_MAP_EXPR      0x20
#define FLAG_MAP_UNIQUE    0x40

// :*menu
#define ARG_MENU_FLAGS     0
#define ARG_MENU_ICON      1
#define ARG_MENU_PRI       2
#define ARG_MENU_NAME      3
#define ARG_MENU_TEXT      4
#define ARG_MENU_RHS       5

#define FLAG_MENU_SILENT   0x01
#define FLAG_MENU_SPECIAL  0x02
#define FLAG_MENU_SCRIPT   0x04
#define FLAG_MENU_DISABLE  0x08
#define FLAG_MENU_ENABLE   0x10

// :*mapclear/:*abclear
#define ARG_CLEAR_BUFFER   0

// :aboveleft and friends
#define ARG_MODIFIER_CMD   ARG_NO_ARGS

// :argdo/:bufdo
#define ARG_DO_CMD         ARG_NO_ARGS

// :args/:e
#define ARG_E_FILES        0
#define ARG_E_OPT          1
#define ARG_E_ENC          2

// :argadd/:argdelete/:source
#define ARG_SO_FILES       0

// :au
#define ARG_AU_GROUP       0
#define ARG_AU_EVENTS      1
#define ARG_AU_PATTERNS    2
#define ARG_AU_NESTED      3

// :aug/:behave/:colorscheme
#define ARG_NAME_NAME      0

// :*unmap/:*unabbrev
#define ARG_UNMAP_BUFFER   0
#define ARG_UNMAP_LHS      1

// :*unmenu
#define ARG_UNMENU_LHS     0

// :breakadd/:breakdel
// lnum is recorded in range
#define ARG_BREAK_FLAGS    0
#define ARG_BREAK_NAME     1

#define FLAG_BREAK_FUNC    0x01
#define FLAG_BREAK_FILE    0x02
#define FLAG_BREAK_HERE    0x04

// :caddexpr/:laddexpr/:call
#define ARG_EXPR_STR       0
#define ARG_EXPR_EXPR      1

// :catch/:djump
#define ARG_REG_REG        0

// :center
#define ARG_CENTER_WIDTH   0

// :clist
#define ARG_CLIST_FIRST    0
#define ARG_CLIST_LAST     1

// :copy/:move
#define ARG_ADDR_ADDR      0

// :command
#define ARG_CMD_FLAGS      0
#define ARG_CMD_COMPLETE   1

#define FLAG_CMD_NARGS_MASK 0x007
#define VAL_CMD_NARGS_NO    0x000
#define VAL_CMD_NARGS_ONE   0x001
#define VAL_CMD_NARGS_ANY   0x002
#define VAL_CMD_NARGS_Q     0x003
#define VAL_CMD_NARGS_P     0x004
// Number is recorded in count
#define FLAG_CMD_RANGE_MASK 0x018
#define VAL_CMD_RANGE_NO    0x000
#define VAL_CMD_RANGE_CUR   0x008
#define VAL_CMD_RANGE_ALL   0x010
#define VAL_CMD_RANGE_COUNT 0x018
#define FLAG_CMD_BANG       0x020
#define FLAG_CMD_BAR        0x040
#define FLAG_CMD_REGISTER   0x080
#define FLAG_CMD_BUFFER     0x100

// :cscope/:sign
#define ARG_SUBCMD         0

// :cscope
typedef enum {
  kCscopeAdd,
  kCscopeFind,
  kCscopeHelp,
  kCscopeKill,
  kCscopeReset,
  kCscopeShow,
} CscopeArgType;
#define CSCOPE_ARG_ADD_PATH     0
#define CSCOPE_ARG_ADD_PRE_PATH 1
#define CSCOPE_ARG_ADD_FLAGS    2
#define CSCOPE_ARGS_ADD {kArgString, kArgString, kArgStrings}

typedef enum {
  kCscopeFindSymbol,
  kCscopeFindDefinition,
  kCscopeFindCallees,
  kCscopeFindCallers,
  kCscopeFindText,
  kCscopeFindEgrep,
  kCscopeFindFile,
  kCscopeFindIncluders,
} CscopeArg;
#define CSCOPE_ARG_FIND_TYPE    0
#define CSCOPE_ARG_FIND_NAME    1
#define CSCOPE_ARGS_FIND {kArgFlags, kArgString}

// :cstag
#define ARG_CSTAG_TAG      0

// :digraphs
#define ARG_DIG_DIGRAPHS   0
#define ARG_DIG_CHARS      1

// :doautocmd/:doautoall
#define ARG_DOAU_NOMDLINE  0
#define ARG_DOAU_GROUP     1
#define ARG_DOAU_EVENT     2
#define ARG_DOAU_FNAME     3

// :echo*/:execute
#define ARG_EXPRS_STR      0
#define ARG_EXPRS_EXPRS    1
// :lockvar
#define ARG_LOCKVAR_DEPTH  (ARG_EXPRS_EXPRS+1)

// :exit
#define ARG_EXIT_FILES     0
#define ARG_EXIT_OPT       1
#define ARG_EXIT_ENC       2

// :first/:rewind/:last/:next
#define ARG_FIRST_OPT      0
#define ARG_FIRST_ENC      1

// :wnext/:wNext/:wprevious
#define ARG_WN_FILES       0
#define ARG_WN_OPT         1
#define ARG_WN_ENC         2

// :for/:let
#define ARG_ASSIGN_STR     0
#define ARG_ASSIGN_LHS     1
#define ARG_ASSIGN_RHS     2

// :func
#define ARG_FUNC_STR       0
#define ARG_FUNC_REG       1
#define ARG_FUNC_NAME      2
#define ARG_FUNC_ARGS      3
#define ARG_FUNC_FLAGS     4

#define FLAG_FUNC_VARARGS  0x01
#define FLAG_FUNC_RANGE    0x02
#define FLAG_FUNC_ABORT    0x04
#define FLAG_FUNC_DICT     0x08

// :global
#define ARG_G_REG          0

// :grep/:make/:!
#define ARG_SHELL_ARGS     0

// :helpg/:lhelpg
#define ARG_HELPG_REG      0
#define ARG_HELPG_LANG     1

// :highlight
#define ARG_HI_FLAGS       0
#define ARG_HI_GROUP       1
#define ARG_HI_TERM        2
#define ARG_HI_START       3
#define ARG_HI_STOP        4
#define ARG_HI_CTERM       5
#define ARG_HI_CTERMFG     6
#define ARG_HI_CTERMBG     7
#define ARG_HI_GUI         8
#define ARG_HI_FONT        9
#define ARG_HI_GUIFG      10
#define ARG_HI_GUIBG      11
#define ARG_HI_GUISP      12

#define FLAG_HI_TERM_BOLD      0x01
#define FLAG_HI_TERM_UNDERLINE 0x02
#define FLAG_HI_TERM_UNDERCURL 0x04
#define FLAG_HI_TERM_REVERSE   0x08
#define FLAG_HI_TERM_ITALIC    0x10
#define FLAG_HI_TERM_STANDOUT  0x20
#define FLAG_HI_TERM_NONE      0x40

#define FLAG_HI_COLOR_SOME     (0x01<<24)
#define FLAG_HI_COLOR_NONE     (0x02<<24)
#define FLAG_HI_COLOR_BG       (0x04<<24)
#define FLAG_HI_COLOR_FG       (0x08<<24)

#define FLAG_HI_DEFAULT    0x01
#define FLAG_HI_CLEAR      0x02
#define FLAG_HI_LINK       0x04

// :history
#define ARG_HIST_FLAGS     0
#define ARG_HIST_FIRST     1
#define ARG_HIST_LAST      2

#define FLAG_HIST_CMD      0x01
#define FLAG_HIST_SEARCH   0x02
#define FLAG_HIST_EXPR     0x04
#define FLAG_HIST_INPUT    0x08
#define FLAG_HIST_DEBUG    0x10
#define FLAG_HIST_HAS_FST  0x20
#define FLAG_HIST_HAS_LST  0x40

// :language
#define ARG_LANG_FLAGS     0
#define ARG_LANG_LANG      1

#define FLAG_LANG_MESSAGES 0x01
#define FLAG_LANG_CTYPE    0x02
#define FLAG_LANG_TIME     0x04

// :*vimgrep*
#define ARG_VIMG_FLAGS     0
#define ARG_VIMG_REG       1
#define ARG_VIMG_FILES     2

#define FLAG_VIMG_EVERY    0x01
#define FLAG_VIMG_NOJUMP   0x02

// :k/:mark
#define ARG_MARK_CHAR      0

// :simalt
#define ARG_SIMALT_CHAR    0

// :match
#define ARG_MATCH_GROUP    0
#define ARG_MATCH_REG      1

// :menutranslate
#define ARG_MT_FROM        0
#define ARG_MT_TO          1

// :normal
#define ARG_NORMAL_STR     0

// :profile/:profdel
#define ARG_PROFILE_FLAGS  0
#define ARG_PROFILE_FILE   1
#define ARG_PROFILE_PAT    2

#define FLAG_PROFILE_ACTION_MASK  0x07
#define VAL_PROFILE_ACTION_START  0x00
#define VAL_PROFILE_ACTION_PAUSE  0x01
#define VAL_PROFILE_ACTION_CONT   0x02
#define VAL_PROFILE_ACTION_FUNC   0x03
#define VAL_PROFILE_ACTION_FILE   0x04

// :write/:read/:update
#define ARG_W_FILE         0
#define ARG_W_OPT          1
#define ARG_W_ENC          2
#define ARG_W_SHELL        3

// :redir
#define ARG_REDIR_FLAGS    0
#define ARG_REDIR_FILE     1
#define ARG_REDIR_VAR      2

#define FLAG_REDIR_REG_MASK 0x0FF
#define FLAG_REDIR_APPEND   0x100

// :substitute
#define ARG_S_REG          0
#define ARG_S_REP          1
#define ARG_S_FLAGS        2

#define FLAG_S_KEEP        0x001
#define FLAG_S_CONFIRM     0x002
#define FLAG_S_NOERR       0x004
#define FLAG_S_G           0x008
#define FLAG_S_G_REVERSE   0x010
#define FLAG_S_IC          0x020
#define FLAG_S_NOIC        0x040
#define FLAG_S_COUNT       0x080
#define FLAG_S_PRINT       0x100
#define FLAG_S_PRINT_LNR   0x200
#define FLAG_S_PRINT_LIST  0x400
#define FLAG_S_R           0x800

// :set
#define ARG_SET_OPTIONS    0
#define ARG_SET_FLAGSS     1
#define ARG_SET_VALUES     2

#define FLAG_SET_SET       0x001
#define FLAG_SET_UNSET     0x002
#define FLAG_SET_SHOW      0x004
#define FLAG_SET_INVERT    0x008
#define FLAG_SET_VI        0x010
#define FLAG_SET_VIM       0x020
#define FLAG_SET_ASSIGN    0x040
#define FLAG_SET_APPEND    0x080
#define FLAG_SET_PREPEND   0x100
#define FLAG_SET_SUBTRACT  0x200

// :filetype
#define ARG_FT_FLAGS       0

#define FLAG_FT_ON         0x01
#define FLAG_FT_PLUGIN     0x02
#define FLAG_FT_INDENT     0x04
#define FLAG_FT_DETECT     0x08

// :sign
// FIXME

// :sleep
#define ARG_SLEEP_SECONDS  0

// :sniff
#define ARG_SNIFF_SYMBOL   0

// :sort
#define ARG_SORT_FLAGS     0
#define ARG_SORT_REG       1

#define FLAG_SORT_IC       0x01
#define FLAG_SORT_DECIMAL  0x02
#define FLAG_SORT_HEX      0x04
#define FLAG_SORT_OCTAL    0x08
#define FLAG_SORT_KEEPFST  0x10
#define FLAG_SORT_USEMATCH 0x20

// :syn
// FIXME

// :syntime
#define ARG_SYNTIME_ACTION 0

#define ACT_SYNTIME_ON     0x01
#define ACT_SYNTIME_OFF    0x02
#define ACT_SYNTIME_CLEAR  0x03
#define ACT_SYNTIME_REPORT 0x04

// :winsize
#define ARG_WINSIZE_WIDTH  0
#define ARG_WINSIZE_HEIGHT 1

// :wincmd
#define ARG_WINCMD_CHAR    0

// :wqall
#define ARG_WQA_OPT        0
#define ARG_WQA_ENC        1

// syntax error
#define ARG_ERROR_LINESTR  0
#define ARG_ERROR_MESSAGE  1
#define ARG_ERROR_OFFSET   2

// User-defined commands
#define ARG_USER_ARG       0

#endif  // NEOVIM_TRANSLATOR_PARSER_COMMAND_ARGUMENTS_H_
