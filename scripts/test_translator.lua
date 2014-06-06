#!/usr/bin/luajit

ffi = require 'ffi'

cstr = ffi.typeof 'char[?]'

to_cstr = (function(string)
  return cstr(string:len() + 1, string)
end)

libnvim = ffi.load './build/src/libnvim-test.so'

cimport = (function(path)
  pipe = io.popen('cc -Isrc -P -E ' .. path)
  header = pipe:read('*a')
  pipe:close()
  header = header:gsub('%c[^%c]*1UL[^%c]*', '')
  ffi.cdef(header)
end)

cimport 'src/vim.h'
-- cimport 'src/translator/translator/translator.h'
ffi.cdef('int translate_script_str_to_file(char_u *, char *);')
cimport 'src/os_unix.h'
cimport 'src/eval.h'
-- cimport 'src/main.h'
ffi.cdef('void allocate_generic_buffers(void);')
cimport 'src/window.h'
cimport 'src/ops.h'
-- cimport 'src/misc1.h'
ffi.cdef('void init_homedir(void);')
cimport 'src/option.h'
cimport 'src/ex_cmds2.h'

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

local s
if(arg[1]) then
  s = arg[1]
else
  s = io.stdin:read('*a')
end
libnvim.translate_script_str_to_file(to_cstr(s), to_cstr('test.lua'))

package.path = './src/translator/executor/?.lua;' .. package.path

vim = require 'vim'
test = require 'test'
state = vim.new_state()
test.run(state)
