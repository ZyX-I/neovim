#include <dlfcn.h>
#include <stdio.h>

int do_init(void **handle)
{
  void (*init_func)(void);
  *handle = dlopen("build/src/nvim/libnvim-test.so", RTLD_LAZY);

  if (*handle == NULL) {
    const char *error = dlerror();
    if (error == NULL)
      return 1;
    fputs(error, stderr);
    fputs("\n", stderr);
    return 2;
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
    init_func = dlsym(*handle, *iname);
    if (init_func == NULL)
      return 3;
    init_func();
  }
  return 0;
}
