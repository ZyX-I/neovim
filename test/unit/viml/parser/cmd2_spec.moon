{:t, :itn} = require 'test.unit.viml.parser.helpers'

describe 'parse_one_cmd', ->
  opts = {
    ['++encoding=UTF-8']: '++enc=utf-8'
    ['++enc=UtF-8']: '++enc=utf-8'
    ['++bin']: '++bin'
    ['++binary']: '++bin'
    ['++nobin']: '++nobin'
    ['++nobinary']: '++nobin'
    ['++bad=kEeP']: '++bad=keep'
    ['++bad=DROP']: '++bad=drop'
    ['++bad=!']: '++bad=!'
    ['++ff=unix']: '++ff=unix'
    ['++ff=dos']: '++ff=dos'
    ['++ff=mac']: '++ff=mac'
    ['++fileformat=unix']: '++ff=unix'
    ['++fileformat=dos']: '++ff=dos'
    ['++fileformat=mac']: '++ff=mac'
    ['++edit']: '++edit'
    ['++binary ++ff=dos ++edit']: '++bin ++edit ++ff=dos'
  }

  describe '! ++opt +cmd commands', ->
    for trunc, full in pairs {
      fir: 'first'
      la: 'last'
      rew: 'rewind'
      sfir: 'sfirst'
      sre: 'srewind'
      sla: 'slast'
    }
      itn full, trunc
      itn full .. '!', trunc .. '!'
      itn full .. ' +let\\ a\\ =\\ 1', trunc .. '+let\\ a=1'
      for used, expected in pairs opts
        itn full .. ' ' .. expected, trunc .. used
        itn full .. '! ' .. expected, trunc .. '!' .. used

  describe '! count ++opt +cmd commands', ->
    for trunc, full in pairs {
      N: 'Next'
      prev: 'previous'
      sN: 'sNext'
      sa: 'sargument'
      spr: 'sprevious'
    }
      itn full, trunc
      itn full .. '!', trunc .. '!'
      itn full .. ' +let\\ a\\ =\\ 1', trunc .. '+let\\ a=1'
      itn full .. ' ++bin 5', trunc .. '++bin5'
      itn '5' .. full .. ' ++bin', '5' .. trunc .. '++bin'
      for used, expected in pairs opts
        itn full .. ' ' .. expected, trunc .. used
        itn full .. '! ' .. expected, trunc .. '!' .. used

  describe '! ++opt commands', ->
    itn 'wqall', 'wqa'
    itn 'wqall!', 'wqa!'

describe 'parse_cmd_sequence', ->
  describe 'if block', ->
    t '
    if abc
    elseif def
    else
    endif
    ', '
    if abc
    elseif def
    else
    endif'

    t '
    if abc
    elseif def
    else
    endif', 'if abc|elseif def|else|endif'

    t '
    if abc
      join
    endif', 'if abc|join|endif'

    t '
    if abc
      5print
    endif', 'if abc|5|endif'

    t '
    silent if 1
    endif
    ', '
    silent if 1
    endif'

  describe 'while block', ->
    t '
    while 1
    endwhile', 'while 1|endwhile'

  describe 'for block', ->
    t '
    for x in [1]
    endfor
    ', '
    for x in [1]
    endfo'

    t '
    for [x, y] in [[1, 2]]
    endfor
    ', '
    for [x,y] in [[1,2]]
    endfo'

  describe 'function', ->
    t '
    function Abc()
    endfunction
    ', '
    function Abc()
    endfunction'

    t '
    function Abc() range dict abort
    endfunction
    ', '
    function Abc()dictabortrange
    endfunction'

    t '
    function Abc(...)
    endfunction
    ', '
    function Abc(...)
    endfunction'

    t '
    function Abc(a, b, c, ...)
    endfunction
    ', '
    fu Abc(a,b,c,...)
    endfu'

    t 'function Abc', 'fu Abc'

    t '
    function! Abc
    echo "def"
    ', '
    fu!Abc|echo"def"'

    t 'function', 'fu'

    t '
    function s:F()
    endfunction'

    t '
    function foo#bar()
    endfunction'

    t '
    function g:bar()
    endfunction'

    t '
    function foo:bar()
    endfunction'

    t '
    function Abc
    echo 1
    '

  describe 'comments, empty lines', ->
    t '
    unlet a b
    " echo 1+1
    ', '
    unlet a b
    " echo 1+1'

    t '
    " !
    unlet a b
    ', '
    " !
    \t
    \t
    \t
    \t
    \t
    unlet a b'

    t '
    unlet a b
    ', '
    \t
    \t
    \t
    unlet a b'

  describe 'expression commands', ->
    t 'echo 1\necho 2', 'echo1|echo2'
    t '
    if 1
      return 1
    endif
    ', 'if 1 | return 1 | endif'
