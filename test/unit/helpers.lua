local ffi = require('ffi')

-- load neovim shared library
local libnvim = ffi.load('./build/src/libnvim-test.so')

-- Luajit ffi parser only understands function signatures.
-- This helper function normalizes headers, passes to ffi and returns the
-- library pointer
local cimport = function(path)
  if path then
    local pipe = io.popen('cpp -P -E ' .. path)
    local header = pipe:read("*a")
    pipe:close()
    ffi.cdef(header)
  end
  return libnvim
end

-- take a pointer to a C-allocated string and return an interned
-- version while also freeing the memory
local internalize = function(cdata)
  ffi.gc(cdata, ffi.C.free)
  return ffi.string(cdata)
end

return {
  cimport = cimport,
  internalize = internalize,
  eq = function(expected, actual)
    return assert.are.same(expected, actual)
  end,
  ffi = ffi
}

-- vim: sw=2 sts=2 tw=80
