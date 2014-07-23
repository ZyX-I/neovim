//FIXME!!!
#ifdef COMPILE_TEST_VERSION
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

int do_init(void **);

static void (*init_func)(void);
static char *(*parse_cmd_test)(char *arg, uint_least8_t flags, bool one,
                               bool out);

int main(int argc, char **argv, char **env)
{
  if (argc <= 1)
    return 1;

  void *handle;

  int ret = do_init(&handle);
  if (ret)
    return ret + 1;

  parse_cmd_test = dlsym(handle, "parse_cmd_test");
  if (parse_cmd_test == NULL)
    return 5;

  parse_cmd_test(argv[1], (argc > 2
                           ? (uint_least8_t) atoi(argv[2])
                           : 0),
                 false, true);
  putc('\n', stdout);
  return 0;
}
#endif  // COMPILE_TEST_VERSION
int abc2() {
  return 0;
}
