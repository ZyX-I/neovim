local ito, itoe, f
do
  local _obj_0 = require('test.unit.viml.executor.helpers')
  ito = _obj_0.ito
  itoe = _obj_0.itoe
  f = _obj_0.f
end

describe('string() function', function()
  ito('Stringifying dictionaries', [[
    echo string({})
    echo string({'a':1})
    echo string({'a':'ab'})
    echo string({'a':{}})
    echo string({'a':{1: 10}})
  ]], {
    '{}',
    "{'a': 1}",
    "{'a': 'ab'}",
    "{'a': {}}",
    "{'a': {'1': 10}}",
  })

  ito('Stringifying lists', [[
    echo string([])
    echo string([ [] ])
    echo string([ [] , [] ])
    echo string([1, 2])
  ]], {
    '[]',
    '[[]]',
    '[[], []]',
    '[1, 2]',
  })

  ito('Stringifying funcrefs', [[
    function Abc()
    endfunction
    let d = {}
    function d.Abc()
    endfunction
    let d.f = function('Abc')
    echo string(function('Abc'))
    echo string(d.f)
    echo string(d.Abc)
    echo string(function('function'))
    delfunction Abc
    unlet d
  ]], {
    'function(\'Abc\')',
    'function(\'Abc\')',
    'function(\'1\')',
    'function(\'function\')',
  })

  ito('Stringifying strings', [[
    echo string('')
    echo string("'")
    echo string('"')
    echo string('abc')
  ]], {
    [['']],
    [['''']],
    [['"']],
    [['abc']],
  })

  ito('Stringifying numbers', [[
    echo string(0)
    echo string(-0)
    echo string(-1)
    echo string(1)
    echo string(0x20)
  ]], {'0', '0', '-1', '1', '32'})

  ito('Stringifying floats', [[
    echo string(0.0)
    echo string(-0.0)
    echo string(-1.0)
    echo string(-1.0e-15)
    echo string(1.0)
    echo string(1.0e-15)
  ]], {
    '0.000000e+00',
    '-0.000000e+00',
    '-1.000000e+00',
    '-1.000000e-15',
    '1.000000e+00',
    '1.000000e-15',
  })
end)

describe('copy() function', function()
  ito('Copies scalar values', [[
    echo copy(1)
    echo copy(1.0)
    echo copy('')
    echo string(copy(function("copy")))
  ]], {
    1, f(1.0), '', 'function(\'copy\')'
  })
  ito('Copies non-recursive Lists', [[
    let l = [1, 1.0, '', function('copy'), [ [] ], {'d': {}}]
    let l2 = copy(l)
    echo l is# l2
    echo l ==# l2
    echo l[-2] is# l2[-2]
    echo l[-1] is# l2[-1]
    echo l2[0]
    echo l2[1]
    echo l2[2]
    echo l2[4]
    echo string(l2[5])
    unlet l l2
  ]], {
    0, 1, 1, 1,
    1, f(1.0), '', {{_t='list'}}, '{\'d\': {}}',
  })
  ito('Copies non-recursive Dictionaries', [[
    let d = {'n': 1, 'f': 1.0, 's': '', 'F': function('copy'), 'l': [ [] ]}
    let d.d = {'d': {}}
    let d2 = copy(d)
    echo d is# d2
    echo d ==# d2
    echo d.d is# d2.d
    echo d.l is# d2.l
    echo d2.n
    echo d2.f
    echo d2.s
    echo d2.l
    echo string(d2.d)
    unlet d d2
  ]], {
    0, 1, 1, 1,
    1, f(1.0), '', {{_t='list'}}, '{\'d\': {}}',
  })
  ito('Copies recursive Lists', [[
    let l = [0]
    let l[0] = l
    let l2 = copy(l)
    echo l2 is# l
    echo l2[0] is# l[0]
    echo l2[0] is# l
    echo l2[0][0] is# l
    unlet l l2
  ]], {
    0, 1, 1, 1,
  })
  ito('Copies recursive Dictionaries', [[
    let d = {}
    let d.d = d
    let d2 = copy(d)
    echo d2 is# d
    echo d2.d is# d.d
    echo d2.d is# d
    echo d2.d.d is# d
    unlet d d2
  ]], {
    0, 1, 1, 1,
  })
end)

describe('deepcopy() function', function()
  ito('Copies scalar values', [[
    echo deepcopy(1)
    echo deepcopy(1.0)
    echo deepcopy('')
    echo string(deepcopy(function("copy")))
  ]], {
    1, f(1.0), '', 'function(\'copy\')'
  })
  ito('Copies non-recursive Lists', [[
    let l = [1, 1.0, '', function('copy'), [ [] ], {'d': {}}]
    let l2 = deepcopy(l)
    echo l is# l2
    echo l ==# l2
    echo l[-2] is# l2[-2]
    echo l[-1] is# l2[-1]
    echo l2[0]
    echo l2[1]
    echo l2[2]
    echo l2[4]
    echo string(l2[5])
    unlet l l2
  ]], {
    0, 1, 0, 0,
    1, f(1.0), '', {{_t='list'}}, '{\'d\': {}}',
  })
  ito('Copies non-recursive Dictionaries', [[
    let d = {'n': 1, 'f': 1.0, 's': '', 'F': function('copy'), 'l': [ [] ]}
    let d.d = {'d': {}}
    let d2 = deepcopy(d)
    echo d is# d2
    echo d ==# d2
    echo d.d is# d2.d
    echo d.l is# d2.l
    echo d2.n
    echo d2.f
    echo d2.s
    echo d2.l
    echo string(d2.d)
    unlet d d2
  ]], {
    0, 1, 0, 0,
    1, f(1.0), '', {{_t='list'}}, '{\'d\': {}}',
  })
  ito('Copies recursive Lists', [[
    let l = [0]
    let l[0] = l
    let l2 = deepcopy(l)
    echo l2 is# l
    echo l2[0] is# l[0]
    echo l2[0] is# l
    echo l2[0][0] is# l
    echo l2[0] is# l2
    unlet l l2
  ]], {
    0, 0, 0, 0, 1,
  })
  ito('Copies recursive Dictionaries', [[
    let d = {}
    let d.d = d
    let d2 = deepcopy(d)
    echo d2 is# d
    echo d2.d is# d.d
    echo d2.d is# d
    echo d2.d.d is# d
    echo d2.d is# d2
    unlet d d2
  ]], {
    0, 0, 0, 0, 1,
  })
end)
