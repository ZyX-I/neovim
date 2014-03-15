ffi = require 'ffi'

-- load neovim shared library
libnvim = ffi.load './build/src/libnvim-test.so'

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

    pipe = io.popen('cpp -P -E ' .. path)
    header = pipe\read('*a')
    pipe\close()
    ffi.cdef(header)

  return libnvim

cppimport = (path) ->
  return cimport './test/includes/post/' .. path

cimport './src/types.h'

-- take a pointer to a C-allocated string and return an interned
-- version while also freeing the memory
internalize = (cdata) ->
  ffi.gc cdata, ffi.C.free
  return ffi.string cdata

cstr = ffi.typeof 'char[?]'

to_cstr = (string) ->
  cstr (string.len string) + 1, string

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
