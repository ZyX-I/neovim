local ito
do
  local _obj_0 = require('test.unit.viml.executor.helpers')
  ito = _obj_0.ito
end

describe(':if block', function()
  ito('Runs if 1', [[
    echo 1
    if 1
      echo 2
    end
    echo 3
  ]], {1, 2, 3})
  ito('Does not run if 0', [[
    echo 1
    if 0
      echo 2
    end
    echo 3
  ]], {1, 3})
  ito('Runs if 0 ... else', [[
    echo 1
    if 0
      echo 2
    else
      echo 3
    end
    echo 4
  ]], {1, 3, 4})
  ito('Runs if 0 ... elseif 1', [[
    echo 1
    if 0
      echo 2
    elseif 1
      echo 3
    end
    echo 4
  ]], {1, 3, 4})
  ito('Runs if 0 ... elseif 1, but not else', [[
    echo 1
    if 0
      echo 2
    elseif 1
      echo 3
    else
      echo 4
    end
    echo 5
  ]], {1, 3, 5})
  ito('Runs if 0 ... second elseif 1', [[
    echo 1
    if 0
      echo 2
    elseif 0
      echo 3
    elseif 1
      echo 4
    end
    echo 5
  ]], {1, 4, 5})
  ito('Runs if 0 ... second elseif 1, but not else', [[
    echo 1
    if 0
      echo 2
    elseif 0
      echo 3
    elseif 1
      echo 4
    else
      echo 5
    end
    echo 6
  ]], {1, 4, 6})
  ito('Handles empty :if 0', [[
    echo 1
    if 0
    end
    echo 2
  ]], {1, 2})
  ito('Handles empty :if 1', [[
    echo 1
    if 1
    end
    echo 2
  ]], {1, 2})
  ito('Handles empty :if 0 ... else', [[
    echo 1
    if 0
    else
    end
    echo 2
  ]], {1, 2})
  ito('Handles empty :if 1 ... else', [[
    echo 1
    if 1
    else
    end
    echo 2
  ]], {1, 2})
  ito('Handles empty :if 0 ... elseif 1 ... else', [[
    echo 1
    if 0
    elseif 1
    else
    end
    echo 2
  ]], {1, 2})
  ito('Handles empty :if 0 ... elseif 1', [[
    echo 1
    if 0
    elseif 1
    end
    echo 2
  ]], {1, 2})
end)

describe(':try block', function()
  ito('Handles empty :try', [[
    echo 1
    try
    endtry
    echo 2
  ]], {1, 2})
  ito('Handles empty :try .. catch', [[
    echo 1
    try
    catch
    endtry
    echo 2
  ]], {1, 2})
  ito('Handles empty :try .. finally', [[
    echo 1
    try
    finally
    endtry
    echo 2
  ]], {1, 2})
  ito('Handles empty :try .. catch .. finally', [[
    echo 1
    try
    catch
    finally
    endtry
    echo 2
  ]], {1, 2})
  ito('Handles empty :try .. catch .. catch .. finally', [[
    echo 1
    try
    catch
    catch
    finally
    endtry
    echo 2
  ]], {1, 2})
end)
