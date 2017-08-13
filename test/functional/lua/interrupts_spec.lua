-- Test suite for checking :lua* commands
local helpers = require('test.functional.helpers')(after_each)
local thelpers = require('test.functional.terminal.helpers')

local eq = helpers.eq
local neq = helpers.neq
local NIL = helpers.NIL
local feed = helpers.feed
local clear = helpers.clear
local meths = helpers.meths
local funcs = helpers.funcs
local sleep = helpers.sleep
local source = helpers.source
local dedent = helpers.dedent
local exc_exec = helpers.exc_exec
local write_file = helpers.write_file
local redir_exec = helpers.redir_exec
local curbufmeths = helpers.curbufmeths
local get_pending_win32 = helpers.get_pending_win32

local tui_screen_setup = thelpers.tui_screen_setup

local pend_w32 = get_pending_win32(pending)

before_each(clear)

local function lua_screen_setup(...)
  local screen = tui_screen_setup(
    '--cmd', 'lua i = 1',
    '--cmd', 'lua runs = 1',
    '--cmd', [[lua function hlt()
      print("started")
      runs = runs + 1
      while true do
        i = i * 10
      end
    end
    function test()
      err = {pcall(hlt)}
      print("stopped")
      print(#err, err[1], err[2])
    end]],
    ...)
  screen:expect([[
    {1: }                                                 |
    {4:~                                                 }|
    {4:~                                                 }|
    {4:~                                                 }|
    {5:[No Name]                                         }|
                                                      |
    {3:-- TERMINAL --}                                    |
  ]])
  return screen
end

local function screen_check_exit(screen)
  screen:expect([[
                                                      |
    [Process exited 1]{1: }                               |
                                                      |
                                                      |
                                                      |
                                                      |
    {3:-- TERMINAL --}                                    |
  ]])
end
local function screen_check_ctrl_c(screen)
  screen:expect([[
    {1: }                                                 |
    {4:~                                                 }|
    {4:~                                                 }|
    {4:~                                                 }|
    {5:[No Name]                                         }|
                                                      |
    {3:-- TERMINAL --}                                    |
  ]])
end

describe(':lua command', function()
  it('may be interrupted with <C-c>', function()
    if pend_w32(':terminal nvim') then return end
    local screen = lua_screen_setup()
    feed(':lua test()<CR>')
    screen:expect([[
                                                        |
      {4:~                                                 }|
      {4:~                                                 }|
      {4:~                                                 }|
      {5:[No Name]                                         }|
      started{1: }                                          |
      {3:-- TERMINAL --}                                    |
    ]])
    feed('<C-c>')
    screen_check_ctrl_c(screen)
    feed('<C-l>:messages<CR>')
    screen:expect([[
      {4:~                                                 }|
      {5:[No Name]                                         }|
      started                                           |
      stopped                                           |
      2 false interrupted!                              |
      {10:Press ENTER or type command to continue}{1: }          |
      {3:-- TERMINAL --}                                    |
    ]])
    feed(':cq<CR>')
    screen_check_exit(screen)
  end)
  it('may be interrupted with <C-c> without additional :terminal layer',
  function()
    feed(':lua while true do i = nil end<CR>')
    sleep(100)
    feed('<C-c>')
    eq({blocking=false, mode='n'}, meths.get_mode())
    eq('', funcs.getline(1))
  end)
end)

describe(':luado command', function()
  it('may be interrupted with <C-c>', function()
    if pend_w32(':terminal nvim') then return end
    local screen = lua_screen_setup()
    feed('iabc\ndef\nghi<ESC>')
    screen:expect([[
      abc                                               |
      def                                               |
      gh{1:i}                                               |
      {4:~                                                 }|
      {5:[No Name] [+]                                     }|
                                                        |
      {3:-- TERMINAL --}                                    |
    ]])
    feed(':%luado test()<CR>')
    screen:expect([[
      abc                                               |
      def                                               |
      ghi                                               |
      {4:~                                                 }|
      {5:[No Name] [+]                                     }|
      started {1: }                                         |
      {3:-- TERMINAL --}                                    |
    ]])
    feed('<C-c>')
    screen:expect([[
      abc                                               |
      def                                               |
      ghi                                               |
      {4:~                                                 }|
      {5:[No Name] [+]                                     }|
      started {1: }                                         |
      {3:-- TERMINAL --}                                    |
    ]])
    feed('<C-c>')
    screen:expect([[
      abc                                               |
      def                                               |
      gh{1:i}                                               |
      {4:~                                                 }|
      {5:[No Name] [+]                                     }|
                                                        |
      {3:-- TERMINAL --}                                    |
    ]])
    feed('<C-c>')
    screen:expect([[
      abc                                               |
      def                                               |
      gh{1:i}                                               |
      {4:~                                                 }|
      {5:[No Name] [+]                                     }|
      Type  :quit<Enter>  to exit Nvim                  |
      {3:-- TERMINAL --}                                    |
    ]])
    feed(':messages<CR>')
    screen:expect([[
      started                                           |
      stopped                                           |
      2 false interrupted!                              |
      started                                           |
      stopped                                           |
      {10:-- More --}{1: }                                       |
      {3:-- TERMINAL --}                                    |
    ]])
    feed('<Space>')
    screen:expect([[
      2 false interrupted!                              |
      started                                           |
      stopped                                           |
      2 false interrupted!                              |
      Type  :quit<Enter>  to exit Nvim                  |
      {10:Press ENTER or type command to continue}{1: }          |
      {3:-- TERMINAL --}                                    |
    ]])
    feed(':cq<CR>')
    screen_check_exit(screen)
  end)
  it('may be interrupted with <C-c> without additional :terminal layer',
  function()
    feed('iabc<CR>def<CR>ghi<Esc>:%luado while true do i = nil end<CR>')
    sleep(100)
    feed('<C-c><C-c><C-c>')
    eq({blocking=false, mode='n'}, meths.get_mode())
    eq('abc', funcs.getline(1))
  end)
end)

describe(':luafile', function()
  local fname = 'Xtest-functional-lua-commands-luafile'

  after_each(function()
    os.remove(fname)
  end)

  it('may be interrupted with <C-c>', function()
    if pend_w32(':terminal nvim') then return end
    write_file(fname, 'test()')
    local screen = lua_screen_setup()
    feed(':luafile ' .. fname .. '<CR>')
    screen:expect([[
                                                        |
      {4:~                                                 }|
      {4:~                                                 }|
      {4:~                                                 }|
      {5:[No Name]                                         }|
      started{1: }                                          |
      {3:-- TERMINAL --}                                    |
    ]])
    feed('<C-c>')
    screen_check_ctrl_c(screen)
    feed(':messages<CR>')
    screen:expect([[
      {4:~                                                 }|
      {5:[No Name]                                         }|
      started                                           |
      stopped                                           |
      2 false interrupted!                              |
      {10:Press ENTER or type command to continue}{1: }          |
      {3:-- TERMINAL --}                                    |
    ]])
    feed(':cq<CR>')
    screen_check_exit(screen)
  end)
  it('may be interrupted with <C-c> without additional :terminal layer',
  function()
    write_file(fname, 'while true do i = nil end')
    feed(':luafile ' .. fname .. '<CR>')
    sleep(100)
    feed('<C-c>')
    eq({blocking=false, mode='n'}, meths.get_mode())
    eq('', funcs.getline(1))
  end)
end)

describe('luaeval()', function()
  it('may be interrupted with <C-c>', function()
    if pend_w32(':terminal nvim') then return end
    local screen = lua_screen_setup()
    feed(':echomsg luaeval("test()")<CR>')
    screen:expect([[
                                                        |
      {4:~                                                 }|
      {4:~                                                 }|
      {4:~                                                 }|
      {5:[No Name]                                         }|
      started{1: }                                          |
      {3:-- TERMINAL --}                                    |
    ]])
    feed('<C-c>')
    screen_check_ctrl_c(screen)
    feed(':messages<CR>')
    screen:expect([[
      {5:[No Name]                                         }|
      started                                           |
      stopped                                           |
      2 false interrupted!                              |
      null                                              |
      {10:Press ENTER or type command to continue}{1: }          |
      {3:-- TERMINAL --}                                    |
    ]])
    feed(':cq<CR>')
    screen_check_exit(screen)
  end)
  it('may be interrupted with <C-c> without additional :terminal layer',
  function()
    feed(':call luaeval("(function() while true do i = nil end end)()")<CR>')
    sleep(100)
    feed('<C-c>')
    eq({blocking=false, mode='n'}, meths.get_mode())
    eq('', funcs.getline(1))
  end)
end)

describe('luaintchkfreq option', function()
  it('exists and defaults to 100', function()
    eq(1, funcs.exists('&luaintchkfreq'))
    eq(1, funcs.exists('&licf'))
    eq(1, funcs.exists('+luaintchkfreq'))
    eq(1, funcs.exists('+licf'))
    eq(100, meths.get_option('luaintchkfreq'))
    eq(100, meths.get_option('licf'))
    eq(100, meths.eval('&luaintchkfreq'))
    eq(100, meths.eval('&licf'))
  end)
  it('replaces values greater then INT_MAX with INT_MAX', function()
    local err, msg = pcall(meths.set_option, 'luaintchkfreq', 4294967296)
    eq(false, err)
    msg = msg:gsub('^.*:', '')
    eq(' Value for option "luaintchkfreq" is outside range', msg)
    eq(100, meths.get_option('luaintchkfreq'))
    if pend_w32('sizeof(long/*numeric option*/) == sizeof(int)') then return end
    local err, msg = pcall(meths.command, 'set luaintchkfreq=4294967296')
    eq(true, err)
    eq(2147483647, meths.get_option('luaintchkfreq'))
  end)
  it('replaces values lesser then 0 with 0', function()
    eq(100, meths.get_option('licf'))
    local err, msg = pcall(meths.set_option, 'luaintchkfreq', -1)
    eq(true, err)
    eq(0, meths.eval('&licf'))
  end)
  it('can be set before lua code is first run', function()
    eq(100, meths.get_option('luaintchkfreq'))
    meths.set_option('luaintchkfreq', 0)
    eq(0, meths.get_option('luaintchkfreq'))
  end)
  local gethookstr = [[
    (function(hook, flags, cnt)
      if type(hook) == type(nil) then
        hook = 'nil'
      elseif type(hook) == type(function() end) then
        hook = 'luafunc'
      else
        hook = 'cfunc'
      end
      return {hook, flags, cnt}
    end)(debug.gethook())
  ]]
  it('specifies when lua code is interrupted, first set zero', function()
    meths.set_option('luaintchkfreq', 0)
    eq({'nil', '', 0}, funcs.luaeval(gethookstr))
    meths.set_option('luaintchkfreq', 10)
    eq({'cfunc', '', 10}, funcs.luaeval(gethookstr))
  end)
  it('specifies when lua code is interrupted, first set 10', function()
    meths.set_option('luaintchkfreq', 10)
    eq({'cfunc', '', 10}, funcs.luaeval(gethookstr))
    meths.set_option('luaintchkfreq', 0)
    eq({'nil', '', 0}, funcs.luaeval(gethookstr))
  end)
  it('specifies when lua code is interrupted, start from defaults', function()
    eq({'cfunc', '', 100}, funcs.luaeval(gethookstr))
    meths.set_option('luaintchkfreq', 10)
    eq({'cfunc', '', 10}, funcs.luaeval(gethookstr))
    meths.set_option('luaintchkfreq', 0)
    eq({'nil', '', 0}, funcs.luaeval(gethookstr))
  end)
  it('overrides user hooks', function()
    eq({'cfunc', '', 100}, funcs.luaeval(gethookstr))
    eq({'luafunc', '', 5},
       funcs.luaeval(('({debug.sethook(function() end, "", 5), %s})[2]'):format(
         gethookstr)))
    eq({'cfunc', '', 100}, funcs.luaeval(gethookstr))
  end)
  it('does not override user hooks when luaintchkfreq is zero', function()
    meths.set_option('licf', 0)
    eq({'nil', '', 0}, funcs.luaeval(gethookstr))
    eq({'luafunc', '', 5},
       funcs.luaeval(('({debug.sethook(function() end, "", 5), %s})[2]'):format(
         gethookstr)))
    eq({'luafunc', '', 5}, funcs.luaeval(gethookstr))
  end)
end)
