{:cimport, :internalize, :eq, :ffi, :lib, :cstr, :to_cstr} = require 'test.unit.helpers'

cmd = cimport './src/cmd.h'

-- TODO: something better for forward_search and backward_search
address = {
  [0]: {'missing',             (addr) -> 'N/A'},
  {'fixed',                    (addr) -> string.format('line: %u', addr.data.lnr)},
  {'end',                      (addr) -> '$'},
  {'current',                  (addr) -> '.'},
  {'mark',                     (addr) -> addr.data.mark},
  {'forward_search',           (addr) -> '/'},
  {'backward_search',          (addr) -> '?'},
  {'forward_previous_search',  (addr) -> '\\/'},
  {'backward_previous_search', (addr) -> '\\?'},
  {'substitute_search',        (addr) -> '\\&'},
}

-- TODO: something better for forward_pattern and backward_pattern
followup = {
  [0]: {'missing',             (fup) -> 'N/A'},
  {'forward_pattern',          (fup) -> '/'},
  {'backward_pattern',         (fup) -> '?'},
  {'shift',                    (fup) -> string.format('shift: %i', fup.data.shift)}
}

fup_to_string = (fup) ->
  if not fup
    return ''
  type = tonumber(fup.type)
  return followup[type][2](fup) .. ';' .. fup_to_string(fup.next)

address_to_string = (addr) ->
  type = tonumber(addr.type)
  return address[type][2](addr)

range_to_strings = (range, result, i) ->
  start_type = tonumber(range.start.type)
  end_type = tonumber(range.end.type)
  insert_setpos = false
  if start_type ~= 0
    result[i]   = 'start: ' .. address_to_string(range.start)
    result[i] ..= '//' .. fup_to_string(range.start_followups)
    i += 1
    insert_setpos = true
  if end_type ~= 0
    result[i] = 'end: ' .. address_to_string(range.end)
    result[i] ..= '//' .. fup_to_string(range.end_followups)
    i += 1
    insert_setpos = true
  if insert_setpos
    result[i] = 'setpos: ' .. tonumber(range.setpos)
    i += 1
  return result, i

node_to_strings = (node) ->
  result = {}
  i = 0
  result[i] = 'count: ' .. tonumber(node.arg.count)
  i += 1
  range_to_strings(node.range, result, i)
  return result

p1ct = (str, flags=0) ->
  s = to_cstr(str)
  return ffi.gc(cmd.parse_one_cmd_test(s, flags), cmd.free_cmd)

eqn = (lines, cmd, count=0) ->
  lines[0] = 'count: ' .. count
  eq lines, node_to_strings p1ct cmd

describe 'parse_one_cmd', ->
  it 'parses intro', ->
    eqn {}, 'intro'
  it 'parses :intro', ->
    eqn {}, ':intro'

-- vim: sw=2 sts=2 et tw=80
