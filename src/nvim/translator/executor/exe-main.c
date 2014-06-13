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

void print_object(Object obj)
{
  switch (obj.type) {
    case kObjectTypeNil: {puts("kObjectTypeNil"); break;}
    case kObjectTypeBoolean: {
      puts("kObjectTypeBoolean");
      if (obj.data.boolean)
        puts("true");
      else
        puts("false");
      break;
    }
    case kObjectTypeInteger: {
      puts("kObjectTypeInteger");
      printf("%" PRIi64 "\n", obj.data.integer);
      break;
    }
    case kObjectTypeFloat: {
      puts("kObjectTypeFloat");
      printf("%lf\n", obj.data.floating);
      break;
    }
    case kObjectTypeString: {
      puts("kObjectTypeString");
      fwrite(obj.data.string.data, 1, obj.data.string.size, stdout);
      putchar('\n');
      break;
    }
    case kObjectTypeArray: {
      puts("kObjectTypeArray [");
      for (size_t i = 0; i < obj.data.array.size; i++)
        print_object(obj.data.array.items[i]);
      puts("]");
      break;
    }
    case kObjectTypeDictionary: {
      puts("kObjectTypeDictionary {");
      for (size_t i = 0; i < obj.data.dictionary.size; i++) {
        Object new_obj = {.type = kObjectTypeString};
        new_obj.data.string = obj.data.dictionary.items[i].key;
        print_object(new_obj);
        print_object(obj.data.dictionary.items[i].value);
      }
      puts("}");
      break;
    }
  }
}

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

  print_object(result);
  if (err.set) {
    puts("Error was set:");
    puts(err.msg);
  }

  return 0;
}
#endif  // COMPILE_TEST_VERSION
int abc3() {
  return 0;
}
