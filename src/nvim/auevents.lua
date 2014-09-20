return {
  'BufAdd',                 -- after adding a buffer to the buffer list
  'BufNew',                 -- after creating any buffer
  'BufDelete',              -- deleting a buffer from the buffer list
  'BufWipeout',             -- just before really deleting a buffer
  'BufEnter',               -- after entering a buffer
  'BufFilePost',            -- after renaming a buffer
  'BufFilePre',             -- before renaming a buffer
  'BufLeave',               -- before leaving a buffer
  'BufNewFile',             -- when creating a buffer for a new file
  'BufReadPost',            -- after reading a buffer
  'BufReadPre',             -- before reading a buffer
  'BufReadCmd',             -- read buffer using command
  'BufUnload',              -- just before unloading a buffer
  'BufHidden',              -- just after buffer becomes hidden
  'BufWinEnter',            -- after showing a buffer in a window
  'BufWinLeave',            -- just after buffer removed from window
  'BufWritePost',           -- after writing a buffer
  'BufWritePre',            -- before writing a buffer
  'BufWriteCmd',            -- write buffer using command
  'CmdWinEnter',            -- after entering the cmdline window
  'CmdWinLeave',            -- before leaving the cmdline window
  'ColorScheme',            -- after loading a colorscheme
  'CompleteDone',           -- after finishing insert complete
  'FileAppendPost',         -- after appending to a file
  'FileAppendPre',          -- before appending to a file
  'FileAppendCmd',          -- append to a file using command
  'FileChangedShell',       -- after shell command that changed file
  'FileChangedShellpost',   -- after (not) reloading changed file
  'FileChangedRo',          -- before first change to read-only file
  'FileReadPost',           -- after reading a file
  'FileReadPre',            -- before reading a file
  'FileReadCmd',            -- read from a file using command
  'FileType',               -- new file type detected (user defined)
  'FileWritePost',          -- after writing a file
  'FileWritePre',           -- before writing a file
  'FileWriteCmd',           -- write to a file using command
  'FilterReadPost',         -- after reading from a filter
  'FilterReadPre',          -- before reading from a filter
  'FilterWritePost',        -- after writing to a filter
  'FilterWritePre',         -- before writing to a filter
  'FocusGained',            -- got the focus
  'FocusLost',              -- lost the focus to another app
  'GUIEnter',               -- after starting the GUI
  'GUIFailed',              -- after starting the GUI failed
  'InsertChange',           -- when changing Insert/Replace mode
  'InsertEnter',            -- when entering Insert mode
  'InsertLeave',            -- when leaving Insert mode
  'JobActivity',            -- when job sent some data
  'MenuPopup',              -- just before popup menu is displayed
  'QuickFixCmdPost',        -- after :make, :grep etc.
  'QuickFixCmdPre',         -- before :make, :grep etc.
  'QuitPre',                -- before :quit
  'SessionLoadPost',        -- after loading a session file
  'StdinReadPost',          -- after reading from stdin
  'StdinReadPre',           -- before reading from stdin
  'Syntax',                 -- syntax selected
  'TermChanged',            -- after changing 'term'
  'TermResponse',           -- after setting "v:termresponse"
  'User',                   -- user defined autocommand
  'VimEnter',               -- after starting Vim
  'VimLeave',               -- before exiting Vim
  'VimLeavepre',            -- before exiting Vim and writing .viminfo
  'VimResized',             -- after Vim window was resized
  'WinEnter',               -- after entering a window
  'WinLeave',               -- before leaving a window
  'EncodingChanged',        -- after changing the 'encoding' option
  'InsertCharPre',          -- before inserting a char
  'CursorHold',             -- cursor in same position for a while
  'CursorHoldI',            -- idem, in Insert mode
  'FuncUndefined',          -- if calling a function which doesn't exist
  'RemoteReply',            -- upon string reception from a remote vim
  'SwapExists',             -- found existing swap file
  'SourcePre',              -- before sourcing a Vim script
  'SourceCmd',              -- sourcing a Vim script using command
  'SpellFileMissing',       -- spell file missing
  'CursorMoved',            -- cursor was moved
  'CursorMovedI',           -- cursor was moved in Insert mode
  'TabLeave',               -- before leaving a tab page
  'TabEnter',               -- after entering a tab page
  'ShellCmdPost',           -- after ":!cmd"
  'ShellFilterPost',        -- after ":1,2!cmd", ":w !cmd", ":r !cmd".
  'TextChanged',            -- text was modified
  'TextChangedI',           -- text was modified in Insert mode
}
