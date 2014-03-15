{:cimport, :internalize, :eq, :ffi, :lib, :cstr, :to_cstr} = require 'test.unit.helpers'

expr = cimport('./src/expr.h')
cstr = ffi.typeof('char[?]')

expression_type = {
  [0]: 'Unknown',
  '?:',
  '||',
  '&&',
  '>',
  '>=',
  '<',
  '<=',
  '==',
  '!=',
  'is',
  'isnot',
  '=~',
  '!~',
  '+',
  '-',
  '..',
  '*',
  '/',
  '%',
  '!',
  '-!',
  '+!',
  'N',
  'O',
  'X',
  'F',
  '"',
  '\'',
  '&',
  '@',
  '$',
  'cvar',
  'var',
  'id',
  'curly',
  'expr',
  '[]',
  '{}',
  'index',
  '.',
  'call',
  'empty',
}

case_compare_strategy = {
  [0]: '',
  '#',
  '?',
}

node_to_string = (node, err, offset) ->
  type = expression_type[tonumber(node.type)]
  case_suffix = case_compare_strategy[tonumber(node.ignore_case)]
  func = type .. case_suffix
  result = func

  if (err and err.message ~= nil)
    result = 'error(' .. ffi.string(err.message) .. ')'
    return result

  if (node.position ~= nil)
    str = ffi.string(node.position)
    if (node.end_position ~= nil)
      len = node.end_position - node.position + 1
      result = result .. '[+' .. str\sub(1, len) .. '+]'
    else
      result = result .. '[!' .. str\sub(1, 1) .. '!]'

  if (node.children ~= nil)
    result = (result .. '(' .. node_to_string(node.children, nil, nil) .. ')')

  if (node.next ~= nil)
    result = result .. ', ' .. node_to_string(node.next, nil, nil)

  return result

describe 'parse0', ->
  p0 = (str) ->
    s = cstr(string.len(str), str)
    parsed = ffi.gc(expr.parse0_test(s), expr.free_test_expr_result)
    offset = parsed['end'] - s
    return node_to_string(parsed.node, parsed.error, offset)

  it 'parses number 0', ->
    eq 'N[+0+]', p0'0'
  it 'parses number 10', ->
    eq 'N[+10+]', p0'10'
  it 'parses number 110', ->
    eq 'N[+110+]', p0'110'
  it 'parses number 01900', ->
    eq 'N[+01900+]', p0'01900'
  it 'parses octal number 010', ->
    eq 'O[+010+]', p0'010'
  it 'parses octal number 0000015', ->
    eq 'O[+0000015+]', p0'0000015'
  it 'parses hex number 0x1C', ->
    eq 'X[+0x1C+]', p0'0x1C'
  it 'parses hex number 0X1C', ->
    eq 'X[+0X1C+]', p0'0X1C'
  it 'parses hex number 0X1c', ->
    eq 'X[+0X1c+]', p0'0X1c'
  it 'parses hex number 0x1c', ->
    eq 'X[+0x1c+]', p0'0x1c'
  it 'parses float 0.0', ->
    eq 'F[+0.0+]', p0'0.0'
  it 'parses float 0.0e0', ->
    eq 'F[+0.0e0+]', p0'0.0e0'
  it 'parses float 0.1e+1', ->
    eq 'F[+0.1e+1+]', p0'0.1e+1'
  it 'parses float 0.1e-1', ->
    eq 'F[+0.1e-1+]', p0'0.1e-1'
  it 'parses "abc"', ->
    eq '"[+"abc"+]', p0'"abc"'
  it 'parses "a\\"bc"', ->
    eq '"[+"a\\"bc"+]', p0'"a\\"bc"'
  it 'parses \'abc\'', ->
    eq '\'[+\'abc\'+]', p0'\'abc\''
  it 'parses \'a\'\'bc\'', ->
    eq '\'[+\'ab\'\'c\'+]', p0'\'ab\'\'c\''
  it 'parses option', ->
    eq '&[+abc+]', p0'&abc'
  it 'parses local option', ->
    eq '&[+l:abc+]', p0'&l:abc'
  it 'parses global option', ->
    eq '&[+g:abc+]', p0'&g:abc'
  it 'parses register r', ->
    eq '@[+@r+]', p0'@r'
  it 'parses register NUL', ->
    eq '@[+@+]', p0'@'
  -- TODO Requires chartab to be initialized
  -- it('parses environment variable', function()
    -- eq '$[+abc+]', p0'$abc'
  it 'parses varname', ->
    eq 'var[+varname+]', p0'varname'
  it 'parses g:varname', ->
    eq 'var[+g:varname+]', p0'g:varname'
  it 'parses abc:func', ->
    eq 'var[+abc:func+]', p0'abc:func'
  it 'parses s:v', ->
    eq 'var[+s:v+]', p0's:v'
  it 'parses s:', ->
    eq 'var[+s:+]', p0's:'
  it 'parses <SID>v', ->
    eq 'var[+<SID>v+]', p0'<SID>v'
  it 'parses abc#def', ->
    eq 'var[+abc#def+]', p0'abc#def'
  it 'parses g:abc#def', ->
    eq 'var[+g:abc#def+]', p0'g:abc#def'
  it 'parses <SNR>12_v', ->
    eq 'var[+<SNR>12_v+]', p0'<SNR>12_v'
  it 'parses curly braces name: v{a}', ->
    eq 'cvar(id[+v+], curly[!{!](var[+a+]))', p0'v{a}'
  it 'parses curly braces name: {a}', ->
    eq 'cvar(curly[!{!](var[+a+]))', p0'{a}'
  it 'parses curly braces name: {a}b', ->
    eq 'cvar(curly[!{!](var[+a+]), id[+b+])', p0'{a}b'
  it 'parses curly braces name: x{a}b', ->
    eq 'cvar(id[+x+], curly[!{!](var[+a+]), id[+b+])', p0'x{a}b'
  it 'parses curly braces name: x{a}1', ->
    eq 'cvar(id[+x+], curly[!{!](var[+a+]), id[+1+])', p0'x{a}1'
  it 'parses abc.key', ->
    eq '.[+key+](var[+abc+])', p0'abc.key'
  it 'parses abc.key.2', ->
    eq '.[+2+](.[+key+](var[+abc+]))', p0'abc.key.2'
  it 'parses abc.g:v', ->
    eq '..(var[+abc+], var[+g:v+])', p0'abc.g:v'
  it 'parses abc.autoload#var', ->
    eq '..(var[+abc+], var[+autoload#var+])', p0'abc.autoload#var'
  it 'parses 1.2.3.4', ->
    eq '.[+4+](.[+3+](.[+2+](N[+1+])))', p0'1.2.3.4'
  it 'parses "abc".def', ->
    eq '..("[+"abc"+], var[+def+])', p0'"abc".def'
  it 'parses 1 . 2 . 3 . 4', ->
    eq '..(N[+1+], N[+2+], N[+3+], N[+4+])', p0'1 . 2 . 3 . 4'
  it 'parses 1. 2. 3. 4', ->
    eq '..(N[+1+], N[+2+], N[+3+], N[+4+])', p0'1. 2. 3. 4'
  it 'parses 1 .2 .3 .4', ->
    eq '..(N[+1+], N[+2+], N[+3+], N[+4+])', p0'1 .2 .3 .4'
  it 'parses a && b && c', ->
    eq '&&(var[+a+], var[+b+], var[+c+])', p0'a && b && c'
  it 'parses a || b || c', ->
    eq '||(var[+a+], var[+b+], var[+c+])', p0'a || b || c'
  it 'parses a || b && c || d', ->
    eq '||(var[+a+], &&(var[+b+], var[+c+]), var[+d+])', p0'a || b && c || d'
  it 'parses a && b || c && d', ->
    eq '||(&&(var[+a+], var[+b+]), &&(var[+c+], var[+d+]))',
      p0'a && b || c && d'
  it 'parses a && (b || c) && d', ->
    eq '&&(var[+a+], expr[!(!](||(var[+b+], var[+c+])), var[+d+])',
      p0'a && (b || c) && d'
  it 'parses a + b + c*d/e/f  - g % h .i', ->
    str   = '..(-(+(var[+a+], '
    str ..=     'var[+b+], '
    str ..=    '/(*(var[+c+], '
    str ..=        'var[+d+]), '
    str ..=      'var[+e+], '
    str ..=      'var[+f+])), '
    str ..=    '%(var[+g+], var[+h+])), '
    str ..=    'var[+i+])'
    eq str, p0'a + b + c*d/e/f  - g % h .i'
  it 'parses !+-!!++a', ->
    eq '!(+!(-!(!(!(+!(+!(var[+a+])))))))', p0'!+-!!++a'
  it 'parses (abc)', ->
    eq 'expr[!(!](var[+abc+])', p0'(abc)'
  it 'parses [1, 2 , 3 ,4]', ->
    eq '[](N[+1+], N[+2+], N[+3+], N[+4+])', p0'[1, 2 , 3 ,4]'
  it 'parses {1:2, v : c, (10): abc}', ->
    str   = '{}(N[+1+], N[+2+], '
    str ..=    'var[+v+], var[+c+], '
    str ..=    'expr[!(!](N[+10+]), var[+abc+])'
    eq str, p0'{1:2, v : c, (10): abc}'
  it 'parses 1 == 2 && 3 != 4 && 5 > 6 && 7 < 8', ->
    str   = '&&(==(N[+1+], N[+2+]), !=(N[+3+], N[+4+]), >(N[+5+], N[+6+]), '
    str ..=    '<(N[+7+], N[+8+]))'
    eq str, p0'1 == 2 && 3 != 4 && 5 > 6 && 7 < 8'
  it 'parses "" ># "a" || "" <? "b" || "" is "c"', ->
    str   = '||(>#("[+""+], "[+"a"+]), <?("[+""+], "[+"b"+]), '
    str ..=    'is("[+""+], "[+"c"+]))'
    eq str, p0'"" ># "a" || "" <? "b" || "" is "c"'
  it 'parses 1== 2 &&  1 ==#2 && 1==?2', ->
    eq ('&&(==(N[+1+], N[+2+]), ==#(N[+1+], N[+2+]), ==?(N[+1+], N[+2+]))'),
      p0'1== 2 &&  1 ==#2 && 1==?2'
  it 'parses 1!= 2 &&  1 !=#2 && 1!=?2', ->
    eq ('&&(!=(N[+1+], N[+2+]), !=#(N[+1+], N[+2+]), !=?(N[+1+], N[+2+]))'),
      p0'1!= 2 &&  1 !=#2 && 1!=?2'
  it 'parses 1> 2 &&  1 >#2 && 1>?2', ->
    eq ('&&(>(N[+1+], N[+2+]), >#(N[+1+], N[+2+]), >?(N[+1+], N[+2+]))'),
      p0'1> 2 &&  1 >#2 && 1>?2'
  it 'parses 1< 2 &&  1 <#2 && 1<?2', ->
    eq ('&&(<(N[+1+], N[+2+]), <#(N[+1+], N[+2+]), <?(N[+1+], N[+2+]))'),
      p0'1< 2 &&  1 <#2 && 1<?2'
  it 'parses 1<= 2 &&  1 <=#2 && 1<=?2', ->
    eq ('&&(<=(N[+1+], N[+2+]), <=#(N[+1+], N[+2+]), <=?(N[+1+], N[+2+]))'),
      p0'1<= 2 &&  1 <=#2 && 1<=?2'
  it 'parses 1>= 2 &&  1 >=#2 && 1>=?2', ->
    eq ('&&(>=(N[+1+], N[+2+]), >=#(N[+1+], N[+2+]), >=?(N[+1+], N[+2+]))'),
      p0'1>= 2 &&  1 >=#2 && 1>=?2'
  it 'parses 1is 2 &&  1 is#2 && 1is?2', ->
    eq ('&&(is(N[+1+], N[+2+]), is#(N[+1+], N[+2+]), is?(N[+1+], N[+2+]))'),
      p0'1is 2 &&  1 is#2 && 1is?2'
  it 'parses 1isnot 2 &&  1 isnot#2 && 1isnot?2', ->
    str   = '&&(isnot(N[+1+], N[+2+]), isnot#(N[+1+], N[+2+]), '
    str ..=    'isnot?(N[+1+], N[+2+]))'
    eq str, p0'1isnot 2 &&  1 isnot#2 && 1isnot?2'
  it 'parses 1=~ 2 &&  1 =~#2 && 1=~?2', ->
    eq ('&&(=~(N[+1+], N[+2+]), =~#(N[+1+], N[+2+]), =~?(N[+1+], N[+2+]))'),
      p0'1=~ 2 &&  1 =~#2 && 1=~?2'
  it 'parses 1!~ 2 &&  1 !~#2 && 1!~?2', ->
    eq ('&&(!~(N[+1+], N[+2+]), !~#(N[+1+], N[+2+]), !~?(N[+1+], N[+2+]))'),
      p0'1!~ 2 &&  1 !~#2 && 1!~?2'
  it 'parses call(1, 2, 3, 4, 5)', ->
    eq ('call(var[+call+], N[+1+], N[+2+], N[+3+], N[+4+], N[+5+])'),
      p0'call(1, 2, 3, 4, 5)'
  it 'parses (1)(2)', ->
    eq 'call(expr[!(!](N[+1+]), N[+2+])', p0'(1)(2)'
  -- TODO see #251
  -- it('parses call (def)', function()
    -- eq 'call(var[+call+], var[+def+])', p0'call (def)'
  it 'parses [][1]', ->
    eq 'index([], N[+1+])', p0'[][1]'
  it 'parses [][1:]', ->
    eq 'index([], N[+1+], empty[!]!])', p0'[][1:]'
  it 'parses [][:1]', ->
    eq 'index([], empty[!:!], N[+1+])', p0'[][:1]'
  it 'parses [][:]', ->
    eq 'index([], empty[!:!], empty[!]!])', p0'[][:]'
  it 'parses [][i1 : i2]', ->
    eq 'index([], var[+i1+], var[+i2+])', p0'[][i1 : i2]'

  -- SEGV!!!
  -- it('fails to parse <', function()
    -- eq 'error', p0'<'

-- vim: sw=2 sts=2 et tw=80
