-- {{{1 State manipulations
copy_table = function(state)
  local new_state = {}
  for k, v in pairs(state) do
    new_state[k] = v
  end
  return new_state
end

new_scope = function(is_default_scope)
  ret = dict.new(state, ':is_default_scope', is_default_scope)
  ret[type_idx] = VIM_SCOPE
  return ret
end

new_state = function()
  local state = {
    is_trying=false,
    is_silent=false,
    functions={
      [type_idx]=VIM_FUNCTIONS,
      type=function(state, self, ...)
        return vim_type(state, ...)[1]
      end,
    },
    options={},
    buffer={b=new_scope(false), options={}},
    window={w=new_scope(false), options={}},
    tabpage={t=new_scope(false)},
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
    redir_callbacks={},
    v=new_scope(false),
    a=nil,
    l=nil,
    user_functions={[type_idx]=VIM_FUNCTIONS},
    user_commands={},
    call_stack={},
    set_script_locals=function(self, s)
      local state = copy_table(self)
      state.s = s
      return state
    end,
    enter_function=function(self, fcall)
      local state = copy_table(self)
      state.a = new_scope(false)
      state.l = new_scope(true)
      state.current_scope = state.l
      state.call_stack = copy_table(call_stack)
      table.insert(state.call_stack, fcall)
      return state
    end,
    stack={},
    get_local_option=function(self, other, name)
      return other.options[name] or self.options[name]
    end,
  }
  state.g = new_scope(true)
  state.current_scope = state.g
  return state
end

get_state = function()
  return new_state()
end

-- {{{1 Types
-- Special values that cannot be assigned from VimL
type_idx = true
locks_idx = false
val_idx = 0

number = {
  new=function(state, n)
    return n
  end,
  to_string_echo=function(state, n, _)
    return tostring(n)
  end,
}

string = {
  new=function(state, s)
    return s
  end,
  to_string_echo=function(state, s, _)
    return s
  end,
}

list = {
  new=function(state, ...)
    local ret = {...}
    ret[type_idx] = VIM_LIST
    ret.iterators = {}
    ret.locked = false
    ret.fixed = false
    return ret
  end,
  insert=function(state, lst, position, value, value_position)
    if (lst.fixed) then
      -- TODO error out
      return nil
    elseif (lst.locked) then
      -- TODO error out
      return nil
    end
    table.insert(self, value)
    return lst
  end,
  raw_length=table.maxn,
  length=function(state, lst, position)
    return list.raw_length(lst)
  end,
  raw_subscript=function(lst, index)
    return lst[index + 1]
  end,
  subscript=function(state, lst, lst_position, index, index_position)
    local length = list.raw_length(lst)
    if (index < 0) then
      index = length + index
    end
    if (index >= length) then
      err.err(state, lst_position, true,
              'E684: list index out of range: %i', index)
      return nil
    end
    return list.raw_subscript(lst, index)
  end,
  raw_assign_subscript = function(lst, index, val)
    lst[index + 1] = val
  end,
  slice=function(state, lst, index1, index2)
    local i
    ret = list.new(state)
    for i = index1,index2 do
      list.raw_assign_subscript(ret, i - index1, list.raw_subscript(lst, i))
    end
    return ret
  end,
  slice_to_end=function(state, lst, index1)
    return list.slice(state, lst, index1, list.raw_length(lst))
  end,
  assign_subscript=function(state, lst, lst_position, index, index_position,
                                   val, val_position)
    -- TODO check for locks, check for out of range
    raw_assign_subscript(lst, index, val)
    return true
  end,
  next=function(it_state, _)
    local i = it_state.i
    if i >= it_state.maxi then
      return nil
    end
    it_state.i = it_state.i + 1
    return i, list.raw_subscript(it_state.lst, i)
  end,
  iterator=function(state, lst)
    local it_state = {
      i=0,
      maxi=list.raw_length(lst),
      lst=lst,
    }
    table.insert(lst.iterators, it_state)
    return list.next, it_state, lst
  end,
  to_string_echo=function(state, lst, refs)
    local ret = '['
    local length = list.raw_length(lst)
    local i
    for i = 0,length-1 do
      chunk = string_echo(state, list.raw_subscript(lst, i), nil, refs)
      if chunk == nil then
        return nil
      end
      ret = ret .. chunk
      if i ~= length-1 then
        ret = ret .. ', '
      end
    end
    ret = ret .. ']'
    return ret
  end,
}

dict = {
  new=function(state, ...)
    local i = 1
    local key
    local val
    local ret
    local t
    local max
    t = {...}
    ret = {}
    ret[type_idx] = VIM_DICTIONARY
    max = table.maxn(t)
    if max % 2 == 1 then
      err.err(state, position, true,
              'Internal error: odd number of arguments to dict.new')
      return nil
    end
    while i < max / 2 do
      key = get_string(state, t[i*2], nil)
      val = t[i*2 + 1]
      if key == nil or val == nil then
        return nil
      end
      ret[key] = val
      i = i + 1
    end
    return ret
  end,
  subscript=function(state, dct, dct_position, key, key_position)
    ret = dct[key]
    if (ret == nil) then
      local message
      if dct[type_idx] == VIM_DICTIONARY then
        message = 'E716: Key not present in Dictionary'
      elseif dct[type_idx] == VIM_SCOPE then
        message = 'E121: Undefined variable'
      elseif dct[type_idx] == VIM_FUNCTIONS then
        message = 'E117: Unknown function'
      end
      err.err(state, value_position, true, message .. ': %s', key)
    end
    return ret
  end,
  assign_subscript=function(state, dct, dct_position, key, key_position,
                                   val, val_position)
    -- TODO check for locks
    dct[key] = val
    return true
  end,
}

float = {
  new=function(state, f)
    return {[type_idx]=VIM_LIST, [val_idx]=f}
  end,
  to_string_echo=function(state, n, _)
    return tostring(n[f])
  end,
}

VIM_NUMBER     = {0}
VIM_STRING     = {1}
VIM_FUNCREF    = {2}
VIM_LIST       = {3}
VIM_DICTIONARY = {4}
VIM_FLOAT      = {5}

VIM_SCOPE      = {4}
VIM_FUNCTIONS  = {4}

types = {
  [VIM_NUMBER]=number,
  [VIM_STRING]=string,
  [VIM_FUNCREF]=funcref,
  [VIM_LIST]=list,
  [VIM_DICTIONARY]=dict,
  [VIM_FLOAT]=float,
}

pseudo_types = {
  [VIM_SCOPE]=VIM_DICTIONARY,
  [VIM_FUNCTIONS]=VIM_DICTIONARY,
}

-- {{{1 Error manipulation
err = {
  matches=function(state, err, regex)
    if (err:match(regex)) then
      return true
    else
      return false
    end
  end,
  propagate=function(state, err)
  end,
  err=function(state, position, vim_error, message, ...)
    local formatted_message
    formatted_message = (message):format(...)
    -- TODO show context
    if state.is_trying then
      error(formatted_message, 0)
    else
      io.stderr:write(formatted_message .. '\n')
    end
  end,
  throw=function(...)
  end,
}

non_nil = function(wrapped)
  return function(state, ...)
    local i
    local maxi = select('#', ...)
    local t = {...}
    if maxi ~= 0 then
      for i = 1,maxi do
        if t[i] == nil then
          return nil
        end
      end
    end
    return wrapped(state, ...)
  end
end

-- {{{1 Command implementations
parse = function(state, command, range, bang, args)
  return args, nil
end

string_echo = function(state, v, v_position, refs)
  t = vim_type(state, v, v_position)
  if t == nil then
    return nil
  end
  if refs ~= nil then
    -- TODO
  end
  return types[t].to_string_echo(state, v, refs)
end

commands={
  append=function(state, range, bang, lines)
  end,
  abclear=function(state, buffer)
  end,
  echo=non_nil(function(state, ...)
    mes = ''
    for i, s in ipairs({...}) do
      if (i > 1) then
        mes = mes .. ' '
      end
      chunk = string_echo(state, s, s_position, {})
      if chunk == nil then
        return nil
      end
      mes = mes .. chunk
    end
    print (mes)
  end),
}

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

-- {{{1 Assign support
assign_dict = non_nil(function(state, val, dct, key)
  if (key == '') then
    err.err(state, key_position, true,
            'E713: Cannot use empty key for dictionary')
    return nil
  end
  return dict.assign_subscript(state, dct, dct_position, key, key_position,
                                      val, val_position)
end)

assign_dict_function = non_nil(function(state, unique, val, dct, key)
  if (unique and dct[key] ~= nil) then
    err.err(state, key_position, true, 'E717: Dictionary entry already exists')
    return nil
  end
  return assign_dict(state, val, dct, key)
end)

assign_scope = assign_dict

assign_scope_function = non_nil(function(state, unique, val, scope, key)
  if (unique and scope[key] ~= nil) then
    err.err(state, key_position, true,
            'E122: Function %s already exists, add ! to replace it', key)
    return nil
  end
  return assign_scope(state, val, scope, key)
end)

assign_subscript = non_nil(function(state, val, dct, key)
  t = vim_type(dct)
  if (t == VIM_DICTIONARY) then
    return assign_dict(state, val, dct, key)
  elseif (t == VIM_LIST) then
    return list.assign_subscript(state, dct, dct_position, key, key_position,
                                             val, val_position)
  else
    err.err(state, dct_position, true,
            'E689: Can only index a List or Dictionary')
    return nil
  end
end)

assign_subscript_function = non_nil(function(state, unique, val, dct, key)
  t = vim_type(dct)
  if (t == VIM_DICTIONARY) then
    return assign_dict_function(state, unique, val, dct, key)
  end
  return assign_subscript(state, val, dct, key)
end)

assign_slice = non_nil(function(state, val, lst, index1, index2)
  t = vim_type(lst)
  if (t == VIM_LIST) then
    -- TODO
  elseif (t == VIM_DICTIONARY) then
    err.err(state, lst_position, true, 'E719: Cannot use [:] with a Dictionary')
    return nil
  else
    err.err(state, lst_position, true,
            'E689: Can only index a List or Dictionary')
    return nil
  end
end)

assign_slice_function = non_nil(function(state, unique, val, lst, index1,
                                         index2)
  t = vim_type(lst)
  if (t == VIM_LIST) then
    err.err(state, lst_position, true,
            'E475: Cannot assign function to a slice')
    return nil
  else
    return assign_slice(state, val, lst, index1, index2)
  end
end)

-- {{{1 Range handling
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

-- {{{1 Type manipulations
vim_type = function(state, value, position)
  if (value == nil) then
    return nil
  end
  t = type(value)
  if (t == 'string') then
    return VIM_STRING
  elseif (t == 'number') then
    return VIM_NUMBER
  elseif (t == 'table') then
    local typ = value[type_idx]
    local ret = pseudo_types[typ] or (types[typ] and typ)
    if ret == nil then
      err.err(state, position, true, 'Internal error: table with unknown type')
    else
      return nil
    end
  else
    err.err(state, position, true, 'Internal error: unknown type')
  end
  return nil
end

get_number = function(state, value, position)
  t = vim_type(state, value, position)
  if (t == nil) then
    return nil
  elseif (t == VIM_FLOAT) then
    return value:number()
  elseif (t == VIM_NUMBER) then
    return value
  elseif (t == VIM_STRING) then
    -- TODO properly convert "0733" style (octal) strings
    -- TODO properly convert "{number}garbage" strings
    return tonumber(value) or 0
  elseif (t == VIM_DICTIONARY) then
    err.err(state, position, true, 'E728: Using Dictionary as a Number')
    return nil
  elseif (t == VIM_LIST) then
    err.err(state, position, true, 'E745: Using List as a Number')
    return nil
  elseif (t == VIM_FUNCREF) then
    err.err(state, position, true, 'E703: Using Funcref as a Number')
    return nil
  end
end

get_string = function(state, value, position)
  t = vim_type(state, value, position)
  if (t == nil) then
    return nil
  elseif (t == VIM_FLOAT) then
    err.err(state, position, true, 'E806: Using Float as a String')
    return nil
  elseif (t == VIM_NUMBER) then
    return value .. ''
  elseif (t == VIM_STRING) then
    return value
  elseif (t == VIM_DICTIONARY) then
    err.err(state, position, true, 'E731: Using Dictionary as a String')
    return nil
  elseif (t == VIM_LIST) then
    err.err(state, position, true, 'E730: Using List as a String')
    return nil
  elseif (t == VIM_FUNCREF) then
    err.err(state, position, true, 'E729: Using Funcref as a String')
    return nil
  end
end

get_boolean = function(state, value, position)
  num = get_number(state, value, position)
  if num == nil then
    return nil
  end
  return num == 0
end

-- {{{1 Basic operations
iterop = non_nil(function(state, converter, opfunc, ...)
  local result
  local i, v
  for i, v in ipairs({...}) do
    local curarg = converter(state, v, position)
    if (curarg == nil) then
      return nil
    end
    if (i == 1) then
      result = curarg
    else
      result = opfunc(state, result, curarg, curarg_position)
      if (result == nil) then
        return nil
      end
    end
  end
  if i == nil or i == 1 then
    return nil
  end
  return result
end)

add = function(state, ...)
  return iterop(state, get_number, function(state, result, a2, a2_position)
    return result + a2
  end, ...)
end

subtract = function(state, ...)
  return iterop(state, get_number, function(state, result, a2, a2_position)
    return result - a2
  end, ...)
end

multiply = function(state, ...)
  return iterop(state, get_number, function(state, result, a2, a2_position)
    return result * a2
  end, ...)
end

divide = function(state, ...)
  return iterop(state, get_number, function(state, result, a2, a2_position)
    return result / a2
  end, ...)
end

modulo = function(state, ...)
  return iterop(state, get_number, function(state, result, a2, a2_position)
    return result % a2
  end, ...)
end

concat = function(state, ...)
  return iterop(state, get_string, function(state, result, a2, a2_position)
    return result .. a2
  end, ...)
end

negate = function(state, value)
  value = get_number(state, value, position)
  if (value == nil) then
    return nil
  else
    return number.new(state, -value)
  end
end

vim_true = 1
vim_false = 0

negate_logical = non_nil(function(state, value)
  value = get_number(state, value, position)
  if (value == nil) then
    return nil
  elseif (value == 0) then
    return vim_true
  else
    return vim_false
  end
end)

promote_integer = function(state, value)
  return get_number(state, value, position)
end

same_complex_types = function(state, t1, t2, arg2_position)
  if (t1 == nil or t2 == nil) then
    return nil
  elseif (t1 == VIM_LIST) then
    if (t2 == VIM_LIST) then
      return 1
    end
    err.err(state, arg2_position, true,
            'E691: Can only compare List with List')
    return nil
  elseif (t1 == VIM_DICTIONARY) then
    if (t2 == VIM_DICTIONARY) then
      return 1
    end
    err.err(state, arg2_position, true,
            'E735: Can only compare Dictionary with Dictionary')
    return nil
  elseif (t1 == VIM_FUNCREF) then
    if (t2 == VIM_FUNCREF) then
      return 1
    end
    err.err(state, arg2_position, true,
            'E693: Can only compare Funcref with Funcref')
    return nil
  end
  return 2
end

equals = function(state, ic, arg1, arg2)
  t1 = vim_type(state, arg1, arg1_position)
  t2 = vim_type(state, arg2, arg2_position)
  if (same_complex_types(state, t1, t2, arg2_position) == nil) then
    return nil
  end
  if (t1 == VIM_FLOAT or t2 == VIM_FLOAT) then
    arg1 = get_float(state, arg1, arg1_position)
    arg2 = get_float(state, arg2, arg2_position)
    if (arg1 == nil or arg2 == nil) then
      return nil
    end
    return (arg1 == arg2) and vim_true or vim_false
  elseif (t1 == VIM_NUMBER or t2 == VIM_NUMBER) then
    arg1 = get_number(state, arg1, arg1_position)
    arg2 = get_number(state, arg2, arg2_position)
    if (arg1 == nil or arg2 == nil) then
      return nil
    end
    return (arg1 == arg2) and vim_true or vim_false
  elseif (t1 == VIM_STRING) then
    arg2 = get_string(state, arg2, arg2_position)
    if (arg2 == nil) then
      return nil
    end
    if (not ic) then
      return (arg1 == arg2) and vim_true or vim_false
    else
      -- TODO
    end
  elseif (t1 == VIM_LIST) then
    for i, v1 in list.iterator(state, arg1) do
      v2 = list.raw_subscript(arg2, i)
      if (v2 == nil) then
        return vim_false
      elseif (equals(state, ic, v1, v2) == vim_false) then
        return vim_false
      end
    end
    return vim_true
  elseif (t2 == VIM_DICTIONARY) then
    -- TODO
  elseif (t1 == VIM_FUNCREF) then
    -- TODO
  end
end

identical = non_nil(function(state, ic, arg1, arg2)
  t1 = vim_type(state, arg1, arg1_position)
  t2 = vim_type(state, arg2, arg2_position)
  if (t1 ~= t2) then
    return vim_false
  elseif (t1 == VIM_DICTIONARY or t1 == VIM_LIST) then
    return (arg1 == arg2) and vim_true or vim_false
  else
    return equals(state, ic, arg1, arg2)
  end
end)

matches = non_nil(function(state, ic, arg1, arg2)
  -- TODO
end)

less_greater_cmp = function(state, ic, cmp, string_icmp, arg1, arg1_position,
                                                         arg2, arg2_position)
  t1 = vim_type(state, arg1, arg1_position)
  t2 = vim_type(state, arg2, arg2_position)
  if (same_complex_types(state, t1, t2, arg2_position) == nil) then
    return nil
  end
  if (t1 == VIM_FLOAT or t2 == VIM_FLOAT) then
    arg1 = get_float(state, arg1, arg1_position)
    arg2 = get_float(state, arg2, arg2_position)
    if (arg1 == nil or arg2 == nil) then
      return nil
    end
    return cmp(arg1, arg2) and vim_true or vim_false
  elseif (t1 == VIM_NUMBER or t2 == VIM_NUMBER) then
    arg1 = get_number(state, arg1, arg1_position)
    arg2 = get_number(state, arg2, arg2_position)
    if (arg1 == nil or arg2 == nil) then
      return nil
    end
    return cmp(arg1, arg2) and vim_true or vim_false
  elseif (t1 == VIM_STRING) then
    -- Second argument is now string for sure: if it was number or float it 
    -- would fall under previous two conditions, if it was string vs something 
    -- else it would be caught by same_complex_types check
    if (ic) then
      return string_icmp(arg1, arg2) and vim_true or vim_false
    else
      return cmp(arg1, arg2) and vim_true or vim_false
    end
  elseif (t1 == VIM_LIST) then
    err.err(state, t1 == VIM_LIST and arg1_position or arg2_position,
            true, 'E692: Invalid operation for Lists')
    return nil
  elseif (t1 == VIM_DICTIONARY) then
    err.err(state, t1 == VIM_DICTIONARY and arg1_position or arg2_position,
            true, 'E736: Invalid operation for Dictionaries')
    return nil
  elseif (t1 == VIM_FUNCREF) then
    err.err(state, t1 == VIM_FUNCREF and arg1_position or arg2_position,
            true, 'E694: Invalid operation for Funcrefs')
    return nil
  end
end

less = function(state, ic, arg1, arg2)
  return less_greater_cmp(state, ic,
                          function(arg1, arg2)
                            return arg1 < arg2
                          end,
                          function(arg1, arg2)
                            -- TODO case-ignorant string comparison
                          end,
                          arg1, arg1_position,
                          arg2, arg2_position)
end

greater = function(state, ic, arg1, arg2)
  return less_greater_cmp(state, ic,
                          function(arg1, arg2)
                            return arg1 > arg2
                          end,
                          function(arg1, arg2)
                            -- TODO case-ignorant string comparison
                          end,
                          arg1, arg1_position,
                          arg2, arg2_position)
end

-- {{{1 Subscripting
func_subscript = non_nil(function(state, value, index)
  local t, f, ft
  t = vim_type(state, value, value_position)
  f = subscript(state, value, index)
  ft = vim_type(state, f)
  if ft ~= VIM_FUNCREF then
    -- TODO echo error message
    return nil
  end
  if t == VIM_DICTIONARY then
    return function(state, self, ...)
      return ft(state, value, ...)
    end
  else
    return function(state, self, ...)
      return ft(state, nil, ...)
    end
  end
end)

subscript = function(state, value, index)
  t = vim_type(state, value, value_position)
  if (t == nil) then
    return nil
  elseif (t == VIM_FLOAT) then
    err.err(state, value_position, true, 'E806: Using Float as a String')
    return nil
  elseif (t == VIM_NUMBER) then
    value = value .. ''
    t = VIM_STRING
  end
  if (t == VIM_STRING) then
    index = get_number(state, index, index_position)
    if (index == nil) then
      return nil
    elseif (index < 0) then
      return ''
    else
      return value:sub(index, index)
    end
  elseif (t == VIM_LIST) then
    index = get_number(state, index, index_position)
    if (index == nil) then
      return nil
    end
    return list.subscript(state, value, value_position, index, index_position)
  elseif (t == VIM_DICTIONARY) then
    index = get_string(state, index, index_position)
    if (index == nil) then
      return nil
    end
    return dict.subscript(state, value, value_position, index, index_position)
  elseif (t == VIM_FUNCREF) then
    err.err(state, value_position, true, 'E695: Cannot index a Funcref')
    return nil
  elseif (t == VIM_FLOAT) then
    err.err(state, value_position, true, 'E806: Using Float as a String')
    return nil
  end
end

func_concat_or_subscript = non_nil(function(state, dct, key)
  local t, f, ft
  t = vim_type(state, dct, dct_position)
  if t == VIM_DICTIONARY then
    f = dict.subscript(state, dct, dct_position, key, key_position)
    ft = vim_type(state, f)
  else
    local real_f
    scope, key = get_scope_and_key(state, key)
    real_f = dict.subscript(state, scope, key_position, key, key_position)
    ft = vim_type(state, real_f)
    f = function(state, self, ...)
      return vim.concat(state, dct, ft(state, nil, ...))
    end
  end
  if ft ~= VIM_FUNCREF then
    -- TODO echo error message
    return nil
  end
  if t == VIM_DICTIONARY then
    return function(state, self, ...)
      return f(state, dct, ...)
    end
  else
    return f
  end
end)

concat_or_subscript = non_nil(function(state, dct, key)
  t = vim_type(state, dct, dct_position)
  if (t == VIM_DICTIONARY) then
    return dict.subscript(state, dct, dct_position, key, key_position)
  else
    return concat(state, dct, key)
  end
end)

slice = non_nil(function(state, value, start_index, end_index)
  -- TODO
end)

call = non_nil(function(state, callee, ...)
  return callee(state, nil, ...)
end)

get_scope_and_key = non_nil(function(state, key)
  -- TODO
end)

-- {{{1 return
zero=number.new(state, 0)
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
  func_subscript=func_subscript,
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
  get_scope_and_key=get_scope_and_key,
  equals=equals,
  identical=identical,
  matches=matches,
  greater=greater,
  less=less,
  number=number,
  string=string,
  float=float,
  err=err,
  assign_scope=assign_scope,
  assign_scope_function=assign_scope_function,
  assign_subscript=assign_subscript,
  assign_subscript_function=assign_subscript_function,
  assign_slice=assign_slice,
  assign_slice_function=assign_slice_function,
  get_boolean=get_boolean,
  zero_=zero,
  false_=zero,
  true_=number.new(state, 1),
  VIM_NUMBER=VIM_NUMBER,
  VIM_STRING=VIM_STRING,
  VIM_FUNCREF=VIM_FUNCREF,
  VIM_LIST=VIM_LIST,
  VIM_DICTIONARY=VIM_DICTIONARY,
  VIM_FLOAT=VIM_FLOAT,
  type=vim_type,
}
