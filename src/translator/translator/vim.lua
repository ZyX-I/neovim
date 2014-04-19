copy_table = function(state)
  local new_state = {}
  for k, v in pairs(state) do
    new_state[k] = v
  end
  return new_state
end

v_meta = copy_table(dictionary_table)

new_state=function()
  local state = {
    is_trying=false,
    is_silent=false,
    functions={},
    options={},
    buffer={'b': {}, 'options': {}},
    window={'w': {}, 'options': {}},
    tabpage={'t': {}},
    registers={},
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
    v={},
    a={},
    l={},
    user_functions={},
    user_commands={},
    set_script_locals=function(self, s)
      local state = copy_table(self)
      state.s = s
      return state
    end,
  }
  state.current_scope = state.g
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

subscript = function(state, value, index)
end

add = function(state, ...)
end

subtract = function(state, ...)
end

divide = function(state, ...)
end

multiply = function(state, ...)
end

modulo = function(state, ...)
end

concat = function(state, ...)
end

concat_or_subscript = function(state, dict, key)
end

call = function(state, caller, ...)
end

negate = function(state, value)
end

negate_logical = function(state, value)
end

promote_integer = function(state, value)
end

slice = function(state, value, start_index, end_index)
end

list = function(state, ...)
end

equals = function(state, ic, arg1, arg2)
end

identical = function(state, ic, arg1, arg2)
end

matches = function(state, ic, arg1, arg2)
end

greater = function(state, ic, arg1, arg2)
end

less = function(state, ic, arg1, arg2)
end

number = function(n)
end

float = function(f)
end

err = {
  matches=function(state, err, regex)
  end,
  propagate=function(state, err)
  end,
  throw=function(state, message)
  end,
}

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
  subscript=subscript,
  slice=slice,
  list=list,
  add=add,
  subtract=subtract,
  divide=divide,
  multiply=multiply,
  modulo=modulo,
  call=call,
  negate=negate,
  negate_logical=negate_logical,
  promote_integer=promote_integer,
  concat=concat,
  concat_or_subscript=concat_or_subscript,
  equals=equals,
  identical=identical,
  matches=matches,
  greater=greater,
  less=less,
  number=number,
  float=float,
  err=err,
}
