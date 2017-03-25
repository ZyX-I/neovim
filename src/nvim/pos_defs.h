#ifndef NVIM_POS_DEFS_H
#define NVIM_POS_DEFS_H

/// Line number type
typedef long linenr_T;
/// Format used to print values which have linenr_T type
#define PRIdLINENR "ld"

/// Column number type
typedef int colnr_T;
/// Format used to print values which have colnr_T type
#define PRIdCOLNR "d"

enum { MAXLNUM = 0x7fffffff };  ///< Maximum (invalid) line number.
enum { MAXCOL = 0x7fffffff };  ///< Maximum column number, 31 bits.

/// Position in file or buffer
typedef struct {
  linenr_T lnum;  ///< Line number.
  colnr_T col;  ///< Column number.
  colnr_T coladd;  ///< Additional offset for multicolumn characters.
} pos_T;

#define INIT_POS_T(l, c, ca) {l, c, ca}

/// Position in file or buffer, without coladd
typedef struct {
  linenr_T lnum;  ///< Line number.
  colnr_T col;  ///< Column number.
} lpos_T;

#endif  // NVIM_POS_DEFS_H
