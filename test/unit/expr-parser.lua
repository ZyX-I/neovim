local helpers = require 'test.unit.helpers'
local cimport, internalize, eq, ffi = helpers.cimport, helpers.internalize, helpers.eq, helpers.ffi

local expr = cimport('./src/expr.h')
local cstr = ffi.typeof('char[?]')

local expression_type = {
  [0] = 'Unknown',
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
  'emptyindex',
}

local case_compare_strategy = {
  [0] = '',
  '#',
  '?',
}

node_to_string = function(node, err)
  local type = expression_type[tonumber(node.type)]
  local case_suffix = case_compare_strategy[tonumber(node.ignore_case)]
  local func = type .. case_suffix
  local result = func
  if (err and err.message ~= nil) then
    result = 'error(' .. ffi.string(err.message) .. ')'
    return result
  end
  if (node.position ~= nil) then
    local str = ffi.string(node.position)
    if (node.end_position ~= nil) then
      local len = node.end_position - node.position + 1
      result = result .. '[+' .. str:sub(1, len) .. '+]'
    else
      result = result .. '[!' .. str:sub(1, 1) .. '!]'
    end
  end
  if (node.children ~= nil) then
    result = (result .. '(' .. node_to_string(node.children, err) .. ')')
  end
  if (node.next ~= nil) then
    result = result .. ', ' .. node_to_string(node.next, err)
  end
  return result
end

local size = ffi.sizeof('ExpressionParserError')
local size2 = ffi.sizeof('ExpressionParserError[1]')
ffi.cdef('void *malloc(size_t size)')
local _e = ffi.C.malloc(size)
_err = ffi.cast('ExpressionParserError*', _e)

describe('parse0', function()
  local parse0 = function(str)
    local s = cstr(string.len(str), str)
    return ffi.gc(expr.parse0_err(s, _err), expr.free_node), _err
    -- return ffi.gc(expr.parse0(s), expr.free_node), nil
  end
  local p0 = function(str)
    local parsed, err = parse0(str)
    return node_to_string(parsed, err)
  end
  it('parses number 0', function()
    eq('N[+0+]', p0('0'))
  end)
  it('parses number 10', function()
    eq('N[+10+]', p0('10'))
  end)
  it('parses number 110', function()
    eq('N[+110+]', p0('110'))
  end)
  it('parses float 0.0', function()
    eq('F[+0.0+]', p0('0.0'))
  end)
  it('parses float 0.0e0', function()
    eq('F[+0.0e0+]', p0('0.0e0'))
  end)
  it('parses float 0.1e+1', function()
    eq('F[+0.1e+1+]', p0('0.1e+1'))
  end)
  it('parses float 0.1e-1', function()
    eq('F[+0.1e-1+]', p0('0.1e-1'))
  end)
  it('parses "abc"', function()
    eq('"[+"abc"+]', p0('"abc"'))
  end)
  it('parses "a\\"bc"', function()
    eq('"[+"a\\"bc"+]', p0('"a\\"bc"'))
  end)
  it('parses \'abc\'', function()
    eq('\'[+\'abc\'+]', p0('\'abc\''))
  end)
  it('parses \'a\'\'bc\'', function()
    eq('\'[+\'ab\'\'c\'+]', p0('\'ab\'\'c\''))
  end)
  it('parses option', function()
    eq('&[+abc+]', p0('&abc'))
  end)
  it('parses local option', function()
    eq('&[+l:abc+]', p0('&l:abc'))
  end)
  it('parses global option', function()
    eq('&[+g:abc+]', p0('&g:abc'))
  end)
  it('parses register r', function()
    eq('@[+@r+]', p0('@r'))
  end)
  it('parses register NUL', function()
    eq('@[+@+]', p0('@'))
  end)
  -- TODO Requires chartab to be initialized
  -- it('parses environment variable', function()
    -- eq('$[+abc+]', p0('$abc'))
  -- end)
  it('parses varname', function()
    eq('var[+varname+]', p0('varname'))
  end)
  it('parses g:varname', function()
    eq('var[+g:varname+]', p0('g:varname'))
  end)
  it('parses abc:func', function()
    eq('var[+abc:func+]', p0('abc:func'))
  end)
  it('parses s:v', function()
    eq('var[+s:v+]', p0('s:v'))
  end)
  it('parses s:', function()
    eq('var[+s:+]', p0('s:'))
  end)
  it('parses <SID>v', function()
    eq('var[+<SID>v+]', p0('<SID>v'))
  end)
  it('parses abc#def', function()
    eq('var[+abc#def+]', p0('abc#def'))
  end)
  it('parses g:abc#def', function()
    eq('var[+g:abc#def+]', p0('g:abc#def'))
  end)
  it('parses <SNR>12_v', function()
    eq('var[+<SNR>12_v+]', p0('<SNR>12_v'))
  end)
  it('parses curly braces name: v{a}', function()
    eq('cvar(id[+v+], curly[!{!](var[+a+]))', p0('v{a}'))
  end)
  it('parses curly braces name: {a}', function()
    eq('cvar(curly[!{!](var[+a+]))', p0('{a}'))
  end)
  it('parses curly braces name: {a}b', function()
    eq('cvar(curly[!{!](var[+a+]), id[+b+])', p0('{a}b'))
  end)
  it('parses curly braces name: x{a}b', function()
    eq('cvar(id[+x+], curly[!{!](var[+a+]), id[+b+])', p0('x{a}b'))
  end)
  it('parses curly braces name: x{a}1', function()
    eq('cvar(id[+x+], curly[!{!](var[+a+]), id[+1+])', p0('x{a}1'))
  end)
  it('parses abc.key', function()
    eq('.[+key+](var[+abc+])', p0('abc.key'))
  end)
  it('parses abc.key.2', function()
    eq('.[+2+](.[+key+](var[+abc+]))', p0('abc.key.2'))
  end)
  it('parses abc.g:v', function()
    eq('..(var[+abc+], var[+g:v+])', p0('abc.g:v'))
  end)
  it('parses 1.2.3.4', function()
    eq('.[+4+](.[+3+](.[+2+](N[+1+])))', p0('1.2.3.4'))
  end)
  it('parses "abc".def', function()
    eq('..("[+"abc"+], var[+def+])', p0('"abc".def'))
  end)
  it('parses 1 . 2 . 3 . 4', function()
    eq('..(N[+1+], N[+2+], N[+3+], N[+4+])', p0('1 . 2 . 3 . 4'))
  end)
  it('parses 1. 2. 3. 4', function()
    eq('..(N[+1+], N[+2+], N[+3+], N[+4+])', p0('1. 2. 3. 4'))
  end)
  it('parses 1 .2 .3 .4', function()
    eq('..(N[+1+], N[+2+], N[+3+], N[+4+])', p0('1 .2 .3 .4'))
  end)
  it('parses a && b && c', function()
    eq('&&(var[+a+], var[+b+], var[+c+])', p0('a && b && c'))
  end)
  it('parses a || b || c', function()
    eq('||(var[+a+], var[+b+], var[+c+])', p0('a || b || c'))
  end)
  it('parses a || b && c || d', function()
    eq('||(var[+a+], &&(var[+b+], var[+c+]), var[+d+])', p0('a || b && c || d'))
  end)
  it('parses a && b || c && d', function()
    eq('||(&&(var[+a+], var[+b+]), &&(var[+c+], var[+d+]))',
       p0('a && b || c && d'))
  end)
  it('parses a && (b || c) && d', function()
    eq('&&(var[+a+], expr[!(!](||(var[+b+], var[+c+])), var[+d+])',
       p0('a && (b || c) && d'))
  end)
  it('parses a + b + c*d/e/f  - g % h .i', function()
    eq('..(-(+(var[+a+], ' ..
              'var[+b+], ' ..
              '/(*(var[+c+], ' ..
                  'var[+d+]), ' ..
                'var[+e+], ' ..
                'var[+f+])), ' ..
            '%(var[+g+], var[+h+])), ' ..
          'var[+i+])',
       p0('a + b + c*d/e/f  - g % h .i'))
  end)
  it('parses !+-!!++a', function()
    eq('!(+!(-!(!(!(+!(+!(var[+a+])))))))', p0('!+-!!++a'))
  end)
  it('parses (abc)', function()
    eq('expr[!(!](var[+abc+])', p0('(abc)'))
  end)
  it('parses [1, 2 , 3 ,4]', function()
    eq('[](N[+1+], N[+2+], N[+3+], N[+4+])', p0('[1, 2 , 3 ,4]'))
  end)
  it('parses {1:2, v : c, (10): abc}', function()
    eq('{}(N[+1+], N[+2+], ' ..
          'var[+v+], var[+c+], ' ..
          'expr[!(!](N[+10+]), var[+abc+])',
       p0('{1:2, v : c, (10): abc}'))
  end)

  -- SEGV!!!
  -- it('fails to parse <', function()
    -- eq('error', p0('<'))
  -- end)
end)

-- vim: sw=2 sts=2 tw=80
