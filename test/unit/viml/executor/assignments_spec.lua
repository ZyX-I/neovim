describe(':let assignments', function()
  local ito, itoe
  do
    local _obj_0 = require('test.unit.viml.executor.helpers')(it)
    ito = _obj_0.ito
    itoe = _obj_0.itoe
  end
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
  itoe('Fails to do list assignments for lists of different length', {
    'let [a, b] = [1, 2, 3]',
    'let [a, b] = []',
    'let [a, b] = [1]',
    'let [a; b] = []',
    'echo a',
    'echo b',
  }, {
    'Vim(let):E688: More targets than List items',
    'Vim(let):E687: Less targets than List items',
    'Vim(let):E687: Less targets than List items',
    'Vim(let):E687: Less targets than List items',
    'Vim(echo):E121: Undefined variable: a',
    'Vim(echo):E121: Undefined variable: b',
  })
end)

describe(':let modifying assignments', function()
  local ito, itoe
  do
    local _obj_0 = require('test.unit.viml.executor.helpers')(it)
    ito = _obj_0.ito
    itoe = _obj_0.itoe
  end
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

describe(':unlet support', function()
  local ito, itoe
  do
    local _obj_0 = require('test.unit.viml.executor.helpers')(it)
    ito = _obj_0.ito
    itoe = _obj_0.itoe
  end
  ito('Unlets a variable', [[
    let a = 1
    echo a
    unlet a
    try
      echo a
    catch
      echo v:exception
    endtry
  ]], {1, 'E121: Undefined variable: a'})
  ito('Fails to unlet unexisting variable', [[
    try
      unlet a
    catch
      echo v:exception
    endtry
  ]], {'E108: No such variable: a'})
end)
