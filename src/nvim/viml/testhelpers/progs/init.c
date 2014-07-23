#include <dlfcn.h>
#include <stdio.h>

typedef enum {
  kTypeVoid,
  kTypeInt,
  kTypePtr
} ReturnType;

typedef struct {
  const char *name;
  ReturnType rettype;
} Init;

int do_init(void **handle)
{
  void (*init_func_void)(void);
  int (*init_func_int)(void);
  void *(*init_func_ptr)(void);
  *handle = dlopen("build/src/nvim/libnvim-test.so", RTLD_LAZY);

  if (*handle == NULL) {
    const char *error = dlerror();
    if (error == NULL)
      return 1;
    fputs(error, stderr);
    fputs("\n", stderr);
    return 2;
  }

  const Init inits[] = {
    {"mch_early_init", kTypeVoid},
    {"mb_init", kTypePtr},
    {"eval_init", kTypeVoid},
    {"init_normal_cmds", kTypeVoid},
    {"win_alloc_first", kTypeInt},
    {"init_yank", kTypeVoid},
    {"init_homedir", kTypeVoid},
    {"set_init_1", kTypeVoid},
    {"set_lang_var", kTypeVoid},
    {NULL, 0}
  };

  for (const Init *init = inits; init->name != NULL; init++) {
    switch (init->rettype) {
      case kTypeVoid: {
        init_func_void = dlsym(*handle, init->name);
        if (init_func_void == NULL)
          return 3;
        init_func_void();
        break;
      }
      case kTypeInt: {
        init_func_int = dlsym(*handle, init->name);
        if (init_func_int == NULL)
          return 3;
        init_func_int();
        break;
      }
      case kTypePtr: {
        init_func_ptr = dlsym(*handle, init->name);
        if (init_func_ptr == NULL)
          return 3;
        init_func_ptr();
        break;
      }
    }
  }
  return 0;
}
