//FIXME!!!
#ifdef COMPILE_TEST_VERSION
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>

#include "nvim/vim.h"
#include "nvim/api/private/defs.h"

int do_init(void **);

static Object (*eval_lua)(String str, Error *err);

static Object (*execute_viml)(const char *const s);

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

  void *handle;

  int ret = do_init(&handle);
  if (ret)
    return ret + 1;

  if (argc == 3 && argv[1][0] == '-' && argv[1][1] == 'v') {
    execute_viml = dlsym(handle, "execute_viml");
    if (execute_viml == NULL)
      return 5;

    result = execute_viml(argv[2]);
  } else {
    eval_lua = dlsym(handle, "eval_lua");
    if (eval_lua == NULL)
      return 7;

    String arg = {
      .data = argv[1],
      .size = strlen(argv[1]),
    };
    Error err = {
      .set = false,
    };
    result = eval_lua(arg, &err);

    if (err.set) {
      puts("Error was set:");
      puts(err.msg);
    }
  }

  print_object(result);

  return 0;
}
#endif  // COMPILE_TEST_VERSION
int abc3() {
  return 0;
}
