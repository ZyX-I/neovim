#ifndef NVIM_VIML_PARSER_COMMAND_ARGUMENTS_H_
#define NVIM_VIML_PARSER_COMMAND_ARGUMENTS_H_

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
#define ARGS_E        {kArgString, kArgGlob}
#define ARGS_SO       {kArgPattern}
#define ARGS_AU       {kArgString, kArgAuEvents, kArgPattern, kArgFlags}
#define ARGS_NAME     {kArgString}
#define ARGS_UNMAP    {kArgFlags, kArgString}
#define ARGS_UNMENU   {kArgMenuName}
#define ARGS_BREAK    {kArgFlags, kArgPattern}
#define ARGS_EXPR     {kArgExpression}
#define ARGS_REG      {kArgRegex}
#define ARGS_CENTER   {kArgNumber}
#define ARGS_CLIST    {kArgNumber, kArgNumber}
#define ARGS_ADDR     {kArgAddress}
#define ARGS_CMD      {kArgFlags, kArgCmdComplete, kArgString}
#define ARGS_SUBCMD   {kArgArgs}
#define ARGS_CSTAG    {kArgString}
#define ARGS_DIG      {kArgStrings, kArgNumbers}
#define ARGS_DOAU     {kArgFlags, kArgString, kArgAuEvent, kArgString}
#define ARGS_EXPRS    {kArgExpressions}
#define ARGS_LOCKVAR  {kArgExpressions, kArgUNumber}
#define ARGS_EXIT     {kArgString, kArgGlob}
#define ARGS_WN       {kArgString, kArgGlob}
#define ARGS_FOR      {kArgString, kArgAssignLhs, kArgExpression}
#define ARGS_LET      {kArgFlags, kArgAssignLhs, kArgExpression}
#define ARGS_FUNC     {kArgRegex, kArgAssignLhs, kArgStrings, kArgFlags}
#define ARGS_G        {kArgRegex}
#define ARGS_SHELL    {kArgString}
#define ARGS_HELPG    {kArgRegex, kArgString}
#define ARGS_HI       {kArgFlags,  kArgString, kArgFlags,   kArgString,  \
                       kArgString, kArgFlags,  kArgUNumber, kArgUNumber, \
                       kArgFlags,  kArgString, kArgUNumber, kArgUNumber, \
                       kArgUNumber}
#define ARGS_HIST     {kArgFlags, kArgNumber, kArgNumber}
#define ARGS_LANG     {kArgFlags, kArgString}
#define ARGS_VIMG     {kArgFlags, kArgRegex, kArgString, kArgGlob}
#define ARGS_MARK     {kArgChar}
#define ARGS_SIMALT   {kArgChar}
#define ARGS_MATCH    {kArgString, kArgRegex}
#define ARGS_MT       {kArgString, kArgString}
#define ARGS_NORMAL   {kArgString}
#define ARGS_PROFILE  {kArgFlags, kArgString, kArgGlob, kArgPattern}
#define ARGS_W        {kArgString, kArgGlob, kArgString}
#define ARGS_REDIR    {kArgFlags, kArgString, kArgGlob, kArgAssignLhs}
#define ARGS_S        {kArgRegex, kArgReplacement, kArgFlags}
#define ARGS_SET      {kArgStrings, kArgNumbers, kArgStrings}
#define ARGS_FT       {kArgFlags}
#define ARGS_SLEEP    {kArgUNumber}
#define ARGS_SNIFF    {kArgString}
#define ARGS_SORT     {kArgFlags, kArgRegex}
#define ARGS_SYNTIME  {kArgFlags}
#define ARGS_WINSIZE  {kArgUNumber, kArgUNumber}
#define ARGS_WINCMD   {kArgChar}
#define ARGS_ERROR    {kArgString, kArgString, kArgColumn}
#define ARGS_USER     {kArgString}

// ++opt flags
#define FLAG_OPT_FF_MASK      0x0003
#define VAL_OPT_FF_USEOPT     0x0000
#define VAL_OPT_FF_DOS        0x0001
#define VAL_OPT_FF_UNIX       0x0002
#define VAL_OPT_FF_MAC        0x0003
#define FLAG_OPT_BIN_USE_FLAG 0x0004
#define FLAG_OPT_BIN          0x0008
#define FLAG_OPT_EDIT         0x0010
#define FLAG_OPT_BAD_USE_FLAG 0x0020
#define SHIFT_OPT_BAD         6
#define FLAG_OPT_BAD_MASK     (0x001FF << SHIFT_OPT_BAD)
#define VAL_OPT_BAD_KEEP      (0x00100 << SHIFT_OPT_BAD)
#define VAL_OPT_BAD_DROP      (0x001FF << SHIFT_OPT_BAD)
// :gui/:gvim: -g/-b flag
#define FLAG_OPT_GUI_USE_FLAG 0x0200
#define FLAG_OPT_GUI_FORK     0x0400
// :write/:update
#define FLAG_OPT_W_APPEND     0x0800

#define CHAR_TO_VAL_OPT_BAD(c) (((uint_least32_t) c) << SHIFT_OPT_BAD)
#define VAL_OPT_BAD_TO_CHAR(f) ((char) ((f >> SHIFT_OPT_BAD) & 0xFF))

// Constants to index arguments in CommandNode
#define ARG_NO_ARGS -1

// :append/:insert
enum {
  ARG_APPEND_LINES  = 0,
};

// :*map/:*abbrev
enum {
  ARG_MAP_FLAGS     = 0,
  ARG_MAP_LHS,
  ARG_MAP_RHS,
  ARG_MAP_EXPR,
};

#define FLAG_MAP_BUFFER    0x01
#define FLAG_MAP_NOWAIT    0x02
#define FLAG_MAP_SILENT    0x04
#define FLAG_MAP_SPECIAL   0x08
#define FLAG_MAP_SCRIPT    0x10
#define FLAG_MAP_EXPR      0x20
#define FLAG_MAP_UNIQUE    0x40

// :*menu
enum {
  ARG_MENU_FLAGS    = 0,
  ARG_MENU_ICON,
  ARG_MENU_PRI,
  ARG_MENU_NAME,
  ARG_MENU_TEXT,
  ARG_MENU_RHS,
};

#define FLAG_MENU_SILENT   0x01
#define FLAG_MENU_SPECIAL  0x02
#define FLAG_MENU_SCRIPT   0x04
#define FLAG_MENU_DISABLE  0x08
#define FLAG_MENU_ENABLE   0x10

// :*mapclear/:*abclear
enum {
  ARG_CLEAR_BUFFER  = 0,
};

// :aboveleft and friends
#define ARG_MODIFIER_CMD   ARG_NO_ARGS

// :argdo/:bufdo
#define ARG_DO_CMD         ARG_NO_ARGS

// :args/:e
enum {
  ARG_E_EXPR        = 0,
  ARG_E_FILES,
};

// :argadd/:argdelete/:source
enum {
  ARG_SO_FILES      = 0,
};

// :au
enum {
  ARG_AU_GROUP      = 0,
  ARG_AU_EVENTS,
  ARG_AU_PATTERNS,
  ARG_AU_NESTED,
};

// :aug/:behave/:colorscheme
enum {
  ARG_NAME_NAME     = 0,
};

// :*unmap/:*unabbrev
enum {
  ARG_UNMAP_BUFFER  = 0,
  ARG_UNMAP_LHS,
};

// :*unmenu
enum {
  ARG_UNMENU_LHS    = 0,
};

// :breakadd/:breakdel
// lnum is recorded in range
enum {
  ARG_BREAK_FLAGS   = 0,
  ARG_BREAK_NAME,
};

#define FLAG_BREAK_FUNC    0x01
#define FLAG_BREAK_FILE    0x02
#define FLAG_BREAK_HERE    0x04

// :caddexpr/:laddexpr/:call
enum {
  ARG_EXPR_EXPR     = 0,
};

// :catch/:djump
enum {
  ARG_REG_REG       = 0,
};

// :center
enum {
  ARG_CENTER_WIDTH  = 0,
};

// :clist
enum {
  ARG_CLIST_FIRST   = 0,
  ARG_CLIST_LAST,
};

// :copy/:move
enum {
  ARG_ADDR_ADDR     = 0,
};

// :command
enum {
  ARG_CMD_FLAGS     = 0,
  ARG_CMD_COMPLETE,
};

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
enum {
  ARG_SUBCMD        = 0,
};

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
enum {
  ARG_CSTAG_TAG     = 0,
};

// :digraphs
enum {
  ARG_DIG_DIGRAPHS  = 0,
  ARG_DIG_CHARS,
};

// :doautocmd/:doautoall
enum {
  ARG_DOAU_NOMDLINE = 0,
  ARG_DOAU_GROUP,
  ARG_DOAU_EVENT,
  ARG_DOAU_FNAME,
};

// :echo*/:execute
enum {
  ARG_EXPRS_EXPRS   = 0,
  // :lockvar
  ARG_LOCKVAR_DEPTH,
};

// :exit
enum {
  ARG_EXIT_EXPR     = 0,
  ARG_EXIT_FILES,
};

// :wnext/:wNext/:wprevious
enum {
  ARG_WN_EXPR       = 0,
  ARG_WN_FILES,
};

// :for
enum {
  ARG_FOR_STR       = 0,
  ARG_FOR_LHS,
  ARG_FOR_RHS,
};

// :let
enum {
  ARG_LET_ASS_TYPE  = 0,
  ARG_LET_LHS,
  ARG_LET_RHS,
};

typedef enum {
  VAL_LET_NO_ASS    = 0,
  VAL_LET_ASSIGN,
  VAL_LET_ADD,
  VAL_LET_SUBTRACT,
  VAL_LET_APPEND,
} LetAssignmentType;

// :func
enum {
  ARG_FUNC_REG      = 0,
  ARG_FUNC_NAME,
  ARG_FUNC_ARGS,
  ARG_FUNC_FLAGS,
};

#define FLAG_FUNC_VARARGS  0x01
#define FLAG_FUNC_RANGE    0x02
#define FLAG_FUNC_ABORT    0x04
#define FLAG_FUNC_DICT     0x08

// :global
enum {
  ARG_G_REG         = 0,
};

// :grep/:make/:!
enum {
  ARG_SHELL_ARGS    = 0,
};

// :helpg/:lhelpg
enum {
  ARG_HELPG_REG     = 0,
  ARG_HELPG_LANG,
};

// :highlight
enum {
  ARG_HI_FLAGS      = 0,
  ARG_HI_GROUP,
  ARG_HI_TERM,
  ARG_HI_START,
  ARG_HI_STOP,
  ARG_HI_CTERM,
  ARG_HI_CTERMFG,
  ARG_HI_CTERMBG,
  ARG_HI_GUI,
  ARG_HI_FONT,
  ARG_HI_GUIFG,
  ARG_HI_GUIBG,
  ARG_HI_GUISP,
};

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
enum {
  ARG_HIST_FLAGS    = 0,
  ARG_HIST_FIRST,
  ARG_HIST_LAST,
};

#define FLAG_HIST_CMD      0x01
#define FLAG_HIST_SEARCH   0x02
#define FLAG_HIST_EXPR     0x04
#define FLAG_HIST_INPUT    0x08
#define FLAG_HIST_DEBUG    0x10
#define FLAG_HIST_HAS_FST  0x20
#define FLAG_HIST_HAS_LST  0x40

// :language
enum {
  ARG_LANG_FLAGS    = 0,
  ARG_LANG_LANG,
};

#define FLAG_LANG_MESSAGES 0x01
#define FLAG_LANG_CTYPE    0x02
#define FLAG_LANG_TIME     0x04

// :*vimgrep*
enum {
  ARG_VIMG_FLAGS    = 0,
  ARG_VIMG_REG,
  ARG_VIMG_FILES,
};

#define FLAG_VIMG_EVERY    0x01
#define FLAG_VIMG_NOJUMP   0x02

// :k/:mark
enum {
  ARG_MARK_CHAR     = 0,
};

// :simalt
enum {
  ARG_SIMALT_CHAR   = 0,
};

// :match
enum {
  ARG_MATCH_GROUP   = 0,
  ARG_MATCH_REG,
};

// :menutranslate
enum {
  ARG_MT_FROM       = 0,
  ARG_MT_TO,
};

// :normal
enum {
  ARG_NORMAL_STR    = 0,
};

// :profile/:profdel
enum {
  ARG_PROFILE_FLAGS = 0,
  ARG_PROFILE_EXPR,
  ARG_PROFILE_FILE,
  ARG_PROFILE_PAT,
};

#define FLAG_PROFILE_ACTION_MASK  0x07
#define VAL_PROFILE_ACTION_START  0x00
#define VAL_PROFILE_ACTION_PAUSE  0x01
#define VAL_PROFILE_ACTION_CONT   0x02
#define VAL_PROFILE_ACTION_FUNC   0x03
#define VAL_PROFILE_ACTION_FILE   0x04

// :write/:read/:update
enum {
  ARG_W_EXPR        = 0,
  ARG_W_FILE,
  ARG_W_SHELL,
};

// :redir
enum {
  ARG_REDIR_EXPR    = 0,
  ARG_REDIR_FLAGS,
  ARG_REDIR_FILE,
  ARG_REDIR_VAR,
};

#define FLAG_REDIR_REG_MASK 0x0FF
#define FLAG_REDIR_APPEND   0x100

// :substitute
enum {
  ARG_S_REG         = 0,
  ARG_S_REP,
  ARG_S_FLAGS,
};

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
enum {
  ARG_SET_OPTIONS   = 0,
  ARG_SET_FLAGSS,
  ARG_SET_VALUES,
};

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
enum {
  ARG_FT_FLAGS      = 0,
};

#define FLAG_FT_ON         0x01
#define FLAG_FT_PLUGIN     0x02
#define FLAG_FT_INDENT     0x04
#define FLAG_FT_DETECT     0x08

// :sign
// FIXME

// :sleep
enum {
  ARG_SLEEP_SECONDS = 0,
};

// :sniff
enum {
  ARG_SNIFF_SYMBOL  = 0,
};

// :sort
enum {
  ARG_SORT_FLAGS    = 0,
  ARG_SORT_REG,
};

#define FLAG_SORT_IC       0x01
#define FLAG_SORT_DECIMAL  0x02
#define FLAG_SORT_HEX      0x04
#define FLAG_SORT_OCTAL    0x08
#define FLAG_SORT_KEEPFST  0x10
#define FLAG_SORT_USEMATCH 0x20

// :syn
// FIXME

// :syntime
enum {
  ARG_SYNTIME_ACTION= 0,
};

#define ACT_SYNTIME_ON     0x01
#define ACT_SYNTIME_OFF    0x02
#define ACT_SYNTIME_CLEAR  0x03
#define ACT_SYNTIME_REPORT 0x04

// :winsize
enum {
  ARG_WINSIZE_WIDTH = 0,
  ARG_WINSIZE_HEIGHT,
};

// :wincmd
enum {
  ARG_WINCMD_CHAR   = 0,
};

// syntax error
enum {
  ARG_ERROR_LINESTR = 0,
  ARG_ERROR_MESSAGE,
  ARG_ERROR_OFFSET,
};

// User-defined commands
enum {
  ARG_USER_ARG      = 0,
};

#endif  // NVIM_VIML_PARSER_COMMAND_ARGUMENTS_H_
