//FIXME!!!
#ifdef COMPILE_TEST_VERSION
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

static void (*init_func)(void);
static char *(*parse_cmd_test)(char *arg, uint_least8_t flags, bool one);

int main(int argc, char **argv, char **env)
{
  if (argc <= 1)
    return 1;

  void *handle = dlopen("build/src/libnvim-test.so", RTLD_LAZY);

  if (handle == NULL) {
    const char *error = dlerror();
    if (error == NULL)
      return 2;
    fputs(error, stderr);
    fputs("\n", stderr);
    return 3;
  }

  const char *inits[] = {
    "mch_early_init",
    "mb_init",
    "eval_init",
    "init_normal_cmds",
    "allocate_generic_buffers",
    "win_alloc_first",
    "init_yank",
    "init_homedir",
    "set_init_1",
    "set_lang_var",
    NULL
  };
  for (const char **iname = inits; *iname != NULL; iname++) {
    init_func = dlsym(handle, *iname);
    if (init_func == NULL)
      return 4;
    init_func();
  }

  parse_cmd_test = dlsym(handle, "parse_cmd_test");
  if (parse_cmd_test == NULL)
    return 5;

  char *result = parse_cmd_test(argv[1], (argc > 2
                                          ? (uint_least8_t) atoi(argv[2])
                                          : 0),
                                false);

  if (result == NULL)
    return 6;

  puts(result);
  return 0;
}
#endif  // COMPILE_TEST_VERSION
int abc2() {
  return 0;
}
