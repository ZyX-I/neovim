local ito
do
  local _obj_0 = require('test.unit.viml.executor.helpers')
  ito = _obj_0.ito
end

describe(':let assignments', function()
  ito('Assigns single value to default scope', [[
    let a = 1
    echo a
  ]], {1})
  ito('Assigns single value to g: (default) scope', [[
    let g:a = 1
    echo a
  ]], {1})
  ito('Assigns single value to g: scope', [[
    let g:a = 1
    echo g:a
  ]], {1})
  ito('Handles list assignments', [[
    let [a, b, c] = [1, 2, 3]
    echo a
    echo b
    echo c
  ]], {1, 2, 3})
  ito('Handles rest list assignments', [[
    let [a, b; c] = [1, 2, 3, 4]
    echo a
    echo b
    echo c
    let [a, b; c] = [1, 2]
    echo c
    let [a, b; c] = [1, 2, 3, 4, 5]
    echo c
    let [a, b; c] = [1, 2, 3]
    echo c
    unlet a b c
  ]], {1, 2, {3, 4}, {_t='list'}, {3, 4, 5}, {3}})
end)

describe(':let modifying assignments', function()
  ito('Increments single value from default scope', [[
    let a = 1
    echo a
    let a += 1
    echo a
  ]], {1, 2})
  ito('Increments single value from g: scope', [[
    let g:a = 1
    echo g:a
    let g:a += 1
    echo g:a
  ]], {1, 2})
  ito('Decrements single value from default scope', [[
    let a = 1
    echo a
    let a -= 1
    echo a
  ]], {1, 0})
  ito('Decrements single value from g: scope', [[
    let g:a = 1
    echo g:a
    let g:a -= 1
    echo g:a
  ]], {1, 0})
end)
