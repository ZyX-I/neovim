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
    stack={},
  }
  state.current_scope = state.g
  return state
end

err = {
  matches=function(state, err, regex)
  end,
  propagate=function(state, err)
  end,
  err=function(state, position, vim_error, message, ...)
    -- TODO show context
    error((message):format(...), 1)
  end,
  throw=function(...)
  end,
}

commands={
  append=function(state, range, bang, lines)
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

parse = function(state, command, range, bang, args)
  return args, nil
end

run_user_command = function(state, command, range, bang, args)
  command = state.user_commands[command]
  if (command == nil) then
    -- TODO Record line number
    err.err(state, position, true, 'E492: Not a editor command: %s', command)
    return nil
  end
  args, after = parse(state, command, range, bang, args)
  command.func(state, range, bang, args)
  if (after) then
    after(state)
  end
end

list = {
  insert=table.insert,
  length=table.maxn,
  new=function(state, ...)
    return {...}
  end,
}
dict = {
  new=function(state, ...)
    -- TODO
  end,
}

iter_start = function(state, lst)
  ret = {
    i=0,
    lst=lst,
  }
  list.insert(lst.iterators, ret)
  return ret
end

VIM_NUMBER     = 0
VIM_STRING     = 1
VIM_FUNCREF    = 2
VIM_LIST       = 3
VIM_DICTIONARY = 4
VIM_FLOAT      = 5

vim_type = function(state, value, position)
  t = type(value)
  if (t == 'string') then
    return VIM_STRING
  else if (t == 'number') then
    return VIM_NUMBER
  else if (t == 'table') then
    meta = getmetatable(value)
    if (meta == funcref_meta) then
      return VIM_FUNCREF
    else if (meta == list_meta) then
      return VIM_LIST
    else if (meta == dictionary_meta) then
      return VIM_DICTIONARY
    else if (meta == float_meta) then
      return VIM_FLOAT
    else
      err.err(state, position, true,
              'Internal error: table with unknown metatable')
    end
  else
    err.err(state, position, true, 'Internal error: unknown type')
  end
end

get_number = function(state, value, position)
  t = vim_type(state, value, position)
  if (t == VIM_FLOAT) then
    return value:number()
  else if (t == VIM_NUMBER) then
    return value
  else if (t == VIM_STRING) then
    return tonumber(value)
  else if (t == VIM_DICTIONARY) then
    err.err(state, position, true, 'E728: Using Dictionary as a Number')
    return nil
  else if (t == VIM_LIST) then
    err.err(state, position, true, 'E745: Using List as a Number')
    return nil
  else if (t == VIM_FUNCREF) then
    err.err(state, position, true, 'E703: Using Funcref as a Number')
    return nil
  end
end

get_string = function(state, value, position)
  t = vim_type(state, value, position)
  if (t == VIM_FLOAT) then
    err.err(state, position, true, 'E806: Using Float as a String')
    return nil
  else if (t == VIM_NUMBER) then
    return value .. ''
  else if (t == VIM_STRING) then
    return value
  else if (t == VIM_DICTIONARY) then
    err.err(state, position, true, 'E731: Using Dictionary as a String')
    return nil
  else if (t == VIM_LIST) then
    err.err(state, position, true, 'E730: Using List as a String')
    return nil
  else if (t == VIM_FUNCREF) then
    err.err(state, position, true, 'E729: Using Funcref as a String')
    return nil
  end
end

subscript = function(state, value, index)
  t = vim_type(state, value, value_position)
  if (t == nil) then
    return nil
  end
  if (t == VIM_FLOAT) then
    err.err(state, value_position, true, 'E806: Using Float as a String')
    return nil
  end
  if (t == VIM_NUMBER) then
    value = value .. ''
    t = VIM_STRING
  end
  if (t == VIM_STRING) then
    index = get_number(state, index, index_position)
    if (index == nil) then
      return nil
    else if (index < 0) then
      return ''
    else
      return value:sub(index, index)
    end
  else if (t == VIM_LIST) then
    index = get_number(state, index, index_position)
    if (index == nil) then
      return nil
    end
    length = list.length(value)
    if (index < 0) then
      index = length + index
    end
    if (index >= length) then
      err.err(state, value_position, true,
              'E684: list index out of range: ' .. index)
      return nil
    end
    return value[index + 1]
  else if (t == VIM_DICT) then
    index = get_string(state, index, index_position)
    if (index == nil) then
      return nil
    else
      ret = value[index]
      if (ret == nil) then
        err.err(state, value_position, true,
                'E716: Key not present in Dictionary: ' .. index)
        return nil
      end
      return ret
    end
  else if (t == VIM_FUNCREF) then
    err.err(state, value_position, true, 'E695: Cannot index a Funcref')
    return nil
  else if (t == VIM_FLOAT) then
    err.err(state, value_position, true, 'E806: Using Float as a String')
    return nil
  end
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
