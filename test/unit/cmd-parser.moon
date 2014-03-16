{:cimport, :internalize, :eq, :ffi, :lib, :cstr, :to_cstr} = require 'test.unit.helpers'

cmd = cimport './src/cmd.h'

p1ct = (str, flags=0) ->
  s = to_cstr(str)
  result = cmd.parse_one_cmd_test(s, flags)
  if result == nil
    error('parse_one_cmd_test returned nil')
  return ffi.string(result)

eqn = (expected_result, cmd, count=0) ->
  result = p1ct cmd
  eq expected_result, result

describe 'parse_one_cmd', ->
  it 'parses intro', ->
    eqn 'intro', 'intro'
  it 'parses :intro', ->
    eqn 'intro', ':intro'
  it 'parses :1,2join', ->
    eqn '1,2join', ':1,2join'
  it 'parses :1,2join!', ->
    eqn '1,2join!', ':::::::1,2join!'
  it 'parses :1,2,3,4join!', ->
    eqn '1,2,3,4join!', ':::::::1,2,3,4join!'
  it 'parses :1      join!', ->
    eqn '1join!', ':1      join!'
  it 'parses :%join', ->
    eqn '1,$join', ':%join'
  -- TODO: requires &p_cpo option to be initialized
  -- it 'parses :*join', ->
    -- eqn '\'<,\'>join', ':*join'

-- vim: sw=2 sts=2 et tw=80
