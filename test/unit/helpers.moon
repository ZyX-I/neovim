ffi = require 'ffi'

-- load neovim shared library
libnvim = ffi.load './build/src/libnvim-test.so'

-- Luajit ffi parser only understands function signatures.
-- This helper function normalizes headers, passes to ffi and returns the
-- library pointer
cimport = (path) ->
  if path
    pipe = io.popen('cpp -P ' .. path)
    header = pipe\read('*a')
    pipe\close()
    ffi.cdef(header)

  return libnvim

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
}
