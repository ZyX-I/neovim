ffi = require 'ffi'
lpeg = require 'lpeg'
formatc = require 'test.unit.formatc'
Set = require 'test.unit.set'
Preprocess = require 'test.unit.preprocess'

-- add some standard header locations
-- TODO(aktau, jszakmeister): optionally pass more header locations via env
Preprocess.add_to_include_path('./src')
Preprocess.add_to_include_path('./.deps/usr/include')
Preprocess.add_to_include_path('./build/config')

if ffi.abi('32bit')
  Preprocess.add_to_include_path('/opt/neovim-deps/32/include')
else
  Preprocess.add_to_include_path('/opt/neovim-deps/include')

-- load neovim shared library
testlib = os.getenv 'NVIM_TEST_LIB'
unless testlib
    testlib = './build/src/libnvim-test.so'

libnvim = ffi.load testlib

trim = (s) ->
  s\match'^%s*(.*%S)' or ''

-- a Set that keeps around the lines we've already seen
export cdefs
if cdefs == nil
  cdefs = Set!

export imported
if imported == nil
  imported = Set!

-- some things are just too complex for the LuaJIT C parser to digest. We
-- usually don't need them anyway.
filter_complex_blocks = (body) ->
  result = {}
  for line in body\gmatch("[^\r\n]+")
    -- remove all lines that contain Objective-C block syntax, the LuaJIT ffi
    -- doesn't understand it.
    if string.find(line, "(^)", 1, true) ~= nil
      continue
    if string.find(line, "_ISwupper", 1, true) ~= nil
      continue
    result[#result + 1] = line
  table.concat(result, "\n")

-- use this helper to import C files, you can pass multiple paths at once,
-- this helper will return the C namespace of the nvim library.
-- cimport = (path) ->
cimport = (...) ->
  -- filter out paths we've already imported
  paths = [path for path in *{...} when not imported\contains(path)]
  for path in *paths
    imported\add(path)

  if #paths == 0
    return libnvim

  -- preprocess the header
  stream = Preprocess.preprocess_stream(unpack(paths))
  body = stream\read("*a")
  stream\close!

  -- format it (so that the lines are "unique" statements), also filter out
  -- Objective-C blocks
  body = formatc(body)
  body = filter_complex_blocks(body)

  -- add the formatted lines to a set
  new_cdefs = Set!
  for line in body\gmatch("[^\r\n]+")
    new_cdefs\add(trim(line))

  -- subtract the lines we've already imported from the new lines, then add
  -- the new unique lines to the old lines (so they won't be imported again)
  new_cdefs\diff(cdefs)
  cdefs\union(new_cdefs)

  if new_cdefs\size! == 0
    -- if there's no new lines, just return
    return libnvim

  -- request a sorted version of the new lines (same relative order as the
  -- original preprocessed file) and feed that to the LuaJIT ffi
  new_lines = new_cdefs\to_table!
  ffi.cdef(table.concat(new_lines, "\n"))

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

export vim_initialized

if not vim_initialized
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

  vim_initialized = true

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
