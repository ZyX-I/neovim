local ito
do
  local _obj_0 = require('test.unit.viml.executor.helpers')
  ito = _obj_0.ito
end

describe(':let assignments', function()
  ito('Assigns single value to default scope', [[
    let a = 1
    echo a
    unlet a
  ]], {1})
  ito('Assigns single value to g: (default) scope', [[
    let g:a = 1
    echo a
    unlet g:a
  ]], {1})
  ito('Assigns single value to g: scope', [[
    let g:a = 1
    echo g:a
    unlet g:a
  ]], {1})
  ito('Handles list assignments', [[
    let [a, b, c] = [1, 2, 3]
    echo a
    echo b
    echo c
    unlet a b c
  ]], {1, 2, 3})
end)

describe(':let modifying assignments', function()
  ito('Increments single value from default scope', [[
    let a = 1
    echo a
    let a += 1
    echo a
    unlet a
  ]], {1, 2})
  ito('Increments single value from g: scope', [[
    let g:a = 1
    echo g:a
    let g:a += 1
    echo g:a
    unlet g:a
  ]], {1, 2})
  ito('Decrements single value from default scope', [[
    let a = 1
    echo a
    let a -= 1
    echo a
    unlet a
  ]], {1, 0})
  ito('Decrements single value from g: scope', [[
    let g:a = 1
    echo g:a
    let g:a -= 1
    echo g:a
    unlet g:a
  ]], {1, 0})
end)
