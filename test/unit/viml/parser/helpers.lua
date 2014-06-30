local cimport, internalize, eq, ffi, lib, cstr, to_cstr, vim_init
do
  local _obj_0 = require('test.unit.helpers')
  cimport, internalize, eq, ffi, lib, cstr, to_cstr = _obj_0.cimport, _obj_0.internalize, _obj_0.eq, _obj_0.ffi, _obj_0.lib, _obj_0.cstr, _obj_0.to_cstr
  vim_init = _obj_0.vim_init
end

vim_init()

local libnvim = cimport('./src/nvim/viml/testhelpers/parser.h')
local p1ct
p1ct = function(str, one, flags)
  if flags == nil then
    flags = 0
  end
  local s = to_cstr(str)
  local result = libnvim.parse_cmd_test(s, flags, one, false)
  if result == nil then
    error('parse_cmd_test returned nil')
  end
  return ffi.string(result)
end
local eqn
eqn = function(expected_result, cmd, one, flags)
  if flags == nil then
    flags = 0
  end
  if cmd == nil then
    cmd = expected_result
  end
  local result = p1ct(cmd, one, flags)
  return eq(expected_result, result)
end
local itn
itn = function(expected_result, cmd, flags)
  if cmd == nil then
    cmd = nil
  end
  if flags == nil then
    flags = 0
  end
  return it('parses ' .. (cmd or expected_result), function()
    return eqn(expected_result, cmd, true, flags)
  end)
end
local t
t = function(expected_result, cmd, flags)
  if cmd == nil then
    cmd = nil
  end
  if flags == nil then
    flags = 0
  end
  return it('parses ' .. (cmd or expected_result), function()
    local expected = expected_result
    expected = expected:gsub('^%s*\n', '')
    local first_indent = expected:match('^%s*')
    expected = expected:gsub('^' .. first_indent, '')
    expected = expected:gsub('\n' .. first_indent, '\n')
    expected = expected:gsub('\n%s*$', '')
    return eqn(expected, cmd, false, flags)
  end)
end
return {
  t = t,
  itn = itn
}
