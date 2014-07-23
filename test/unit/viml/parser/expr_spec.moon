{:cimport, :internalize, :eq, :ffi, :lib, :cstr, :to_cstr, :vim_init} = require 'test.unit.helpers'

vim_init()

libnvim = cimport('./src/nvim/viml/testhelpers/parser.h')

p0 = (str) ->
  s = to_cstr(str)
  parsed = libnvim.srepresent_parse0(s, false)
  if parsed == nil
    error('srepresent_parse0 returned nil')
  return ffi.string(parsed)

eqn = (expected_result, expr, expected_offset=nil) ->
  if not expected_offset
    expected_offset = expr\len()

  result = p0 expr

  expected_result = string.format('%X:%s', expected_offset, expected_result)

  eq expected_result, result

describe 'parse0', ->
  it 'parses number 0', ->
    eqn 'N[+0+]', '0'
  it 'parses number 10', ->
    eqn 'N[+10+]', '10'
  it 'parses number 110', ->
    eqn 'N[+110+]', '110'
  it 'parses number 01900', ->
    eqn 'N[+01900+]', '01900'
  it 'parses octal number 010', ->
    eqn 'O[+010+]', '010'
  it 'parses octal number 0000015', ->
    eqn 'O[+0000015+]', '0000015'
  it 'parses hex number 0x1C', ->
    eqn 'X[+0x1C+]', '0x1C'
  it 'parses hex number 0X1C', ->
    eqn 'X[+0X1C+]', '0X1C'
  it 'parses hex number 0X1c', ->
    eqn 'X[+0X1c+]', '0X1c'
  it 'parses hex number 0x1c', ->
    eqn 'X[+0x1c+]', '0x1c'
  it 'parses float 0.0', ->
    eqn 'F[+0.0+]', '0.0'
  it 'parses float 0.0e0', ->
    eqn 'F[+0.0e0+]', '0.0e0'
  it 'parses float 0.1e+1', ->
    eqn 'F[+0.1e+1+]', '0.1e+1'
  it 'parses float 0.1e-1', ->
    eqn 'F[+0.1e-1+]', '0.1e-1'
  it 'parses "abc"', ->
    eqn '"[+"abc"+]', '"abc"'
  it 'parses "a\\"bc"', ->
    eqn '"[+"a\\"bc"+]', '"a\\"bc"'
  it 'parses \'abc\'', ->
    eqn '\'[+\'abc\'+]', '\'abc\''
  it 'parses \'ab\'\'c\'', ->
    eqn '\'[+\'ab\'\'c\'+]', '\'ab\'\'c\''
  it 'parses \'ab\'\'\'', ->
    eqn '\'[+\'ab\'\'\'+]', '\'ab\'\'\''
  it 'parses \'\'\'c\'', ->
    eqn '\'[+\'\'\'c\'+]', '\'\'\'c\''
  it 'parses option', ->
    eqn '&[+abc+]', '&abc'
  it 'parses local option', ->
    eqn '&[+l:abc+]', '&l:abc'
  it 'parses global option', ->
    eqn '&[+g:abc+]', '&g:abc'
  it 'parses register r', ->
    eqn '@[+@r+]', '@r'
  it 'parses register NUL', ->
    eqn '@[+@+]', '@'
  it 'parses environment variable', ->
    eqn '$[+abc+]', '$abc'
  it 'parses varname', ->
    eqn 'var[+varname+]', 'varname'
  it 'parses g:varname', ->
    eqn 'var[+g:varname+]', 'g:varname'
  it 'parses abc:func', ->
    eqn 'var[+abc:func+]', 'abc:func'
  it 'parses s:v', ->
    eqn 'var[+s:v+]', 's:v'
  it 'parses s:', ->
    eqn 'var[+s:+]', 's:'
  it 'parses <SID>v', ->
    eqn 'var[+<SID>v+]', '<SID>v'
  it 'parses abc#def', ->
    eqn 'var[+abc#def+]', 'abc#def'
  it 'parses g:abc#def', ->
    eqn 'var[+g:abc#def+]', 'g:abc#def'
  it 'parses <SNR>12_v', ->
    eqn 'var[+<SNR>12_v+]', '<SNR>12_v'
  it 'parses curly braces name: v{a}', ->
    eqn 'cvar(id[+v+], curly[!{!](var[+a+]))', 'v{a}'
  it 'parses curly braces name: {a}', ->
    eqn 'cvar(curly[!{!](var[+a+]))', '{a}'
  it 'parses curly braces name: {a}b', ->
    eqn 'cvar(curly[!{!](var[+a+]), id[+b+])', '{a}b'
  it 'parses curly braces name: x{a}b', ->
    eqn 'cvar(id[+x+], curly[!{!](var[+a+]), id[+b+])', 'x{a}b'
  it 'parses curly braces name: x{a}1', ->
    eqn 'cvar(id[+x+], curly[!{!](var[+a+]), id[+1+])', 'x{a}1'
  it 'parses abc.key', ->
    eqn '.[+key+](var[+abc+])', 'abc.key'
  it 'parses abc.key.2', ->
    eqn '.[+2+](.[+key+](var[+abc+]))', 'abc.key.2'
  it 'parses abc.g:v', ->
    eqn '..(var[+abc+], var[+g:v+])', 'abc.g:v'
  it 'parses abc.autoload#var', ->
    eqn '..(var[+abc+], var[+autoload#var+])', 'abc.autoload#var'
  it 'parses 1.2.3.4', ->
    eqn '..(N[+1+], N[+2+], N[+3+], N[+4+])', '1.2.3.4'
  it 'parses "abc".def', ->
    eqn '..("[+"abc"+], var[+def+])', '"abc".def'
  it 'parses 1 . 2 . 3 . 4', ->
    eqn '..(N[+1+], N[+2+], N[+3+], N[+4+])', '1 . 2 . 3 . 4'
  it 'parses 1. 2. 3. 4', ->
    eqn '..(N[+1+], N[+2+], N[+3+], N[+4+])', '1. 2. 3. 4'
  it 'parses 1 .2 .3 .4', ->
    eqn '..(N[+1+], N[+2+], N[+3+], N[+4+])', '1 .2 .3 .4'
  it 'parses a && b && c', ->
    eqn '&&(var[+a+], var[+b+], var[+c+])', 'a && b && c'
  it 'parses a || b || c', ->
    eqn '||(var[+a+], var[+b+], var[+c+])', 'a || b || c'
  it 'parses a || b && c || d', ->
    eqn '||(var[+a+], &&(var[+b+], var[+c+]), var[+d+])', 'a || b && c || d'
  it 'parses a && b || c && d', ->
    eqn '||(&&(var[+a+], var[+b+]), &&(var[+c+], var[+d+]))',
      'a && b || c && d'
  it 'parses a && (b || c) && d', ->
    eqn '&&(var[+a+], expr[!(!](||(var[+b+], var[+c+])), var[+d+])',
      'a && (b || c) && d'
  it 'parses a + b + c*d/e/f  - g % h .i', ->
    str   = '..(-(+(var[+a+], '
    str ..=     'var[+b+], '
    str ..=    '/(*(var[+c+], '
    str ..=        'var[+d+]), '
    str ..=      'var[+e+], '
    str ..=      'var[+f+])), '
    str ..=    '%(var[+g+], var[+h+])), '
    str ..=    'var[+i+])'
    eqn str, 'a + b + c*d/e/f  - g % h .i'
  it 'parses !+-!!++a', ->
    eqn '!(+!(-!(!(!(+!(+!(var[+a+])))))))', '!+-!!++a'
  it 'parses (abc)', ->
    eqn 'expr[!(!](var[+abc+])', '(abc)'
  it 'parses [1, 2 , 3 ,4]', ->
    eqn '[][![!](N[+1+], N[+2+], N[+3+], N[+4+])', '[1, 2 , 3 ,4]'
  it 'parses {1:2, v : c, (10): abc}', ->
    str   = '{}[!{!](N[+1+], N[+2+], '
    str ..=         'var[+v+], var[+c+], '
    str ..=         'expr[!(!](N[+10+]), var[+abc+])'
    eqn str, '{1:2, v : c, (10): abc}'
  it 'parses 1 == 2 && 3 != 4 && 5 > 6 && 7 < 8', ->
    str   = '&&(==(N[+1+], N[+2+]), !=(N[+3+], N[+4+]), >(N[+5+], N[+6+]), '
    str ..=    '<(N[+7+], N[+8+]))'
    eqn str, '1 == 2 && 3 != 4 && 5 > 6 && 7 < 8'
  it 'parses "" ># "a" || "" <? "b" || "" is "c"', ->
    str   = '||(>#("[+""+], "[+"a"+]), <?("[+""+], "[+"b"+]), '
    str ..=    'is("[+""+], "[+"c"+]))'
    eqn str, '"" ># "a" || "" <? "b" || "" is "c"'
  it 'parses 1== 2 &&  1 ==#2 && 1==?2', ->
    eqn ('&&(==(N[+1+], N[+2+]), ==#(N[+1+], N[+2+]), ==?(N[+1+], N[+2+]))'),
      '1== 2 &&  1 ==#2 && 1==?2'
  it 'parses 1!= 2 &&  1 !=#2 && 1!=?2', ->
    eqn ('&&(!=(N[+1+], N[+2+]), !=#(N[+1+], N[+2+]), !=?(N[+1+], N[+2+]))'),
      '1!= 2 &&  1 !=#2 && 1!=?2'
  it 'parses 1> 2 &&  1 >#2 && 1>?2', ->
    eqn ('&&(>(N[+1+], N[+2+]), >#(N[+1+], N[+2+]), >?(N[+1+], N[+2+]))'),
      '1> 2 &&  1 >#2 && 1>?2'
  it 'parses 1< 2 &&  1 <#2 && 1<?2', ->
    eqn ('&&(<(N[+1+], N[+2+]), <#(N[+1+], N[+2+]), <?(N[+1+], N[+2+]))'),
      '1< 2 &&  1 <#2 && 1<?2'
  it 'parses 1<= 2 &&  1 <=#2 && 1<=?2', ->
    eqn ('&&(<=(N[+1+], N[+2+]), <=#(N[+1+], N[+2+]), <=?(N[+1+], N[+2+]))'),
      '1<= 2 &&  1 <=#2 && 1<=?2'
  it 'parses 1>= 2 &&  1 >=#2 && 1>=?2', ->
    eqn ('&&(>=(N[+1+], N[+2+]), >=#(N[+1+], N[+2+]), >=?(N[+1+], N[+2+]))'),
      '1>= 2 &&  1 >=#2 && 1>=?2'
  it 'parses 1is 2 &&  1 is#2 && 1is?2', ->
    eqn ('&&(is(N[+1+], N[+2+]), is#(N[+1+], N[+2+]), is?(N[+1+], N[+2+]))'),
      '1is 2 &&  1 is#2 && 1is?2'
  it 'parses 1isnot 2 &&  1 isnot#2 && 1isnot?2', ->
    str   = '&&(isnot(N[+1+], N[+2+]), isnot#(N[+1+], N[+2+]), '
    str ..=    'isnot?(N[+1+], N[+2+]))'
    eqn str, '1isnot 2 &&  1 isnot#2 && 1isnot?2'
  it 'parses 1=~ 2 &&  1 =~#2 && 1=~?2', ->
    eqn ('&&(=~(N[+1+], N[+2+]), =~#(N[+1+], N[+2+]), =~?(N[+1+], N[+2+]))'),
      '1=~ 2 &&  1 =~#2 && 1=~?2'
  it 'parses 1!~ 2 &&  1 !~#2 && 1!~?2', ->
    eqn ('&&(!~(N[+1+], N[+2+]), !~#(N[+1+], N[+2+]), !~?(N[+1+], N[+2+]))'),
      '1!~ 2 &&  1 !~#2 && 1!~?2'
  it 'parses call(1, 2, 3, 4, 5)', ->
    eqn ('call(var[+call+], N[+1+], N[+2+], N[+3+], N[+4+], N[+5+])'),
      'call(1, 2, 3, 4, 5)'
  it 'parses (1)(2)', ->
    eqn 'call(expr[!(!](N[+1+]), N[+2+])', '(1)(2)'
  -- TODO see #251
  -- it('parses call (def)', function()
    -- eq 'call(var[+call+], var[+def+])', 'call (def)'
  it 'parses [][1]', ->
    eqn 'index([][![!], N[+1+])', '[][1]'
  it 'parses [][1:]', ->
    eqn 'index([][![!], N[+1+], empty[!]!])', '[][1:]'
  it 'parses [][:1]', ->
    eqn 'index([][![!], empty[!:!], N[+1+])', '[][:1]'
  it 'parses [][:]', ->
    eqn 'index([][![!], empty[!:!], empty[!]!])', '[][:]'
  it 'parses [][i1 : i2]', ->
    eqn 'index([][![!], var[+i1+], var[+i2+])', '[][i1 : i2]'

  it 'partially parses (abc)(def) (ghi)', ->
    eqn 'call(expr[!(!](var[+abc+]), var[+def+])', '(abc)(def) (ghi)', 11

  it 'fully parses tr>--(1, 2, 3)', ->
    eqn 'call(var[+tr+], N[+1+], N[+2+], N[+3+])', 'tr\t(1, 2, 3)'

  it 'parses s:pls[plid].runtimepath is# pltrp', ->
    eqn 'is#(.[+runtimepath+](index(var[+s:pls+], var[+plid+])), var[+pltrp+])',
      's:pls[plid].runtimepath is# pltrp'
  it 'parses abc[def] is# 123', ->
    eqn 'is#(index(var[+abc+], var[+def+]), N[+123+])', 'abc[def] is# 123'

describe 'parse0, failures', ->
  it 'fails to parse [1;2]', ->
    eqn 'error:E696: Missing comma in List', '[1;2]', 2
  it 'fails to parse <', ->
    eqn 'error:E15: expected expr7 (value)', '<', 0

-- vim: sw=2 sts=2 et tw=80
