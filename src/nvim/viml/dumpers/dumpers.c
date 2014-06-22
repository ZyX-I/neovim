#include <string.h>

#include "nvim/vim.h"
#include "nvim/viml/dumpers/dumpers.h"

#ifdef INCLUDE_GENERATED_DECLARATIONS
# include "viml/dumpers/dumpers.h.generated.h"
#endif

/// Write string with the given length
///
/// @param[in]  s       String that will be written.
/// @param[in]  len     Length of this string.
/// @param[in]  write   Function used to write the string.
/// @param[in]  cookie  Last argument to that function.
///
/// @return OK in case of success, FAIL otherwise.
int write_string_len(const char *const s, size_t len, Writer write,
                     void *cookie)
{
  size_t written;
  if (len) {
    written = write(s, 1, len, cookie);
    // TODO Examine errno
  }
  return OK;
}

/// Write string with given characters escaped
///
/// @param[in]  s       String that will be written.
/// @param[in]  len     Length of this string.
/// @param[in]  write   Function used to write the string.
/// @param[in]  cookie  Pointer to the structure with last argument to that 
///                     function and characters that need to be escaped.
///
/// @return Number of characters written.
size_t write_escaped_string_len(const void *s, size_t size, size_t nmemb,
                                void *cookie)
{
  const EscapedCookie *const arg = (const EscapedCookie *) cookie;

  if (size != 1) {
    return arg->write(s, size, nmemb, arg->cookie);
  }

  const char *const e = ((char *) s) + nmemb - 1;
  const char bslash[] = {'\\'};
  size_t written = 0;

  for (const char *p = (char *) s; p <= e; p++) {
    if (strchr(arg->echars, *p) != NULL) {
      written += arg->write(bslash, 1, 1, arg->cookie);
    }
    written += arg->write(p, 1, 1, arg->cookie);
    // TODO Examine errno
  }
  return written;
}
