{:t, :itn} = require 'test.unit.translator.parser.helpers'

describe 'parse_one_cmd errors', ->
  describe 'NOFUNC commands errors', ->
    for cmd in *{
      'intro'
      '<'
      'al'
      'as'
      'bN'
      'ba'
      'bf'
      'bl'
      'bm'
      'bp'
      'br'
      'brea'
      'buffers'
      'cN'
      'X'
      'xa'
      'wa'
      'viu'
      'unh'
      'undol'
      'undoj'
      'u'
      'try'
      'tr'
      'tp'
      'tn'
      'tl'
      'tf'
      'tabs'
      'tabr'
      'tabN'
      'tabp'
      'tabo'
      'tabn'
      'tabl'
      'tabfir'
      'tabc'
      'tags'
      'tN'
      'syncbind'
      'sw'
      'sus'
      'sun'
      'stopi'
      'startr'
      'startg'
      'star'
      'st'
      'spellr'
      'spelli'
      'spelld'
      'sh'
      'scrip'
      'sbr'
      'sbp'
      'sbn'
      'sbm'
      'sbl'
      'sbf'
      'sba'
      'sbN'
      'sal'
      'redraws'
      'redr'
      'red'
      'pw'
      'ptr'
      'ptn'
      'ptl'
      'ptf'
      'ptN'
      'pre'
      'pp'
      'po'
      'pc'
      'opt'
      'on'
      'ol'
      'noh'
      'nbc'
      'mes'
      'ls'
      'lw'
      'cw'
      'lr'
      'cr'
      'lpf'
      'cpf'
      'lNf'
      'cNf'
      'lp'
      'cp'
      'lN'
      'cN'
      'lop'
      'cope'
      'lol'
      'col'
      'lnew'
      'cnew'
      'lnf'
      'cnf'
      'lne'
      'cn'
      'lla'
      'cla'
      'll'
      'cc'
      'lfir'
      'cfir'
      'lcl'
      'ccl'
      'ju'
      'go'
      'foldo'
      'foldc'
      'fix'
      'fini'
      'fina'
      'files'
      'exu'
      'end'
      'endf'
      'endfo'
      'endt'
      'endw'
      'el'
      'difft'
      'diffo'
      'diffu'
      'debugg'
      'cq'
      'con'
      'comc'
      'clo'
      'che'
      'changes'
    }
      itn '\\ error: E488: Trailing characters: ' .. cmd .. ' !!a!!bc',
        cmd .. ' abc'

describe 'parse_cmd_sequence errors', ->
  describe 'missing block ends', ->
    t '
    if 1
    \\ error: E171: Missing :endif:     endfor!!!
    \\ error: E588: :endfor without :for:     endfor!!!
    ', '
    if 1
    endfor'

    t '
    if 1
    endif
    \\ error: E580: :endif without :if:     endif!!!
    ', '
    if 1
    endif
    endif'

    t '
    if 1
    endif
    \\ error: E580: :endif without :if:     endif !!|!! if 1 | endif
    if 1
    endif
    ', '
    if 1
    endif
    endif | if 1 | endif'

    t '
    \\ error: E606: :finally without :try:     finally!!!
    \\ error: E602: :endtry without :try:     endtry!!!
    ', '
    finally
    endtry
    '
