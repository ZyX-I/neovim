local ito, itoe
do
  local _obj_0 = require('test.unit.viml.executor.helpers')
  ito = _obj_0.ito
  itoe = _obj_0.itoe
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
