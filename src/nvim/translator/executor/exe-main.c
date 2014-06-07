//FIXME!!!
#ifdef COMPILE_TEST_VERSION
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>

#include "nvim/api/private/defs.h"

static void (*init_func)(void);
static Object (*eval_lua)(String str, Error *err);

int main(int argc, char **argv, char **env)
{
  if (argc <= 1)
    return 1;

  void *handle = dlopen("build/src/nvim/libnvim-test.so", RTLD_LAZY);

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

  eval_lua = dlsym(handle, "eval_lua");
  if (eval_lua == NULL)
    return 5;

  String arg = {
    .data = argv[1],
    .size = strlen(argv[1]),
  };
  Error err = {
    .set = false,
  };
  Object result = eval_lua(arg, &err);

  switch (result.type) {
    case kObjectTypeNil: {puts("kObjectTypeNil"); break;}
    case kObjectTypeBoolean: {
      puts("kObjectTypeBoolean");
      if (result.data.boolean)
        puts("true");
      else
        puts("false");
      break;
    }
    case kObjectTypeInteger: {
      puts("kObjectTypeInteger");
      printf("%" PRIi64 "\n", result.data.integer);
      break;
    }
    case kObjectTypeFloat: {
      puts("kObjectTypeFloat");
      printf("%lf\n", result.data.floating);
      break;
    }
    case kObjectTypeString: {
      puts("kObjectTypeString");
      fwrite(result.data.string.data, 1, result.data.string.size, stdout);
      break;
    }
    case kObjectTypeArray: {puts("kObjectTypeArray"); break;}
    case kObjectTypeDictionary: {puts("kObjectTypeDictionary"); break;}
  }
  return 0;
}
#endif  // COMPILE_TEST_VERSION
int abc3() {
  return 0;
}
