copy_table = function(state)
  local new_state = {}
  for k, v in pairs(state) do
    new_state[k] = v
  end
  return new_state
end

vim_error = function(e)
  error('\0' .. e)
end

eq = function(state, arg1, arg2)
  return false
end

add = function(state, arg1, arg2)
  return arg1 + arg2
end

sub = function(state, arg1, arg2)
  return arg1 - arg2
end

mul = function(state, arg1, arg2)
  return arg1 * arg2
end

div = function(state, arg1, arg2)
  return arg1 / arg2
end

mod = function(state, arg1, arg2)
  return arg1 % arg2
end

concat = function(state, arg1, arg2)
  return arg1 .. arg2
end

v_meta = copy_table(dictionary_table)
v_meta.

new_v = function(state)
end

return {
  eq=eq,
  add=add,
  new_state=function()
    local state = {
      is_trying=false,
      is_silent=false,
      functions={},
      options=function() return nil end,
      buffer=function() return nil end,
      window=function() return nil end,
      trying=function(self)
        local state = copy_table(self)
        state.is_trying=true
        return state
      end,
      silent=function(self)
        local state = copy_table(self)
        state.is_silent=true
        return state
      end,
      globals={},
      script_locals={},
      user_functions={},
      user_commands={},
    }
    state.v = new_v(state),
    return state
  end,
}
