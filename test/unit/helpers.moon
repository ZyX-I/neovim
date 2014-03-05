ffi = require 'ffi'

-- load neovim shared library
libnvim = ffi.load './build/src/libnvim-test.so'

cimport = (path) ->
  if path
    pipe = io.popen('cpp -P -E ' .. path)
    header = pipe\read('*a')
    pipe\close()
    ffi.cdef(header)

  return libnvim

cimport './src/types.h'

-- take a pointer to a C-allocated string and return an interned
-- version while also freeing the memory
internalize = (cdata) ->
  ffi.gc cdata, ffi.C.free
  return ffi.string cdata

return {
  cimport: cimport
  internalize: internalize
  eq: (expected, actual) -> assert.are.same expected, actual
  ffi: ffi
  lib: libnvim
  cstr: ffi.typeof 'char[?]'
}
