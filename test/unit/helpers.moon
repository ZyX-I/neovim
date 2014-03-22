ffi = require 'ffi'

-- load neovim shared library
testlib = os.getenv 'NVIM_TEST_LIB'
unless testlib
    testlib = './build/src/libnvim-test.so'

libnvim = ffi.load testlib

export imported = {}

cimport = (path) ->
  if path and not imported[path]
    imported[path] = true

    -- pipe = io.popen('cpp -M -E ' .. path)
    -- deps = pipe\read('*a')
    -- pipe\close()

    -- idx = string.find(deps, ' ')
    -- deps = string.sub(deps, idx + 1)
    -- deps = string.gsub(deps, '%s*\\%s*', ' ') .. ' '
    -- idx = string.find(deps, ' ')
    -- while idx
      -- imported[path]

    pipe = io.popen('cc -Isrc -P -E ' .. path)
    header = pipe\read('*a')
    pipe\close()
    ffi.cdef(header)

  return libnvim

testinc = os.getenv 'TEST_INCLUDES'
unless testinc
    testinc = './build/test/includes/post'

cppimport = (path) ->
  return cimport testinc .. '/' .. path

cimport './src/types.h'

-- take a pointer to a C-allocated string and return an interned
-- version while also freeing the memory
internalize = (cdata) ->
  ffi.gc cdata, ffi.C.free
  return ffi.string cdata

cstr = ffi.typeof 'char[?]'

to_cstr = (string) ->
  cstr (string.len string) + 1, string

-- cimport './src/os_unix.h'
ffi.cdef('void mch_early_init(void);')
-- cimport './src/eval.h'
ffi.cdef('void eval_init(void);')
-- cimport './src/main.h'
ffi.cdef('void allocate_generic_buffers(void);')
-- cimport './src/window.h'
ffi.cdef('int win_alloc_first(void);')
-- cimport './src/ops.h'
ffi.cdef('void init_yank(void);')
-- cimport './src/misc1.h'
ffi.cdef('void init_homedir(void);')
-- cimport './src/option.h'
ffi.cdef('void set_init_1(void);')
-- cimport './src/ex_cmds2.h'
ffi.cdef('void set_lang_var(void);')
-- cimport './src/mbyte.h'
ffi.cdef('char_u *mb_init(void);')
-- cimport './src/normal.h'
ffi.cdef('void init_normal_cmds(void);')

export garray_defined

if not garray_defined
  -- FIXME Required by os/users and cmd-parser
  ffi.cdef [[
  typedef struct growarray {
    int ga_len;
    int ga_maxlen;
    int ga_itemsize;
    int ga_growsize;
    void    *ga_data;
  } garray_T;
  ]]
  garray_defined = true

libnvim.mch_early_init()
libnvim.mb_init()
libnvim.eval_init()
libnvim.init_normal_cmds()
libnvim.allocate_generic_buffers()
libnvim.win_alloc_first()
libnvim.init_yank()
libnvim.init_homedir()
libnvim.set_init_1()
libnvim.set_lang_var()

return {
  cimport: cimport
  cppimport: cppimport
  internalize: internalize
  eq: (expected, actual) -> assert.are.same expected, actual
  neq: (expected, actual) -> assert.are_not.same expected, actual
  ffi: ffi
  lib: libnvim
  cstr: cstr
  to_cstr: to_cstr
}

-- vim: sw=2 sts=2 et tw=80
