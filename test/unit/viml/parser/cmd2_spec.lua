local t, itn
do
  local _obj_0 = require('test.unit.viml.parser.helpers')
  t, itn = _obj_0.t, _obj_0.itn
end
describe('parse_one_cmd', function()
  local opts = {
    ['++encoding=UTF-8'] = '++enc=utf-8',
    ['++enc=UtF-8'] = '++enc=utf-8',
    ['++bin'] = '++bin',
    ['++binary'] = '++bin',
    ['++nobin'] = '++nobin',
    ['++nobinary'] = '++nobin',
    ['++bad=kEeP'] = '++bad=keep',
    ['++bad=DROP'] = '++bad=drop',
    ['++bad=!'] = '++bad=!',
    ['++ff=unix'] = '++ff=unix',
    ['++ff=dos'] = '++ff=dos',
    ['++ff=mac'] = '++ff=mac',
    ['++fileformat=unix'] = '++ff=unix',
    ['++fileformat=dos'] = '++ff=dos',
    ['++fileformat=mac'] = '++ff=mac',
    ['++edit'] = '++edit',
    ['++binary ++ff=dos ++edit'] = '++bin ++edit ++ff=dos'
  }
  describe('! ++opt +cmd commands', function()
    for trunc, full in pairs({
      fir = 'first',
      la = 'last',
      rew = 'rewind',
      sfir = 'sfirst',
      sre = 'srewind',
      sla = 'slast'
    }) do
      itn(full, trunc)
      itn(full .. '!', trunc .. '!')
      itn(full .. ' +let\\ a\\ =\\ 1', trunc .. '+let\\ a=1')
      for used, expected in pairs(opts) do
        itn(full .. ' ' .. expected, trunc .. used)
        itn(full .. '! ' .. expected, trunc .. '!' .. used)
      end
    end
  end)
  describe('! count ++opt +cmd commands', function()
    for trunc, full in pairs({
      N = 'Next',
      prev = 'previous',
      sN = 'sNext',
      sa = 'sargument',
      spr = 'sprevious'
    }) do
      itn(full, trunc)
      itn(full .. '!', trunc .. '!')
      itn(full .. ' +let\\ a\\ =\\ 1', trunc .. '+let\\ a=1')
      itn(full .. ' ++bin 5', trunc .. '++bin5')
      itn('5' .. full .. ' ++bin', '5' .. trunc .. '++bin')
      for used, expected in pairs(opts) do
        itn(full .. ' ' .. expected, trunc .. used)
        itn(full .. '! ' .. expected, trunc .. '!' .. used)
      end
    end
  end)
  return describe('! ++opt commands', function()
    itn('wqall', 'wqa')
    return itn('wqall!', 'wqa!')
  end)
end)
return describe('parse_cmd_sequence', function()
  describe('if block', function()
    t('\n    if abc\n    elseif def\n    else\n    endif\n    ', '\n    if abc\n    elseif def\n    else\n    endif')
    t('\n    if abc\n    elseif def\n    else\n    endif', 'if abc|elseif def|else|endif')
    t('\n    if abc\n      join\n    endif', 'if abc|join|endif')
    t('\n    if abc\n      5print\n    endif', 'if abc|5|endif')
    return t('\n    silent if 1\n    endif\n    ', '\n    silent if 1\n    endif')
  end)
  describe('while block', function()
    return t('\n    while 1\n    endwhile', 'while 1|endwhile')
  end)
  describe('for block', function()
    t('\n    for x in [1]\n    endfor\n    ', '\n    for x in [1]\n    endfo')
    return t('\n    for [x, y] in [[1, 2]]\n    endfor\n    ', '\n    for [x,y] in [[1,2]]\n    endfo')
  end)
  describe('function', function()
    t('\n    function Abc()\n    endfunction\n    ', '\n    function Abc()\n    endfunction')
    t('\n    function Abc() range dict abort\n    endfunction\n    ', '\n    function Abc()dictabortrange\n    endfunction')
    t('\n    function Abc(...)\n    endfunction\n    ', '\n    function Abc(...)\n    endfunction')
    t('\n    function Abc(a, b, c, ...)\n    endfunction\n    ', '\n    fu Abc(a,b,c,...)\n    endfu')
    t('function Abc', 'fu Abc')
    t('\n    function! Abc\n    echo "def"\n    ', '\n    fu!Abc|echo"def"')
    t('function', 'fu')
    t('\n    function s:F()\n    endfunction')
    t('\n    function foo#bar()\n    endfunction')
    t('\n    function g:bar()\n    endfunction')
    t('\n    function foo:bar()\n    endfunction')
    return t('\n    function Abc\n    echo 1\n    ')
  end)
  describe('comments, empty lines', function()
    t('\n    unlet a b\n    " echo 1+1\n    ', '\n    unlet a b\n    " echo 1+1')
    t('\n    " !\n    unlet a b\n    ', '\n    " !\n    \t\n    \t\n    \t\n    \t\n    \t\n    unlet a b')
    return t('\n    unlet a b\n    ', '\n    \t\n    \t\n    \t\n    unlet a b')
  end)
  return describe('expression commands', function()
    t('echo 1\necho 2', 'echo1|echo2')
    return t('\n    if 1\n      return 1\n    endif\n    ', 'if 1 | return 1 | endif')
  end)
end)
