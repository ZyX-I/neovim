#ifndef NVIM_NORMAL_DEFS_H
#define NVIM_NORMAL_DEFS_H

#include <stdbool.h>

#include "nvim/pos_defs.h"
#include "nvim/buffer_defs.h"

/// Values for find_ident_under_cursor
enum {
  FIND_IDENT = 1,  ///< Find identifier (word).
  FIND_STRING = 2,  ///< Find any string (WORD).
  FIND_EVAL = 4,  ///< Include ->, [] and ..
};

/// Motion types, used for operators and for yank/delete registers.
///
/// The three valid numerical values must not be changed, as they
/// are used in external communication and serialization.
typedef enum {
  kMTCharWise = 0,     ///< character-wise movement/register
  kMTLineWise = 1,     ///< line-wise movement/register
  kMTBlockWise = 2,    ///< block-wise movement/register
  kMTUnknown = -1      ///< Unknown or invalid motion type
} MotionType;

/// Arguments for operators
typedef struct oparg_S {
  int op_type;  ///< Current pending operator type.
  int regname;  ///< Register to use for the operator.
  MotionType motion_type;  ///< Type of the current cursor motion.
  int motion_force;  ///< Force motion type: 'v', 'V' or CTRL-V.
  bool use_reg_one;  ///< True if delete uses reg 1 even when not linewise.
  bool inclusive;  ///< True if char motion is inclusive (only valid when
                   ///< motion_type is kMTCharWise).
  bool end_adjusted;  ///< Backuped b_op_end one char
                      ///< (only used by do_format()).
  pos_T start;  ///< Start of the operator.
  pos_T end;  ///< End of the operator.
  pos_T cursor_start;  ///< Cursor position before motion for "gw".

  long line_count;  ///< Number of lines from op_start to op_end (inclusive).
  bool empty;  ///< Op_start and op_end the same (only used by op_change()).
  bool is_visual;  ///< Operator on Visual area.
  colnr_T start_vcol;  ///< Start col for block mode operator.
  colnr_T end_vcol;  ///< End col for block mode operator.
  long prev_opcount;  ///< ca.opcount saved for K_EVENT.
  long prev_count0;  ///< ca.count0 saved for K_EVENT.
} oparg_T;

/// Arguments for Normal mode commands
typedef struct cmdarg_S {
  oparg_T     *oap;             // Operator arguments
  int prechar;                  // prefix character (optional, always 'g')
  int cmdchar;                  // command character
  int nchar;                    // next command character (optional)
  int ncharC1;                  // first composing character (optional)
  int ncharC2;                  // second composing character (optional)
  int extra_char;               // yet another character (optional)
  long opcount;                 // count before an operator
  long count0;                  // count before command, default 0
  long count1;                  // count before command, default 1
  int arg;                      // extra argument from nv_cmds[]
  int retval;                   // return: CA_* values
  char_u      *searchbuf;       // return: pointer to search pattern or NULL
} cmdarg_T;

/* values for retval: */
/// Values for retval
enum {
  CA_COMMAND_BUSY = 1,  ///< Skip restarting edit() once.
  CA_NO_ADJ_OP_END = 2,  ///< Don’t adjust operator end.
};

#endif  // NVIM_NORMAL_DEFS_H
