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
  it 'parses 1,2|', ->
    eqn '1,2print', '1,2|'
  it 'parses 1', ->
    eqn '1', '1'
  it 'parses aboveleft join', ->
    eqn 'aboveleft join', 'aboveleft join'
  it 'parses abo join', ->
    eqn 'aboveleft join', 'abo join'
  it 'parses bel join', ->
    eqn 'belowright join', 'bel join'
  it 'parses bot join', ->
    eqn 'botright join', 'bot join'
  it 'parses bro join', ->
    eqn 'browse join', 'bro join'
  it 'parses conf join', ->
    eqn 'confirm join', 'conf join'
  it 'parses hid join', ->
    eqn 'hide join', 'hid join'
  it 'parses keepa join', ->
    eqn 'keepalt join', 'keepa join'
  it 'parses keepp join', ->
    eqn 'keeppatterns join', 'keepp join'
  it 'parses keepj join', ->
    eqn 'keepjumps join', 'keepj join'
  it 'parses keepm join', ->
    eqn 'keepmarks join', 'keepm join'
  it 'parses 1,2', ->
    eqn '1,2', '1,2'
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
  it 'parses :1+1;7+1+2+3join', ->
    eqn '1+1;7+1+2+3join', ':1+1;7+1+2+3join'
  it 'parses :join #', ->
    eqn 'join #', ':join #'
  it 'parses :join 5 #', ->
    eqn 'join 5 #', ':join 5 #'
  it 'parses :join#', ->
    eqn 'join #', ':join#'
  it 'parses :join5#', ->
    eqn 'join 5 #', ':join5#'
  it 'parses :j#', ->
    eqn 'join #', ':j#'
  it 'parses :num#', ->
    eqn 'number #', ':num#'
  it 'parses :p l', ->
    eqn 'print l', ':p l'
  it 'parses :##', ->
    eqn '# #', ':##'
  it 'parses :#l', ->
    eqn '# l', ':#l'
  it 'parses :=l', ->
    eqn '= l', ':=l'
  it 'parses :>l', ->
    eqn '> l', ':>l'
  it 'parses :num5#', ->
    eqn 'number 5 #', ':num5#'
  it 'parses :p 5l', ->
    eqn 'print 5 l', ':p 5l'
  it 'parses :#1#', ->
    eqn '# 1 #', ':#1#'
  it 'parses :>3l', ->
    eqn '> 3 l', ':>3l'
  it 'parses :<3', ->
    eqn '< 3', ':<3'
  it 'parses :<', ->
    eqn '<', ':<'
  it 'parses :all', ->
    eqn 'all', ':all'
  it 'parses :5all', ->
    eqn '5all', ':5all'
  it 'parses :all5', ->
    eqn 'all 5', ':all5'
  it 'parses :al5', ->
    eqn 'all 5', ':al5'
  it 'parses :ascii', ->
    eqn 'ascii', ':ascii'
  it 'parses :bN', ->
    eqn 'bNext', ':bNext'
  it 'parses :bN5', ->
    eqn 'bNext 5', ':bNext5'
  it 'parses :ba', ->
    eqn 'ball', ':ba'
  it 'parses :bf', ->
    eqn 'bfirst', ':bf'
  it 'parses :bl', ->
    eqn 'blast', ':bl'
  it 'parses :bm!', ->
    eqn 'bmodified!', ':bm!'
  it 'parses :bn', ->
    eqn 'bnext', ':bn'
  it 'parses :bp5', ->
    eqn 'bprevious 5', ':bp5'
  it 'parses :br', ->
    eqn 'brewind', ':br'
  it 'parses :brea', ->
    eqn 'break', ':brea'
  it 'parses :breakl', ->
    eqn 'breaklist', ':breakl'
  it 'parses :buffers', ->
    eqn 'buffers', ':buffers'
  it 'parses :cN', ->
    eqn 'cNext', ':cN'
  it 'parses :X', ->
    eqn 'X', ':X'
  it 'parses :xa', ->
    eqn 'xall', ':xa'
  it 'parses :wa', ->
    eqn 'wall', ':wa'
  it 'parses :viu', ->
    eqn 'viusage', ':viu'
  it 'parses :ver', ->
    eqn 'version', ':ver'
  it 'parses :unh', ->
    eqn 'unhide', ':unh'
  it 'parses :undol', ->
    eqn 'undolist', ':undol'
  it 'parses :undoj', ->
    eqn 'undojoin', ':undoj'
  it 'parses :undo', ->
    eqn 'undo 5', ':u5'
  it 'parses :try', ->
    eqn 'try', ':try'
  it 'parses :tr', ->
    eqn 'trewind', ':tr'
  it 'parses :tp', ->
    eqn 'tprevious', ':tp'
  it 'parses :tn', ->
    eqn 'tnext', ':tn'
  it 'parses :tl', ->
    eqn 'tlast', ':tl'
  it 'parses :tf', ->
    eqn 'tfirst', ':tf'

-- vim: sw=2 sts=2 et tw=80
