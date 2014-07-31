local ito, itoe
do
  local _obj_0 = require('test.unit.viml.executor.helpers')
  ito = _obj_0.ito
  itoe = _obj_0.itoe
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
  -- XXX Incompatible behavior
  ito('Allows changing type of the variable', [[
    let a = 1
    echo a
    let a = []
    echo a
    unlet a
  ]], {1, {_t='list'}})
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
  ito('List :let modifying assignments', [[
    let a = 1
    let b = 2
    let l = []
    echo [a, b, string(l)]
    let [a, b] += [3, 5]
    echo [a, b, string(l)]
    let [a; l] += [3, 7]
    echo [a, b, string(l)]
    unlet a b l
  ]], {
    {1, 2, '[]'},
    {4, 7, '[]'},
    {7, 7, '[7]'},
  })
  ito('List :let modifying assignment does not create new variables', [[
    try
      let [a, b] += [1, 2]
    catch
      echo v:exception
    endtry
    let [a, b] += [1, 2]
    try
      echo [a, b]
    catch
      echo v:exception
    endtry
  ]], {
    'Vim(let):E121: Undefined variable: a',
    'Vim(echo):E121: Undefined variable: a',
  })
end)

describe('Slice assignment', function()
  itoe('Fails to assign to non-List', {
    'let [d, s, n, f, F] = [{}, "", 1, 1.0, function("string")]',
    'let d[1:2] = [1, 2]',
    'let s[1:2] = [1, 2]',
    'let n[1:2] = [1, 2]',
    'let f[1:2] = [1, 2]',
    'let F[1:2] = [1, 2]',
    'unlet d s n f F'
  }, {
    'Vim(let):E719: Cannot use [:] with a Dictionary',
    'Vim(let):E689: Can only index a List or Dictionary',
    'Vim(let):E689: Can only index a List or Dictionary',
    'Vim(let):E689: Can only index a List or Dictionary',
    'Vim(let):E689: Can only index a List or Dictionary',
  })
  itoe('Fails to assign non-List to List slice', {
    'let l = [1, 2, 3]',
    'let l[1:2] = 10',
    'let l[1:2] = "ab"',
    'let l[1:2] = 0.0',
    'let l[1:2] = function("string")',
    'let l[1:2] = {}',
    'unlet l',
  }, {
    'Vim(let):E709: [:] requires a List value',
    'Vim(let):E709: [:] requires a List value',
    'Vim(let):E709: [:] requires a List value',
    'Vim(let):E709: [:] requires a List value',
    'Vim(let):E709: [:] requires a List value',
  })
  itoe('Fails to assign lists with different length', {
    'let l = [1, 2, 3]',
    'let l[1:2] = [2]',
    'let l[1:2] = [2, 3, 4]',
    'unlet l'
  }, {
    'Vim(let):E711: List value has not enough items',
    'Vim(let):E710: List value has too many items',
  })
  ito('Performs slice assignment', [[
    let l = [1, 2, 3, 4, 5]
    echo l
    let l[1:2] = ["2", "3"]
    echo l
    let l[-4:2] = ["-4", "-3"]
    echo l
    unlet l
  ]], {
    {1, 2, 3, 4, 5},
    {1, '2', '3', 4, 5},
    {1, '-4', '-3', 4, 5},
  })
end)

describe(':unlet support', function()
  ito('Unlets a variable', [[
    let a = 1
    echo a
    unlet a
    try
      echo a
    catch
      echo v:exception
    endtry
  ]], {1, 'Vim(echo):E121: Undefined variable: a'})
  ito('Fails to unlet unexisting variable', [[
    try
      unlet a
    catch
      echo v:exception
    endtry
  ]], {'Vim(unlet):E108: No such variable: a'})
  ito('Succeeds to unlet unexisting variable with bang', [[
    try
      unlet! a
    catch
      echo v:exception
    endtry
  ]], {})
end)
