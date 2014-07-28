local t, itn
do
  local _obj_0 = require('test.unit.viml.parser.helpers')
  t, itn = _obj_0.t, _obj_0.itn
end
describe('parse_one_cmd errors', function()
  describe('NOFUNC commands errors', function()
    local _list_0 = {
      'intro',
      '<',
      'al',
      'as',
      'bN',
      'ba',
      'bf',
      'bl',
      'bm',
      'bp',
      'br',
      'brea',
      'buffers',
      'cN',
      'X',
      'xa',
      'wa',
      'viu',
      'unh',
      'undol',
      'undoj',
      'u',
      'try',
      'tr',
      'tp',
      'tn',
      'tl',
      'tf',
      'tabs',
      'tabr',
      'tabN',
      'tabp',
      'tabo',
      'tabn',
      'tabl',
      'tabfir',
      'tabc',
      'tags',
      'tN',
      'syncbind',
      'sw',
      'sus',
      'sun',
      'stopi',
      'startr',
      'startg',
      'star',
      'st',
      'spellr',
      'spelli',
      'spelld',
      'sh',
      'scrip',
      'sbr',
      'sbp',
      'sbn',
      'sbm',
      'sbl',
      'sbf',
      'sba',
      'sbN',
      'sal',
      'redraws',
      'redr',
      'red',
      'pw',
      'ptr',
      'ptn',
      'ptl',
      'ptf',
      'ptN',
      'pre',
      'pp',
      'po',
      'pc',
      'opt',
      'on',
      'ol',
      'noh',
      'nbc',
      'mes',
      'ls',
      'lw',
      'cw',
      'lr',
      'cr',
      'lpf',
      'cpf',
      'lNf',
      'cNf',
      'lp',
      'cp',
      'lN',
      'cN',
      'lop',
      'cope',
      'lol',
      'col',
      'lnew',
      'cnew',
      'lnf',
      'cnf',
      'lne',
      'cn',
      'lla',
      'cla',
      'll',
      'cc',
      'lfir',
      'cfir',
      'lcl',
      'ccl',
      'ju',
      'go',
      'foldo',
      'foldc',
      'fix',
      'fini',
      'fina',
      'files',
      'exu',
      'end',
      'endf',
      'endfo',
      'endt',
      'endw',
      'el',
      'difft',
      'diffo',
      'diffu',
      'debugg',
      'cq',
      'con',
      'comc',
      'clo',
      'che',
      'changes'
    }
    for _index_0 = 1, #_list_0 do
      local cmd = _list_0[_index_0]
      itn('\\ error: E488: Trailing characters: ' .. cmd .. ' !!a!!bc', cmd .. ' abc')
    end
  end)
  describe('commands with required argument', function()
    local _list_0 = {
      'norm',
      'luado',
      'perldo',
      'pydo',
      'py3do',
      'tcldo'
    }
    for _index_0 = 1, #_list_0 do
      local cmd = _list_0[_index_0]
      itn('\\ error: E471: Argument required: ' .. cmd .. '!!!', cmd)
    end
    local _list_1 = {
      'setf',
      'nbk'
    }
    for _index_0 = 1, #_list_1 do
      local cmd = _list_1[_index_0]
      itn('\\ error: E471: Argument required: !!!', cmd)
    end
  end)
  describe('invalid ++ff values', function()
    itn('\\ error: E474: Invalid ++ff argument: e++ff=!!u!!ix', 'e++ff=uix')
    itn('\\ error: E474: Invalid ++ff argument: e++ff=!!u!!ixtttt', 'e++ff=uixtttt')
    itn('\\ error: E474: Invalid ++ff argument: e++ff=!!d!!as', 'e++ff=das')
    itn('\\ error: E474: Invalid ++ff argument: e++ff=!!m!!as', 'e++ff=mas')
    return itn('\\ error: E474: Invalid ++ff argument: e++ff=!!t!!ty', 'e++ff=tty')
  end)
  describe(':call with non-call arguments', function()
    itn('\\ error: E129: :call accepts only function calls: call !!A!!bc', 'call Abc')
  end)
end)
return describe('parse_cmd_sequence errors', function()
  return describe('missing block ends', function()
    t('\n    if 1\n    \\ error: E171: Missing :endif:     endfor!!!\n    \\ error: E588: :endfor without :for:     endfor!!!\n    ', '\n    if 1\n    endfor')
    t('\n    if 1\n    endif\n    \\ error: E580: :endif without :if:     endif!!!\n    ', '\n    if 1\n    endif\n    endif')
    t('\n    if 1\n    endif\n    \\ error: E580: :endif without :if:     endif !!|!! if 1 | endif\n    if 1\n    endif\n    ', '\n    if 1\n    endif\n    endif | if 1 | endif')
    t('\n    \\ error: E606: :finally without :try:     finally!!!\n    \\ error: E602: :endtry without :try:     endtry!!!\n    ', '\n    finally\n    endtry\n    ')
    return t('\n    if 1\n      if 1\n        echo 1\n      else\n        echo 2\n      \\ error: E583: multiple :else:       else!!!\n        echo 3\n      \\ error: E583: multiple :else:       else!!!\n        echo 4\n      endif\n    endif\n    echo 6\n    for x in x\n      echo 7\n    \\ error: E733: Using :endwhile with :for:     endwhile!!!\n    echo 8\n    ', '\n    if 1\n      if 1\n        echo 1\n      else\n        echo 2\n      else\n        echo 3\n      else\n        echo 4\n      endif\n    endif\n    echo 6\n    for x in x\n      echo 7\n    endwhile\n    echo 8')
  end)
end)
