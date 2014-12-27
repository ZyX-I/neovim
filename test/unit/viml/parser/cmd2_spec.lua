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
      caddf = {full='caddfile'},
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
      wq = {full='wq', opts=true, range=true, bang=true},
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
      checkt = {full='checktime', count=true},
      rundo = {full='rundo'},
      wundo = {full='wundo', bang=true},
      rv = {full='rviminfo', bang=true},
      wv = {full='wviminfo', bang=true},
      sav = {full='saveas', opts=true, bang=true},
      wn = {full='wnext', opts=true, count=true, bang=true},
      wN = {full='wNext', opts=true, count=true, bang=true},
      wp = {full='wprevious', opts=true, count=true, bang=true},
      b = {full='buffer', count=true, bufnr=true},
      sb = {full='sbuffer', count=true, bufnr=true},
      bad = {full='badd', cmd=true},
      bd = {full='bdelete', count=true, bufnr=true, bang=true},
      bw = {full='bwipeout', count=true, bufnr=true, bang=true},
      bun = {full='bunload', count=true, bufnr=true, bang=true},
      diffg = {full='diffget', range=true},
      diffpu = {full='diffput', range=true},
      dr = {full='drop', cmd=true, opts=true},
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
        if cmdprops.bufnr then
          itn(full .. ' lit(10)', trunc .. '10')
          itn(full .. ' lit(10a)', trunc .. '10a')
        elseif trunc:match('^py') then
          -- Vim also throws E492 for py3f10 and pyf10
          itn('\\ error: E492: Not an editor command: !!' .. trunc:sub(1, 1) .. '!!' .. trunc:sub(2) .. '10', trunc .. '10')
        else
          itn(full .. ' lit(10)', trunc .. '10')
        end
      end
    end
  end)
  describe(':autocmd', function()
    itn('autocmd GROUP BufNewFile,BufAdd any().lit(abc) nested :echo abc | echo def',
    'au GROUP BufNewFile,BufAdd *abc nested ::::::\techo abc|echo def')
    itn('autocmd GROUP BufNewFile,BufAdd any().lit(abc) :echo abc | echo def',
    'au GROUP BufNewFile,BufAdd *abc ::::::\techo abc|echo def')
    itn('autocmd BufNewFile,BufAdd any().lit(abc) nested :echo abc | echo def',
    'au BufNewFile,BufAdd *abc nested ::::::\techo abc|echo def')
    itn('autocmd GROUP * any().lit(abc) :echo abc | echo def',
    'au GROUP * *abc ::::::\techo abc|echo def')
    itn('autocmd GROUP',
    'au\tGROUP   ')
    itn('autocmd GROUP BufAdd',
    'au\tGROUP \t  BufAdd')
    itn('autocmd GROUP BufAdd any()',
    'au\tGROUP \t  BufAdd *')
    itn('autocmd BufAdd any()',
    'au\t\t  BufAdd *')
    itn('autocmd *',
    'au\t\t  *')
    itn('autocmd BufAdd any()',
    'au\t\t  BufCreate *')
    itn('autocmd BufReadPost any()',
    'au\t\t  BufRead *')
    itn('autocmd BufWritePre any()',
    'au\t\t  BufWrite *')
    itn('autocmd EncodingChanged any()',
    'au\t\t  FileEncoding *')
  end)
  describe(':cbuffer and friends', function()
    for trunc, full in pairs({
      cad = 'caddbuffer',
      cb = 'cbuffer',
      cgetb = 'cgetbuffer',
      laddb = 'laddbuffer',
      lb = 'lbuffer',
      lgetb = 'lgetbuffer',
    }) do
      itn('10' .. full, '10' .. trunc)
      itn(full .. ' 10', trunc .. '10')
      if trunc:sub(2, 2) == 'b' then
        itn('10' .. full .. '!', '10' .. trunc .. '!')
        itn(full .. '! 10', trunc .. '!10')
      end
    end
  end)
  describe('align commands', function()
    for trunc, full in pairs({
      ce = 'center',
      le = 'left',
      ri = 'right',
    }) do
      itn('$' .. full .. ' 0', '$  ' .. trunc)
      itn(full .. ' 0', trunc .. '-abc')
      itn(full .. ' -10', trunc .. '-10')
      itn(full .. ' 10', trunc .. '10')
    end
  end)
  describe(':clist and :llist', function()
    for trunc, full in pairs({
      cl = 'clist',
      lli = 'llist',
      ['cl!'] = 'clist!',
      ['lli!'] = 'llist!',
    }) do
      itn(full .. ' 1, -1', trunc)
      itn(full .. ' 2, 2', trunc .. '2')
      itn(full .. ' 1, 2', trunc .. ',2')
      itn(full .. ' -2, -2', trunc .. '-2')
      itn(full .. ' 1, -2', trunc .. ',-2')
    end
  end)
  describe(':ilist and friends', function()
    for _trunc, _full in pairs({
      dj = 'djump',
      ij = 'ijump',
      tj = 'tjump',
      ptj = 'ptjump',
      stj = 'stjump',
      dli = 'dlist',
      il = 'ilist',
      ds = 'dsearch',
      is = 'isearch',
      ps = 'psearch',
      dsp = 'dsplit',
      lt = 'ltag',
      -- :ptag is officially abbreviated as :pta, even though :pt works as well
      pta = 'ptag',
      sta = 'stag',
      ta = 'tag',
      sts = 'stselect',
      pts = 'ptselect',
    }) do
      for _, suffix in ipairs({'', '!'}) do
        local full = _full .. suffix
        local trunc = _trunc .. suffix
        local ch = full:sub(3, 3)
        itn(full .. ' /abc/', trunc..'/abc')
        itn(full .. ' /abc/', trunc..'/abc/')
        itn(full .. ' /\\V\\<abc\\>/', trunc..' abc')
        itn(full, trunc)
        if (
          ch == 'u' or ch == 'j'  -- *jump
          or ch == 'e'            -- *search
          or ch == 'p'            -- *split
        ) then
          if not ({ptj=1, stj=1, tj=1, sts=1, pts=1, ts=1})[_trunc] then
            itn(full .. ' 10 /abc/', trunc..'10/abc')
            itn(full .. ' 10 /abc/', trunc..'10/abc/')
            itn(full .. ' 10 /\\V\\<abc\\>/', trunc..'10abc')
            itn(full .. ' 10', trunc..'10')
          end
        end
      end
    end
  end)
  describe(':copy/:move/:t', function()
    itn('1,$copy 12+1;/abc/', '%co12+1;/abc')
    itn('1,$move 12+1;/abc/', '%m12+1;/abc')
    itn('1,$t 12+1;/abc/', '%t12+1;/abc')
    itn('copy', 'co')
    itn('move', 'm')
    itn('t', 't')
  end)
  describe(':command', function()
    itn('command -bar Abc', 'com-BaR Abc')
    itn('command -bang Abc', 'com-bAnG Abc')
    itn('command -buffer Abc', 'com-buFFer Abc')
    itn('command -register Abc', 'com-RegIster Abc')
    itn('command -bang -buffer -bar -register Abc', 'com-register -bang -bar -buffer Abc')
    itn('command -nargs=? Abc', 'com-nargs=? Abc')
    itn('command Abc', 'com-nargs=0 Abc')
    itn('command -nargs=1 Abc', 'com-nargs=1 Abc')
    itn('command -nargs=* Abc', 'com-nargs=* Abc')
    itn('command -nargs=+ Abc', 'com-nargs=+ Abc')
    itn('command -range=1 Abc', 'com-range=1 Abc')
    itn('command -range=% Abc', 'com-range=% Abc')
    itn('command -range Abc', 'com-range Abc')
    itn('command -count Abc', 'com-count Abc')
    itn('command -count=1 Abc', 'com-count=1 Abc')
    itn('command -range=1 -count Abc', 'com-count -range=1 Abc')
    itn('command -range -count=1 Abc', 'com-count=1 -range Abc')
    itn('command -complete=dir Abc', 'com-complete=dir Abc')
    itn('command -complete=custom,Def Abc', 'com-complete=custom,Def Abc')
    itn('command -complete=customlist,Def Abc', 'com-complete=customlist,Def Abc')
    itn('command! Abc abc', 'com!Abc abc')
    itn('command! -nargs=1 Abc silent <args>', 'com!-nargs=1 Abc silent <args>')
  end)
  describe(':delmarks', function()
    itn('delmarks 1A-Za-z', 'delm a-z A-Z 1')
    itn('delmarks!', 'delm!')
  end)
  describe(':digraph', function()
    itn('digraphs', 'dig')
    itn('digraphs ++ 10', 'dig++10')
    itn('digraphs ++ 10 -- 20', 'dig++10--20')
  end)
  describe(':doautocmd/:doautoall', function()
    for trunc, full in pairs({
      ['do'] = 'doautocmd',
      doautoa = 'doautoall',
    }) do
      itn(full .. ' GROUP BufNewFile,BufAdd abc.def',
      trunc .. ' GROUP BufNewFile,BufAdd abc.def')
      itn(full .. ' GROUP',
      trunc .. '\tGROUP   ')
      itn(full .. ' GROUP BufAdd',
      trunc .. '\tGROUP \t  BufAdd')
      itn(full .. ' GROUP BufAdd *',
      trunc .. '\tGROUP \t  BufAdd *')
      itn(full .. ' BufAdd *',
      trunc .. '\t\t  BufAdd *')
      itn(full .. ' BufAdd *',
      trunc .. '\t\t  BufCreate *')
      itn(full .. ' <nomodeline> BufAdd foo.bar',
      trunc .. '<nomodeline>BufAdd \t\t\t\t     foo.bar')
    end
  end)
  describe(':earlier/:later', function()
    for trunc, full in pairs({
      lat = 'later',
      ea = 'earlier',
    }) do
      itn(full .. ' 1', trunc)
      for _, c in ipairs({'s', 'm', 'h', 'd', 'f'}) do
        itn(full .. ' 50' .. c, trunc .. '50' .. c)
      end
    end
  end)
  describe(':filetype', function()
    itn('filetype', 'filet')
    itn('filetype on', 'filet on')
    itn('filetype off', 'filet off')
    itn('filetype detect', 'filet detect')
    itn('filetype plugin indent on', 'filet pluginindenton')
    itn('filetype plugin indent on', 'filet indent\t\t\t\tplugin\t    \ton')
  end)
  describe(':history', function()
    itn('history 1, -1', 'his')
    itn('history all 1, -1', 'his a')
    itn('history all 1, -1', 'his A')
    itn('history all 1, -1', 'his AlL')
    itn('history search 1, -1', 'his s')
    itn('history search 1, -1', 'his sea')
    itn('history search 1, -1', 'his SeA')
    itn('history search 1, -1', 'his SeArCh')
    itn('history search 1, -1', 'his ?')
    itn('history search 1, -1', 'his /')
    itn('history expr -5, -1', 'his expr -5,')
    itn('history expr -5, -5', 'his expr -5')
    itn('history expr -5, -5', 'his = -5')
    itn('history expr -5, -5', 'his E -5')
    itn('history cmd 1, -4', 'his : ,-4')
    itn('history cmd 1, -4', 'his C ,-4')
    itn('history cmd 1, -4', 'his cmd ,-4')
    itn('history input 1, -4', 'his in,-4')
    itn('history input 1, -4', 'his @,-4')
    itn('history input 1, -4', 'his INPUT,-4')
    itn('history debug 1, -4', 'his >,-4')
    itn('history debug 1, -4', 'his D,-4')
    itn('history debug 1, -4', 'his debug,-4')
  end)
  describe(':mark/:k', function()
    itn('k a', 'ka')
    itn('mark A', 'mark A')
    itn('mark `', 'mark`')
    itn('mark \'', 'mark\'')
  end)
  describe(':popup/:tearoff', function()
    itn('popup abc.def\tghi', 'popu abc.def\tghi')
    itn('popup! ]abc', 'popu!]abc')
    itn('tearoff abc.def\tghi', 'te abc.def\tghi')
    itn('tearoff ]abc', 'te]abc')
  end)
  describe(':retab', function()
    itn('retab! 1', 'ret!1')
    itn('retab', 'ret')
  end)
  describe(':resize', function()
    itn('resize 0', 'resize')
    itn('resize +0', 'res+abc')
    itn('resize -10', 'res-10')
  end)
  describe(':redir', function()
    itn('redir @a', 'redi@a')
    itn('redir! > file', 'redi!>file')
    itn('redir! >> file name', 'redi!>>file name')
    itn('redir => d[abc . def]', 'redi=>d[abc   .   def]')
    itn('redir @+', 'redir @+')
    itn('redir @+>>', 'redir @+>>')
    itn('redir @*', 'redir @*')
    itn('redir @*>>', 'redir @*>>')
  end)
  describe(':open', function()
    itn('open', 'o')
    itn('open!', 'o!')
    itn('open /abc def ghi/', 'o/abc def ghi')
    itn('open /abc def ghi/', 'o/abc def ghi/ jkl')
  end)
  describe(':[v]global', function()
    itn('global /abc/', 'g/abc')
    itn('vglobal /abc/', 'v/abc')
    itn('global /abc/ :abclear', 'g/abc/abc')
    itn('global! /abc/ :abclear', 'g!_abc_abc')
    -- Regression test: " used to start a comment, the whole command used to 
    -- SEGV.
    itn('global /{"d"}{"e"}{"f"}/', 'g:{"d"}{"e"}{"f"}')
  end)
  describe(':[l]vimgrep[add]', function()
    itn('lvimgrep /abcgj/ lit(def)', 'lv abcgj def')
    itn('lvimgrep /abc/ lit(gj) lit(def)', 'lv abc gj def')
    itn('lvimgrepadd /abc/ lit(def)', 'lvimgrepa /abc/ def')
    itn('vimgrep /abc/gj lit(def)', 'vim /abc/gj def')
    itn('vimgrepadd /abc/j lit(def)', 'vimgrepa /abc/j def')
  end)
  describe(':marks', function()
    itn('marks abc', 'marks /a b\tc')
  end)
  describe(':match', function()
    itn('match', 'match')
    itn('match', 'mat none')
    itn('match', 'mat NoNe')
    itn('match HlGroup /bc/', 'match HlGroup Abc')
    itn('match HlGroup /bc/', 'match HlGroup AbcA    ')
  end)
  describe(':sleep', function()
    itn('sleep', 'sl')
    itn('sleep 10', 'sl10')
    itn('sleep 10m', 'sl10m')
    -- :sleep in Vim does not check character after `m`
    itn('sleep 10m', 'sl10ms')
  end)
  describe(':substitute, :&, :~', function()
    for trunc, full in pairs({
      s='substitute',
      sm='smagic',
      -- FIXME Changing `sno` to `sn` (stands for :snext) causes a crash
      sno='snomagic',
    }) do
      itn(full .. '/abc/def/g', trunc .. '/abc/def/g', 0)
      itn(full .. '/abc/def/g', trunc .. '/abc/def/g', 0x400)
      itn(full .. '/abc/def/g', trunc .. '/abc/def/gg', 0)
      itn(full .. '/abc/def/gg', trunc .. '/abc/def/gg', 0x400)
      itn(full .. '/abc/def/g', trunc .. '/abc/def/ggg', 0)
      itn(full .. '/abc/def/g', trunc .. '/abc/def/ggg', 0x400)
      itn(full .. '/abc/%/', trunc .. '/abc/%', 0x40)
      itn(full .. '/abc/%/', trunc .. '/abc/%', 0)
      itn(full .. '///', trunc .. '')
      itn(full .. '/abc//', trunc .. '/abc')
      itn(full .. '/ab\\@<=c/def/#', trunc .. '@ab\\@<=c@def@#')
      itn(full .. '/a/&/', trunc .. '/a/&', 0)
      itn(full .. '/a/\\0/', trunc .. '/a/\\&', 0)
      itn(full .. '/a/\\0/', trunc .. '/a/&', 0x200)
      itn(full .. '/a/\\&/', trunc .. '/a/\\&', 0x200)
      itn(full .. '/a/~/', trunc .. '/a/~')
      itn(full .. '/a/\\~/', trunc .. '/a/\\~')
      -- Note: `c` flag is missing. This is not a bug: `n` cancels `c`.
      itn(full .. [[/\(abc\)\(def\)\(ghi\)/\u\L\1\E\l\U\2\E\t/einr]],
          trunc .. [[-\(abc\)\(def\)\(ghi\)-\u\L\1\E\l\U\2\E\t-ciren]])
      itn(full .. '//\\=abc . def/cI#', trunc .. '//\\=abc. def/cI#')
    end
    itn('&&10', '&10')
    itn('&&10', '&&10')
    itn('&&r', '&r')
    itn('~&r', '~&r')
    itn('~&i', '~i')
  end)
  describe(':sort', function()
    itn('\'<,\'>sort niru /abc\\|def/', '*sor runi -abc\\|def-')
    itn('\'<,\'>sort niru', '*sor irun')
    itn('\'<,\'>sort', '*sor')
    itn('\'<,\'>sort xiru', '*sor irux')
    itn('\'<,\'>sort oiru', '*sor iruo')
  end)
  describe(':spellgood/:spellwrong', function()
    itn('spellgood abc', 'spe abc')
    itn('spellwrong abc', 'spellw abc')
    itn('spellundo abc', 'spellu abc')
    itn('spellgood! abc', 'spellg!abc')
    itn('spellwrong! abc', 'spellw!abc')
    itn('spellundo! abc', 'spellu!abc')
    itn('10spellgood abc', '10spe abc')
    itn('10spellwrong abc', '10spellw abc')
    itn('10spellundo abc', '10spellu abc')
    itn('10spellgood! abc', '10spellg!abc')
    itn('10spellwrong! abc', '10spellw!abc')
    itn('10spellundo! abc', '10spellu!abc')
  end)
  describe(':syntime', function()
    itn('syntime on', 'syntime on')
    itn('syntime off', 'syntime\toff')
    itn('syntime report', 'syntime\treport\t\t')
    itn('syntime clear', 'syntime     clear\t    \t')
  end)
  describe(':tabmove', function()
    itn('10tabmove', '10     tabm')
    itn('tabmove +10', 'tabm+10')
    itn('tabmove -10', 'tabm-10')
  end)
  describe(':winsize/:winpos', function()
    itn('winpos', 'winp')
    itn('winsize -1 0', 'win-1 0')
    itn('winpos -1 0', 'winp-1 0')
    itn('winsize 10 -10', 'win10-10')
    itn('winpos 10 -10', 'winp10-10')
    itn('winsize 42 52', 'win42\t\t52')
    itn('winpos 42 52', 'winp42\t\t52')
  end)
  describe(':wincmd', function()
    for action, chars in pairs({
      s={'\19', 'S'},
      v={'\22'},
      ['^']={'\30'},
      n={'\14'},
      q={'\17'},
      c={'\3'},
      z={'\26'},
      P={},
      o={'\15'},
      w={'\23'},
      W={},
      j={--[['\10']]},
      k={'\11'},
      l={'\12'},
      T={},
      t={'\20'},
      b={'\2'},
      p={'\16'},
      x={'\24'},
      r={'\18'},
      R={},
      K={},
      J={},
      H={},
      L={},
      ['=']={},
      ['+']={},
      ['-']={},
      ['_']={'\31'},
      ['>']={},
      ['<']={},
      ['|']={},
      ['}']={},
      [']']={'\29'},
      f={'\6', 'F'},
      i={},
      d={'\4'},
      ['\r']={},
      ['g}']={},
      ['g]']={'g\29'},
      gf={},
      gF={},
    }) do
      table.insert(chars, action)
      for _, char in ipairs(chars) do
        itn('wincmd ' .. action, 'winc ' .. char)
        itn('10wincmd ' .. action, '10     winc ' .. char)
      end
    end
    itn('wincmd !', 'wincmd "')
  end)
  describe(':z', function()
    itn('z+10', 'z +10')
    itn('z+++10', 'z +++--+++---++10')
    itn('z=10', 'z=+-+10')
    itn('z^10', 'z^-+-10')
    itn('z.10', 'z.-+-10')
    itn('z10', 'z          10')
    itn('z+', 'z    +')
    itn('z.', 'z    .')
    itn('1,$z^', '%z^')
    itn('1,$z-', '%z-')
    itn('1,$z-----', '%z-----')
  end)
  describe(':*/:@', function()
    itn('\'<,\'>*@', '***')
    itn('@@', '@')
    itn('@+', '@      +')
    itn('@a', '@and the rest is ignored')
  end)
  describe(':help', function()
    itn('help', 'h')
    itn('help @ru', 'h@ru')
    itn('help abc', 'h abc')
    itn('help .@ru', 'h.@ru')
  end)
  describe(':(l)helpgrep', function()
    itn('helpgrep @ru', 'helpg@ru')
    itn('lhelpgrep @ru', 'lh@ru')
    itn('helpgrep /abc/', 'helpg/abc/')
    itn('lhelpgrep /abc/', 'lh/abc/')
    itn('helpgrep .@ru', 'helpg.@ru')
    itn('lhelpgrep .@ru', 'lh.@ru')
  end)
  describe(':cstag', function()
    itn('cstag abc', 'cstag\tabc')
  end)
  describe(':gui/:gvim', function()
    itn('gui -b', 'gu')
    itn('gui! -b', 'gu!')
    itn('gvim! -b', 'gv!')
    itn('gvim -f any()', 'gvim -f *')
    itn('gui -b lit(-f).any()', 'gu -f*')
    itn('gui +normal!\\ gF -b', 'gu+norm!gF')
    itn('gvim -f', 'gv-f')
    itn('gvim -f lit(+redraw!)', 'gv-f +redraw!')
  end)
  describe(':helptags', function()
    itn('helptags lit(++t)', 'helpt++t')
    itn('helptags ++t any()', 'helpt++t *')
  end)
  describe(':language', function()
    itn('language', 'lan')
    itn('language messages', 'lan MeS')
    itn('language Me', 'lan Me')
    itn('language ctype', 'lan cTy')
    itn('language time', 'lan tim')
    itn('language ct', 'lan ct')
    itn('language ti', 'lan ti')
    itn('language messages ru_RU.UTF-8', 'lan mes ru_RU.UTF-8')
    itn('language time abc def\tghi', 'lan TIM abc def\tghi')
    itn('language ctype tt     ttt t t tt   t', 'lan ctype tt     ttt t t tt   t  \t')
  end)
  describe('script commands', function()
    itn([[
perl << EOFEOFEOFEOF
my $s = << "EOF";
EOFEOFEOF
EOFEOF
EOF
EOFEOFEOFEOF
]], [[
perl << EOS
my $s = << "EOF";
EOFEOFEOF
EOFEOF
EOF
EOS
]])
  itn([[
python << EOFEOFEOFEOF
'''No heredocs in Python.
<< EOF
EOFEOFEOF
EOFEOF
EOF
'''
EOFEOFEOFEOF
]], [[
python << EOS
'''No heredocs in Python.
<< EOF
EOFEOFEOF
EOFEOF
EOF
'''
EOS
]])
  itn([[
py3 << EOFEOFEOFEOF
'''No heredocs in Python.
<< EOF
EOFEOFEOF
EOFEOF
EOF
'''
EOFEOFEOFEOF
]], [[
py3 << EOS
'''No heredocs in Python.
<< EOF
EOFEOFEOF
EOFEOF
EOF
'''
EOS
]])
  itn([[
python3 << EOFEOFEOFEOF
'''No heredocs in Python.
<< EOF
EOFEOFEOF
EOFEOF
EOF
'''
EOFEOFEOFEOF
]], [[
python3 << EOS
'''No heredocs in Python.
<< EOF
EOFEOFEOF
EOFEOF
EOF
'''
EOS
]])
  itn([[
lua << EOFEOF
--[=[ lua multiline comment
<< EOF
EOF
]=]
EOFEOF
]], [[
lua <<
--[=[ lua multiline comment
<< EOF
EOF
]=]
.
]])
  itn('tcl puts [::vim::expr "\\[1, 2, 3\\]"]', [[
tcl << EOS
puts [::vim::expr "\[1, 2, 3\]"]
EOS
]])
  itn('mzscheme ;; Comment', [[
mzscheme << EOF
;; Comment
EOF
]])
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
  describe('expression commands', function()
    t('echo 1\necho 2', 'echo1|echo2')
    return t('\n    if 1\n      return 1\n    endif\n    ', 'if 1 | return 1 | endif')
  end)
  t('behave mswin\nbehave xterm', 'behave mswin|behave xterm')
  describe('yank, delete, put', function()
    t([[
      yank a
      delete b
      put _
    ]], 'y a|d b|pu_')
    t([[
      put =echo
      "abc"
    ]], 'put =echo"abc"')
    t([[
      put =def
      print
      abclear
    ]], 'put =def||abc')
  end)
  describe(':delcommand', function()
    t([[
      delcommand Abc Def
      echo 1
    ]], 'delcommand Abc Def | echo 1')
  end)
  describe(':doautocmd', function()
    t([[
      doautocmd GROUP BufNewFile,BufAdd *abc nested ::::::	echo abc
      echo def
    ]], 'doau GROUP BufNewFile,BufAdd *abc nested ::::::\techo abc|echo def')
  end)
  describe(':[l]make, :[l]grep[add]', function()
    itn('make', 'mak')
    itn('lmake', 'lmak')
    itn('make install', 'make install')
    t('lmake install\necho "bacc"', 'lmak install|echo"bacc"')
    t('make install\necho "bacc"', 'mak install|echo"bacc"')
    t('grep abc def\necho "bacc"', 'gr abc def|echo"bacc"')
    t('lgrep abc def\necho "bacc"', 'lgr abc def|echo"bacc"')
    t('grepadd abc def\necho "bacc"', 'grepa abc def|echo"bacc"')
    t('lgrepadd abc def\necho "bacc"', 'lgrepa abc def|echo"bacc"')
  end)
  describe(':set, :setlocal, :setglobal', function()
    itn('setlocal', 'setl')
    itn('setglobal', 'setg')
    itn('set', 'se')
    t('set number nonumber number! number!\necho abc',
      'set nu\tnonu\tinvnu\tnu!|echo abc')
    t('set number& number&vim number&vi number<\n"abc',
      'set nu& nu&vim nu&vi nu<"abc')
    t('set aleph=255 aleph=16 aleph=8 aleph=10',
      'set al=0xFF al=0x10 al=010 al=10')
    t('set aleph=255 aleph=16 aleph=8 aleph=10',
      'set al:0xFF al:0x10 al:010 al:10')
    t('set aleph+=1 aleph-=1 aleph^=1',
      'set al+=1 al-=1 al^=1')
    t('set aleph& aleph&vim aleph&vi aleph<',
      'set al& aleph&vim aleph&vi aleph<')
    t('set aleph?', 'se al?')
    t('set all? all& termcap?', 'se all all& termcap')
    t('set clipboard=unnamed clipboard=unnamed clipboard= clipboard=',
      'set cb:unnamed cb=unnamed cb: cb=')
    t('set clipboard+=unnamed clipboard-=unnamed clipboard^=unnamed',
      'set cb+=unnamed cb-=unnamed cb^=unnamed')
    t('set clipboard& clipboard&vim clipboard&vi clipboard< clipboard?',
      'set cb& cb&vim cb&vi cb< cb?')
    t('set autoindent?', 'se ai    ?')
    t('set wildchar=9 wildcharm=9 wildchar=0', 'se wc=<Tab> wcm=^I wc=0')
    t('set formatlistpat=\\"foo\\|\\\\\\ bar\\"', 'set formatlistpat=\\"foo\\|\\\\\\ bar\\"')
  end)
  describe(':wincmd', function()
    t([[
    wincmd |
    echo abc
    ]], 'wincmd||echo abc')
    t([[
    wincmd !
    echo abc
    ]], 'wincmd g||echo abc')
  end)
end)
