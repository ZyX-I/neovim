{:vim_init, :cimport, :internalize, :eq, :ffi, :lib, :cstr, :to_cstr} = require 'test.unit.helpers'

vim_init()

libnvim = cimport('./src/nvim/viml/testhelpers/parser.h')
p1ct = (str, one, flags=0) ->
  s = to_cstr(str)
  result = libnvim.parse_cmd_test(s, flags, one, false)
  if result == nil
    error('parse_cmd_test returned nil')
  return ffi.string(result)

eqn = (expected_result, cmd, one, flags=0) ->
  if cmd == nil
    cmd = expected_result
  result = p1ct cmd, one, flags
  eq expected_result, result

itn = (expected_result, cmd=nil, flags=0) ->
  it 'parses ' .. (cmd or expected_result), ->
    eqn expected_result, cmd, true, flags

t = (expected_result, cmd=nil, flags=0) ->
  it 'parses ' .. (cmd or expected_result), ->
    expected = expected_result
    expected = expected\gsub('^%s*\n', '')
    first_indent = expected\match('^%s*')
    expected = expected\gsub('^' .. first_indent, '')
    expected = expected\gsub('\n' .. first_indent, '\n')
    expected = expected\gsub('\n%s*$', '')
    eqn expected, cmd, false, flags

return {
  t: t
  itn: itn
}
