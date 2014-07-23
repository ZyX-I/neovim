{:t, :itn} = require 'test.unit.viml.parser.helpers'

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

  describe 'commands with required argument', ->
    for cmd in *{
      'norm'
      'luado'
      'perldo'
      'pydo'
      'py3do'
      'tcldo'
    }
      itn '\\ error: E471: Argument required: ' .. cmd .. '!!!', cmd

    for cmd in *{
      'setf'
      'nbk'
    }
      itn '\\ error: E471: Argument required: !!!', cmd

  describe 'invalid ++ff values', ->
    itn '\\ error: E474: Invalid ++ff argument: e++ff=!!u!!ix', 'e++ff=uix'
    itn '\\ error: E474: Invalid ++ff argument: e++ff=!!u!!ixtttt', 'e++ff=uixtttt'
    itn '\\ error: E474: Invalid ++ff argument: e++ff=!!d!!as', 'e++ff=das'
    itn '\\ error: E474: Invalid ++ff argument: e++ff=!!m!!as', 'e++ff=mas'
    itn '\\ error: E474: Invalid ++ff argument: e++ff=!!t!!ty', 'e++ff=tty'

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

    t '
    if 1
      if 1
        echo 1
      else
        echo 2
      \\ error: E583: multiple :else:       else!!!
        echo 3
      \\ error: E583: multiple :else:       else!!!
        echo 4
      endif
    endif
    echo 6
    for x in x
      echo 7
    \\ error: E733: Using :endwhile with :for:     endwhile!!!
    echo 8
    ', '
    if 1
      if 1
        echo 1
      else
        echo 2
      else
        echo 3
      else
        echo 4
      endif
    endif
    echo 6
    for x in x
      echo 7
    endwhile
    echo 8'
