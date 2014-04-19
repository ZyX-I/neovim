#!/bin/sh

VIML="$1"

echo "$VIML" | $(dirname "$0")/translator > test.lua

cat > script.lua << EOF
vim = require 'vim'
test = require 'test'
state = vim.new_state()
test.run(state)
EOF

export NEW_LUA_PATH="$(dirname "$0")/../src/translator/translator/?.lua"

export LUA_INIT="$LUA_INIT
package.path = os.getenv('NEW_LUA_PATH') .. ';' .. package.path
"

lua script.lua
