describe('parse_one_cmd', function()
  local t, itn
  do
    local _obj_0 = require('test.unit.viml.parser.helpers')(it)
    t, itn = _obj_0.t, _obj_0.itn
  end
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
      spr = 'sprevious',
      argu = 'argument',
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
  describe('! ++opt commands', function()
    itn('wqall', 'wqa')
    return itn('wqall!', 'wqa!')
  end)
  describe(':edit command family', function()
    for trunc, cmdprops in pairs({
      e = {full='edit', cmd=true, opts=true, bang=true},
      vi = {full='visual', cmd=true, opts=true, bang=true},
      vie = {full='view', cmd=true, opts=true, bang=true},
      ped = {full='pedit', cmd=true, opts=true, bang=true},
      ex = {full='ex', cmd=true, opts=true, bang=true},
      ar = {full='args', cmd=true, opts=true, bang=true},
      arga = {full='argadd', count=true},
      argd = {full='argdelete'},
      arge = {full='argedit', cmd=true, opts=true, count=true, bang=true},
      argg = {full='argglobal', cmd=true, opts=true, bang=true},
      argl = {full='arglocal', cmd=true, opts=true, bang=true},
      cf = {full='cfile'},
      cg = {full='cgetfile'},
      lf = {full='lfile'},
      lg = {full='lgetfile'},
      cd = {full='cd', bang=true},
      lcd = {full='lcd', bang=true},
      chd = {full='chdir', bang=true},
      lchd = {full='lchdir', bang=true},
      diffs = {full='diffsplit'},
      diffp = {full='diffpatch'},
      exi = {full='exit', opts=true, range=true, bang=true},
      x = {full='xit', opts=true, range=true, bang=true},
      fin = {full='find', cmd=true, opts=true, count=true, bang=true},
      sf = {full='sfind', cmd=true, opts=true, count=true, bang=true},
      laddf = {full='laddfile'},
      luafile = {full='luafile', range=true},
      mzf = {full='mzfile', range=true},
      pyf = {full='pyfile', range=true},
      py3f = {full='py3file', range=true},
      tclf = {full='tclfile', range=true},
      rubyf = {full='rubyfile', range=true},
      mk = {full='mkexrc', bang=true},
      mks = {full='mksession', bang=true},
      mkv = {full='mkvimrc', bang=true},
      mkvie = {full='mkview', bang=true},
      n = {full='next', cmd=true, opts=true, count=true, bang=true},
      sn = {full='snext', cmd=true, opts=true, count=true, bang=true},
      new = {full='new', cmd=true, opts=true, count=true, bang=true},
      vne = {full='vnew', cmd=true, opts=true, count=true, bang=true},
      sp = {full='split', cmd=true, opts=true, count=true, bang=true},
      vs = {full='vsplit', cmd=true, opts=true, count=true, bang=true},
      sv = {full='sview', cmd=true, opts=true, count=true, bang=true},
      tabe = {full='tabedit', cmd=true, opts=true, count=true, bang=true},
      tabnew = {full='tabnew', cmd=true, opts=true, count=true, bang=true},
      tabf = {full='tabfind', cmd=true, opts=true, count=true, bang=true},
      rec = {full='recover', bang=true},
      ru = {full='runtime', bang=true},
      so = {full='source', bang=true},
      rundo = {full='rundo'},
      wundo = {full='wundo', bang=true},
      rv = {full='rviminfo', bang=true},
      wv = {full='wviminfo', bang=true},
      sav = {full='saveas', opts=true, bang=true},
      wn = {full='wnext', opts=true, count=true, bang=true},
      wN = {full='wNext', opts=true, count=true, bang=true},
      wp = {full='wprevious', opts=true, count=true, bang=true},
    }) do
      full = cmdprops.full
      for trunc, full in pairs(
        cmdprops.bang and {[trunc]=full, [trunc .. '!']=full .. '!'} or {[trunc]=full}
      ) do
        itn(full .. ' lit(abc) lit(def)', full .. '  abc  def')
        itn(full .. ' lit({abc) lit(def)', trunc .. '  {abc  def')
        itn(full .. ' shell(echo abc).lit(def) lit(ghi)', trunc .. '  `echo abc`def ghi')
        itn(full .. ' expr("abc").lit(def) lit(ghi)', trunc .. '  `="abc"`def ghi')
        itn(full .. ' lit(`echo abc)', trunc .. '  `echo\\ abc')
        if cmdprops.count or cmdprops.range then
          itn('10' .. full .. ' lit(abc) lit(def)', '10' .. trunc .. ' abc def')
        end
        if cmdprops.range then
          itn('/abc/;/def/' .. full .. ' lit(abc) lit(def)', '/abc/;/def/' .. trunc .. ' abc def')
        end
        if cmdprops.opts then
          itn(full .. ' ++bin lit(abc) lit(def)', trunc .. ' ++binary abc def')
        end
        if cmdprops.cmd then
          itn(full .. ' +redraw lit(abc) lit(def)', trunc .. ' +redr abc def')
        end
      end
    end
  end)
end)
return describe('parse_cmd_sequence', function()
  local t, itn
  do
    local _obj_0 = require('test.unit.viml.parser.helpers')(it)
    t, itn = _obj_0.t, _obj_0.itn
  end
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
