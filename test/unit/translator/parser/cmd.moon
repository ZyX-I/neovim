{:cimport, :internalize, :eq, :ffi, :lib, :cstr, :to_cstr} = require 'test.unit.helpers'

cmd = cimport('./src/nvim/translator/parser/ex_commands.h')

p1ct = (str, one, flags=0) ->
  s = to_cstr(str)
  result = cmd.parse_cmd_test(s, flags, one)
  if result == nil
    error('parse_cmd_test returned nil')
  return ffi.string(result)

eqn = (expected_result, cmd, one, flags=0) ->
  result = p1ct cmd, one, flags
  eq expected_result, result

itn = (expected_result, cmd, flags=0) ->
  it 'parses ' .. cmd, ->
    eqn expected_result, cmd, true, flags

t = (expected_result, cmd, flags=0) ->
  it 'parses ' .. cmd, ->
    expected = expected_result
    expected = expected\gsub('^%s*\n', '')
    first_indent = expected\match('^%s*')
    expected = expected\gsub('^' .. first_indent, '')
    expected = expected\gsub('\n' .. first_indent, '\n')
    expected = expected\gsub('\n%s*$', '')
    eqn expected, cmd, false, flags

describe 'parse_one_cmd', ->
  describe 'no commands', ->
    itn '1,2print', '1,2|'
    itn '1', '1'
    itn '1,2', '1,2'

  describe 'modifier commands', ->
    itn 'belowright 9join', 'bel 9join'
    itn 'aboveleft join', 'aboveleft join'
    itn 'aboveleft join', 'abo join'
    itn 'belowright join', 'bel join'
    itn 'botright join', 'bo join'
    itn 'browse join', 'bro join'
    itn 'confirm join', 'conf join'
    itn 'hide join', 'hid join'
    itn 'keepalt join', 'keepa join'
    itn 'keeppatterns join', 'keepp join'
    itn 'keepjumps join', 'keepj join'
    itn 'keepmarks join', 'keepm join'
    itn 'leftabove join', 'lefta join'
    itn 'lockmarks join', 'loc   j'
    itn 'noautocmd join', 'noa   j'
    itn 'rightbelow join', 'rightb join'
    itn 'sandbox join', 'san   j'
    itn 'silent join', 'sil   j'
    itn 'silent! join', 'sil!   j'
    itn 'tab join', 'tab j'
    itn 'tab 5 join', '5tab j'
    itn 'topleft join', 'to j'
    itn 'unsilent join', 'uns j'
    itn 'verbose join', 'verb j'
    itn 'verbose 1 join', '1verb j'
    itn 'vertical join', 'vert j'

  describe 'ranges', ->
    itn '1,2join', ':1,2join'
    itn '1,2join!', ':::::::1,2join!'
    itn '1,2,3,4join!', ':::::::1,2,3,4join!'
    itn '1join!', ':1      join!'
    itn '1,$join', ':%join'
    itn '\'<,\'>join', ':*join'
    itn '1+1;7+1+2+3join', ':1+1;7+1+2+3join'
    itn '\\&join', '\\&j'
    itn '\\/join', '\\/j'
    itn '\\?join', '\\?j'
    itn '.-1-1-1join', '---join'

  describe 'count and exflags', ->
    itn 'join #', ':join #'
    itn 'join 5 #', ':join 5 #'
    itn 'join #', ':join#'
    itn 'join 5 #', ':join5#'
    itn 'join #', ':j#'
    itn 'number #', ':num#'
    itn 'print l', ':p l'
    itn '# #', ':##'
    itn '# l', ':#l'
    itn '= l', ':=l'
    itn '> l', ':>l'
    itn 'number 5 #', ':num5#'
    itn 'print 5 l', ':p 5l'
    itn '# 1 #', ':#1#'
    itn '> 3 l', ':>3l'
    itn '< 3', ':<3'

  describe 'NOFUNC commands', ->
    itn 'intro', 'intro'
    itn 'intro', ':intro'
    itn '<', ':<'
    itn 'all', ':all'
    itn '5all', ':5all'
    itn 'all 5', ':all5'
    itn 'all 5', ':al5'
    itn 'ascii', ':ascii'
    itn 'bNext', ':bN'
    itn 'bNext 5', ':bN5'
    itn 'ball', ':ba'
    itn 'bfirst', ':bf'
    itn 'blast', ':bl'
    itn 'bmodified!', ':bm!'
    itn 'bnext', ':bn'
    itn 'bprevious 5', ':bp5'
    itn 'brewind', ':br'
    itn 'break', ':brea'
    itn 'breaklist', ':breakl'
    itn 'buffers', ':buffers'
    itn 'cNext', ':cN'
    itn 'X', ':X'
    itn 'xall', ':xa'
    itn 'wall', ':wa'
    itn 'viusage', ':viu'
    itn 'version', ':ver'
    itn 'unhide', ':unh'
    itn 'undolist', ':undol'
    itn 'undojoin', ':undoj'
    itn 'undo 5', ':u5'
    itn 'try', ':try'
    itn 'trewind', ':tr'
    itn 'tprevious', ':tp'
    itn 'tnext', ':tn'
    itn 'tlast', ':tl'
    itn 'tfirst', ':tf'
    itn 'tabs', ':tabs'
    itn 'tabrewind', ':tabr'
    itn 'tabNext', ':tabN'
    itn '5tabNext', ':5tabN'
    itn '5tabprevious', ':5tabp'
    itn 'tabonly!', ':tabo!'
    itn '5tabnext', ':5tabn'
    itn 'tablast', ':tabl'
    itn 'tabfirst', ':tabfir'
    itn 'tabclose! 1', ':tabc!1'
    itn '1tabclose!', ':1tabc!'
    itn 'tags', ':tags'
    itn '5tNext!', ':5tN!'
    itn 'syncbind', ':syncbind'
    itn 'swapname', ':sw'
    itn 'suspend!', ':sus!'
    itn 'sunhide 5', ':sun5'
    itn 'stopinsert', ':stopi'
    itn 'startreplace!', ':startr!'
    itn 'startgreplace!', ':startg!'
    itn 'startinsert!', ':star!'
    itn 'stop!', ':st!'
    itn 'spellrepall', ':spellr'
    itn 'spellinfo', ':spelli'
    itn 'spelldump!', ':spelld!'
    itn 'shell', ':sh'
    itn 'scriptnames', ':scrip'
    itn 'sbrewind', ':sbr'
    itn 'sbprevious', ':sbp'
    itn 'sbnext', ':sbn'
    itn 'sbmodified 5', ':sbm 5'
    itn 'sblast', ':sbl'
    itn 'sbfirst', ':sbf'
    itn 'sball 5', ':sba5'
    itn '5sball', ':5sba'
    itn 'sbNext 5', ':sbN5'
    itn '5sbNext', ':5sbN'
    itn 'sall! 5', ':sal!5'
    itn '5sall!', ':5sal!'
    itn 'redrawstatus!', ':redraws!'
    itn 'redraw!', ':redr!'
    itn 'redo', ':red'
    itn 'pwd', ':pw'
    itn '5ptrewind!', ':5ptr!'
    itn '5ptprevious!', ':5ptp!'
    itn '5ptnext!', ':5ptn!'
    itn 'ptlast!', ':ptl!'
    itn '5ptfirst!', ':5ptf!'
    itn '5ptNext!', ':5ptN!'
    itn 'preserve', ':pre'
    itn '5ppop!', ':5pp!'
    itn '5pop!', ':5po!'
    itn 'pclose!', ':pc!'
    itn 'options', ':opt'
    itn 'only!', ':on!'
    itn 'oldfiles', ':ol'
    itn 'nohlsearch', ':noh'
    itn 'nbclose', ':nbc'
    itn 'messages', ':mes'
    itn 'ls!', ':ls!'
    itn 'lwindow 5', ':lw 5'
    itn '5lwindow', ':5lw'
    itn 'cwindow 5', ':cw 5'
    itn '5cwindow', ':5cw'
    itn 'lrewind 5', ':lr 5'
    itn '5lrewind', ':5lr'
    itn 'crewind 5', ':cr 5'
    itn '5crewind', ':5cr'
    itn 'lpfile 5', ':lpf 5'
    itn '5lpfile', ':5lpf'
    itn 'cpfile 5', ':cpf 5'
    itn '5cpfile', ':5cpf'
    itn 'lNfile 5', ':lNf 5'
    itn '5lNfile', ':5lNf'
    itn 'cNfile 5', ':cNf 5'
    itn '5cNfile', ':5cNf'
    itn 'lprevious 5', ':lp 5'
    itn '5lprevious', ':5lp'
    itn 'cprevious 5', ':cp 5'
    itn '5cprevious', ':5cp'
    itn 'lNext 5', ':lN 5'
    itn '5lNext', ':5lN'
    itn 'cNext 5', ':cN 5'
    itn '5cNext', ':5cN'
    itn 'lopen 5', ':lop 5'
    itn '5lopen', ':5lop'
    itn 'copen 5', ':cope 5'
    itn '5copen', ':5cope'
    itn 'lolder 5', ':lol 5'
    itn '5lolder', ':5lol'
    itn 'colder 5', ':col 5'
    itn '5colder', ':5col'
    itn 'lnewer 5', ':lnew 5'
    itn '5lnewer', ':5lnew'
    itn 'cnewer 5', ':cnew 5'
    itn '5cnewer', ':5cnew'
    itn '5lnfile!', '5lnf!'
    itn 'lnfile! 5', 'lnf! 5'
    itn '5cnfile!', '5cnf!'
    itn 'cnfile! 5', 'cnf! 5'
    itn '5lnext!', '5lne!'
    itn 'lnext! 5', 'lne! 5'
    itn '5cnext!', '5cn!'
    itn 'cnext! 5', 'cn! 5'
    itn '5llast!', '5lla!'
    itn 'llast! 5', 'lla! 5'
    itn '5clast!', '5cla!'
    itn 'clast! 5', 'cla! 5'
    itn '5ll!', '5ll!'
    itn 'll! 5', 'll! 5'
    itn '5cc!', '5cc!'
    itn 'cc! 5', 'cc! 5'
    itn 'lfirst 5', ':lfir 5'
    itn '5lfirst', ':5lfir'
    itn 'cfirst 5', ':cfir 5'
    itn '5cfirst', ':5cfir'
    itn 'lclose 5', ':lcl 5'
    itn '5lclose', ':5lcl'
    itn 'cclose 5', ':ccl 5'
    itn '5cclose', ':5ccl'
    itn 'jumps', ':ju'
    itn 'helpfind', ':helpf'
    itn '5goto 1', ':5go1'
    itn '5,1foldopen!', ':5,1foldo!'
    itn '5,1foldclose!', ':5,1foldc!'
    itn '5,1fold', ':5,1fold'
    itn 'fixdel', 'fixdel'
    itn 'finish', 'fini'
    itn 'finally', 'fina'
    itn 'files!', 'files!'
    itn 'exusage', 'exu'
    itn 'endif', 'end'
    itn 'endfunction', 'endf'
    itn 'endfor', 'endfo'
    itn 'endtry', 'endt'
    itn 'endwhile', 'endw'
    itn 'else', 'el'
    itn 'diffthis', 'difft'
    itn 'diffoff!', 'diffo!'
    itn 'diffupdate!', 'diffu!'
    itn '0debuggreedy', '0debugg'
    itn 'cquit!', 'cq!'
    itn 'continue', 'con'
    itn 'comclear', 'comc'
    itn 'close!', 'clo!'
    itn 'checkpath!', 'che!'
    itn 'changes', ':changes'

  describe 'append/insert/change commands', ->
    itn 'append\n.', 'a'
    itn 'insert\n.', 'i'
    itn 'change\n.', 'c'
    itn 'append!\n.', 'a!'
    itn 'insert!\n.', 'i!'
    itn 'change!\n.', 'c!'
    itn '1,2append!\n.', '1,2a!'
    itn '1,2insert!\n.', '1,2i!'
    itn '1,2change!\n.', '1,2c!'
    itn 'append\nabc\n.', 'a\nabc\n.'
    itn 'insert\nabc\n.', 'i\nabc\n.'
    itn 'change\nabc\n.', 'c\nabc\n.'
    itn 'append\n  abc\n.', 'a\n  abc\n.'
    itn 'insert\n  abc\n.', 'i\n  abc\n.'
    itn 'change\n  abc\n.', 'c\n  abc\n.'
    itn 'append\n  abc\n.', 'a\n  abc\n  .'
    itn 'insert\n  abc\n.', 'i\n  abc\n  .'
    itn 'change\n  abc\n.', 'c\n  abc\n  .'

  describe ':*map/:*abbrev (but not *unmap/*unabbrev) commands', ->
    for trunc, full in pairs {
      map: 'map'
      ['map!']: 'map!'
      nm: 'nmap'
      vm: 'vmap'
      xm: 'xmap'
      smap: 'smap'
      om: 'omap'
      im: 'imap'
      lm: 'lmap'
      cm: 'cmap'
      no: 'noremap'
      ['no!']: 'noremap!'
      nn: 'nnoremap'
      vn: 'vnoremap'
      xn: 'xnoremap'
      snor: 'snoremap'
      ono: 'onoremap'
      ino: 'inoremap'
      ln: 'lnoremap'
      cno: 'cnoremap'
      ab: 'abbreviate'
      norea: 'noreabbrev'
      ca: 'cabbrev'
      cnorea: 'cnoreabbrev'
      ia: 'iabbrev'
      inorea: 'inoreabbrev'
    }
      itn full .. ' <buffer><unique> a b', trunc .. ' <unique><buffer> a b'
      itn full .. ' <buffer><unique> a b', trunc .. ' <unique> <buffer> a b'
      itn full .. ' a b', trunc .. ' a b'
      itn full .. ' a b', trunc .. ' a\t\t\tb'
      itn full .. ' <nowait><silent> a b', trunc .. ' <nowait>\t<silent> a b'
      itn full .. ' <special><script> a b', trunc .. ' <special>\t<script> a b'
      itn full .. ' <expr><unique> a 1', trunc .. ' <unique>\t<expr> a 1'
      itn full, trunc
      itn full .. ' <buffer>', trunc .. '<buffer>'
      itn full .. ' a', trunc .. ' a'
      itn full .. ' <buffer> a', trunc .. '<buffer>a'
      itn full .. ' a b', trunc .. ' a b|next command'

  describe ':*unmap/:*unabbrev commands', ->
    for trunc, full in pairs {
      unm: 'unmap'
      ['unm!']: 'unmap!'
      vu: 'vunmap'
      xu: 'xunmap'
      sunm: 'sunmap'
      ou: 'ounmap'
      iu: 'iunmap'
      lun: 'lunmap'
      cu: 'cunmap'
      una: 'unabbreviate'
      cuna: 'cunabbrev'
      iuna: 'iunabbrev'
    }
      itn full .. ' <buffer>', trunc .. '<buffer>'
      itn full, trunc
      itn full .. ' <buffer> a   b', trunc .. '<buffer>a   b'

  describe ':*mapclear/*abclear commands', ->
    for trunc, full in pairs {
      mapc: 'mapclear'
      ['mapc!']: 'mapclear!'
      nmapc: 'nmapclear'
      vmapc: 'vmapclear'
      xmapc: 'xmapclear'
      smapc: 'smapclear'
      omapc: 'omapclear'
      imapc: 'imapclear'
      lmapc: 'lmapclear'
      cmapc: 'cmapclear'
      abc: 'abclear'
      iabc: 'iabclear'
      cabc: 'cabclear'
    }
      itn full, trunc
      itn full, trunc .. ' \t '
      itn full .. ' <buffer>', trunc .. '<buffer>'
      itn full .. ' <buffer>', trunc .. '\t<buffer>'

  describe ':*menu commands', ->
    for trunc, full in pairs {
      me: 'menu'
      am: 'amenu'
      nme: 'nmenu'
      ome: 'omenu'
      vme: 'vmenu'
      sme: 'smenu'
      ime: 'imenu'
      cme: 'cmenu'
      noreme: 'noremenu'
      an: 'anoremenu'
      nnoreme: 'nnoremenu'
      onoreme: 'onoremenu'
      vnoreme: 'vnoremenu'
      snoreme: 'snoremenu'
      inoreme: 'inoremenu'
      cnoreme: 'cnoremenu'
    }
      itn '5' .. full, '5' .. trunc
      itn full, trunc
      itn full .. ' icon=abc.gif', trunc .. '\ticon=abc.gif'
      itn full .. ' enable', trunc .. '\tenable'
      itn full .. ' disable', trunc .. '\tdisable'
      itn full .. ' .2 abc.def ghi', trunc .. ' .2 abc.def ghi'
      itn full .. ' 1.2 abc.def ghi', trunc .. ' 1.2 abc.def ghi'
      itn full .. ' abc.def def', trunc .. ' abc.def def'
      itn full .. ' abc.def<Tab>def def', trunc .. ' abc.def<tAb>def def'
      itn full .. ' abc.def<Tab>def def', trunc .. ' abc.def\\\tdef def'

  describe 'expression commands', ->
    itn 'if 1', 'if1'
    itn 'elseif 1', 'elsei1'
    itn 'return 1', 'retu1'
    itn 'throw 1', 'th1'
    itn 'while 1', 'wh1'
    itn 'call tr(1, 2, 3)', 'cal tr(1, 2, 3)'
    itn 'call tr(1, 2, 3)', 'cal\ttr\t(1, 2, 3)'
    itn '1,2call tr(1, 2, 3)', '1,2cal\ttr\t\t\t(1, 2, 3)'
    for trunc, full in pairs {
      cex: 'cexpr'
      lex: 'lexpr'
      cgete: 'cgetexpr'
      lgete: 'lgetexpr'
      cadde: 'caddexpr'
      ladde: 'laddexpr'
    }
      itn full .. ' 1', trunc .. '1'

  describe 'user commands', ->
    itn 'Eq |abc|def', 'Eq|abc|def'
    itn '1Eq! |abc\\|def', '1Eq!|abc\\|def'

  describe 'language commands', ->
    itn 'luado print ("abc\\|def")', 'luad print ("abc\\|def")'
    itn 'pydo print ("abc\\|def")', 'pyd print ("abc\\|def")'
    itn 'py3do print ("abc\\|def")', 'py3d print ("abc\\|def")'
    itn 'rubydo print ("abc\\|def")', 'rubyd print ("abc\\|def")'
    itn 'perldo print ("abc\\|def")', 'perld print ("abc\\|def")'
    itn 'tcldo ::vim::command "echo 1\\|\\|2"',
      'tcld::vim::command "echo 1\\|\\|2"'
    itn '1luado print ("abc\\|def")', '1luad print ("abc\\|def")'
    itn '1pydo print ("abc\\|def")', '1pyd print ("abc\\|def")'
    itn '1py3do print ("abc\\|def")', '1py3d print ("abc\\|def")'
    itn '1rubydo print ("abc\\|def")', '1rubyd print ("abc\\|def")'
    itn '1perldo print ("abc\\|def")', '1perld print ("abc\\|def")'
    itn '1tcldo ::vim::command "echo 1\\|\\|2"',
      '1tcld::vim::command "echo 1\\|\\|2"'

  describe 'normal command', ->
    itn 'normal! abc\\|def', 'norm!abc\\|def'
    itn '2normal! abc\\|def', '2norm!abc\\|def'

  describe 'iterator commands', ->
    itn 'bufdo if 1 | endif', 'bufd:if1|end'
    itn 'windo if 1 | endif', 'wind:if1|end'
    itn 'tabdo if 1 | endif', 'tabd:if1|end'
    itn 'argdo if 1 | endif', 'argdo:if1|end'
    itn 'folddoopen if 1 | endif', 'foldd:if1|end'
    itn 'folddoclosed if 1 | endif', 'folddoc:if1|end'

  describe 'multiple expressions commands', ->
    for trunc, full in pairs {
      ec: 'echo'
      echon: 'echon'
      echom: 'echomsg'
      echoe: 'echoerr'
      exe: 'execute'
    }
      itn full, trunc
      itn full .. ' "abclear"', trunc .. ' "abclear"'
      itn full .. ' "abclear" "<buffer>"', trunc .. ' "abclear" "<buffer>"'
      itn full .. ' "abclear" "<buffer>"', trunc .. ' "abclear""<buffer>"'

  describe 'lvals commands', ->
    for trunc, full in pairs {
      unlo: 'unlockvar'
      unl: 'unlet'
      ['unl!']: 'unlet!'
    }
      itn full .. ' abc', trunc .. ' abc'
      itn full .. ' abc def', trunc .. ' abc def'
    itn 'lockvar abc', 'lockv abc'
    itn 'lockvar! abc', 'lockvar!abc'
    itn 'lockvar 5 abc', 'lockv5abc'
    itn 'delfunction Abc', 'delf Abc'
    itn 'delfunction {"abc"}', 'delf{"abc"}'

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

-- vim: sw=2 sts=2 et tw=80
