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
    g={},
    s={},
    user_functions={},
    user_commands={},
    set_script_locals=function(self, s)
      local state = copy_table(self)
      state.s = s
      return state
    end
  }
  state.v = new_v(state),
  return state
end

commands={
  append=function(state, range, bang, lines)
    print (lines)
  end,
  abclear=function(state, buffer)
  end,
}

range={
  compose=function(state, ...)
  end,
  apply_followup=function(state, followup_type, followup_data, lnr)
  end,

  mark=function(state, mark)
  end,
  forward_search=function(state, regex)
  end,
  backward_search=function(state, regex)
  end,
  last=function(state)
  end,
  current=function(state)
  end,
  substitute_search=function(state)
  end,
  forward_previous_search=function(state)
  end,
  backward_previous_search=function(state)
  end,
}

new_scope = function()
  return {}
end

get_state = function()
  return new_state()
end

run_user_command = function(state, command, range, bang, args)
end

iter_start = function(state, list)
end

return {
  eq=eq,
  add=add,
  new_scope=new_scope,
  new_state=new_state,
  get_state=get_state,
  commands=commands,
  range=range,
  run_user_command=run_user_command,
  iter_start=iter_start,
}
