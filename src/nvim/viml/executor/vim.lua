-- {{{1 Global declarations
local err
local vim_type
local is_func, is_dict, is_list, is_float
local scalar
local number, string, list, dict, float, func
local scope, def_scope, b_scope, a_scope, v_scope, f_scope
local state, assign
local scope_name_idx, type_idx, locks_idx, val_idx
local BASE_TYPE_NUMBER, BASE_TYPE_STRING, BASE_TYPE_FUNCREF, BASE_TYPE_LIST
local BASE_TYPE_DICTIONARY, BASE_TYPE_FLOAT
local get_number, get_float, get_string, get_boolean
-- {{{1 Utility functions
local get_local_option = function(state, other, name)
  return other.options[name] or state.options[name]
end

local non_nil = function(wrapped)
  return function(state, ...)
    for i = 1,select('#', ...) do
      if select(i, ...) == nil then
        return nil
      end
    end
    return wrapped(state, ...)
  end
end

local copy_table = function(tbl)
  local new_tbl = {}
  for k, v in pairs(tbl) do
    new_tbl[k] = v
  end
  return new_tbl
end

local join_tables = function(tbl1, tbl2)
  local ret = {}
  for k, v in pairs(tbl1) do
    ret[k] = v
  end
  for k, v in pairs(tbl2) do
    ret[k] = v
  end
  return ret
end

-- {{{1 State manipulations
local new_script_scope = function(state)
  return scope:new(state, 's')
end

local top

state = {
  new = function()
    local state = {
      -- values that need to be altered when entering/exiting some scopes
      is_trying = false,
      is_silent = false,
      a = nil,
      l = nil,
      sid = nil,
      current_scope = nil,
      call_stack = {},
      exception = '',
      throwpoint = '',
    }
    -- Values that do not need to be altered when exiting scope.
    state.global = {
      options = {},
      buffer = {b = b_scope:new(state), options = {}},
      window = {w = scope:new(state, 'w'), options = {}},
      tabpage = {t = scope:new(state, 't')},
      registers = {},
      v = v_scope:new(state),
      g = def_scope:new(state, 'g'),
      user_functions = f_scope:new(state),
      user_commands = {},
    }
    state.current_scope = state.global.g
    return state
  end,

  get_top = function()
    if top == nil then
      top = state:new()
    end
    return top
  end,

  enter_try = function(old_state)
    local state = copy_table(old_state)
    state.is_trying = true
    return state
  end,

  enter_catch = function(old_state, err)
    local state = copy_table(old_state)
    state.exception = err
    -- FIXME
    state.throwpoint = nil
    return state
  end,

  enter_function = function(old_state, fcall)
    local state = copy_table(old_state)
    state.a = a_scope:new(state)
    state.l = def_scope:new(state, 'l')
    state.current_scope = state.l
    state.call_stack = copy_table(state.call_stack)
    table.insert(state.call_stack, fcall)
    return state
  end,

  enter_script = function(old_state, s, sid)
    local state = copy_table(old_state)
    state.s = s
    state.sid = sid
    return state
  end,
}

-- {{{1 Functions returning specific types
get_number = function(state, val, position)
  local t = vim_type(state, val, position)
  return t.as_number(state, val, position)
end

get_string = function(state, val, position)
  local t = vim_type(state, val, position)
  return t.as_string(state, val, position)
end

get_float = function(state, val, position)
  local t = vim_type(state, val, position)
  return (t.as_float or t.as_number)(state, val, position)
end

get_boolean = function(state, val, position)
  local num = get_number(state, val, position)
  return num and num ~= 0
end

-- {{{1 Types
-- {{{2 Utility functions
local add_type_table = function(func)
  return function(self, ...)
    local ret = func(...)
    if ret then
      ret[type_idx] = self
    end
    return ret
  end
end

-- {{{2 Related constants
-- Special values that cannot be assigned from VimL
type_idx = true
locks_idx = false
val_idx = 0
scope_name_idx = -1

BASE_TYPE_NUMBER     = 0
BASE_TYPE_STRING     = 1
BASE_TYPE_FUNCREF    = 2
BASE_TYPE_LIST       = 3
BASE_TYPE_DICTIONARY = 4
BASE_TYPE_FLOAT      = 5

-- {{{2 Scalar type base
scalar = {
-- {{{3 Assignment support
  assign_subscript = function(state, val, val_position, ...)
    return err.err(state, val_position, true,
                   'E689: Can only index a List or Dictionary')
  end,
  assign_subscript_function = function(state, unique, val, val_position, ...)
    return scalar.assign_subscript(state, val, val_position, ...)
  end,
  assign_slice = function(state, val, val_position, ...)
    return scalar.assign_subscript(state, val, val_position, ...)
  end,
-- {{{3 Querying support
  subscript = function(state, val, val_position, idx, idx_position)
    local str = get_string(state, val, val_position)
    if str == nil then
      return nil
    end
    idx = get_number(idx)
    if idx == nil then
      return nil
    end
    if idx < 0 then
      return ''
    end
    return string.sub(str, idx + 1, idx + 1)
  end,
  slice = function(state, val, val_position, idx1, idx1_position,
                                             idx2, idx2_position)
    local str = get_string(state, val, val_position)
    if str == nil then
      return nil
    end
    idx1 = get_number(idx1)
    if idx1 == nil then
      return nil
    end
    idx2 = get_number(idx2)
    if idx2 == nil then
      return nil
    end
    if idx1 >= 0 then
      idx1 = idx1 + 1
    end
    if idx2 >= 0 then
      idx2 = idx2 + 1
    end
    return string.sub(str, idx1, idx2)
  end,
-- }}}3
}
-- {{{2 Number
number = join_tables(scalar, {
  type_number = BASE_TYPE_NUMBER,
-- {{{3 string()
  to_string_echo = function(state, num, num_position)
    return tostring(num)
  end,
-- {{{3 Type conversions
  as_number = function(state, num, num_position)
    return num
  end,
  as_string = function(state, num, num_position)
    return tostring(num)
  end,
  as_float = function(state, num, num_position)
    return num
  end,
-- {{{3 Operators support
  less_greater_cmp_priority = 1,
  less_greater_cmp_conv_priority = 1,
  less_greater_cmp_conv = 'as_number',
  equals_cmp_conv_priority = 1,
  equals_cmp = 'as_number',
-- }}}3
})

-- {{{2 String
string = join_tables(scalar, {
  type_number = BASE_TYPE_STRING,
-- {{{3 string()
  to_string_echo = function(state, str, str_position)
    return str
  end,
-- {{{3 Type conversions
  as_number = function(state, str, str_position)
    return tonumber(str:match('^%d+')) or 0
  end,
  as_string = function(state, str, str_position)
    return str
  end,
  as_float = function(state, num, num_position)
    return num
  end,
-- {{{3 Operators support
  less_greater_cmp_conv_priority = 0,
  less_greater_cmp_conv = 'as_string',
  equals_cmp_conv_priority = 0,
  equals_cmp_conv = 'as_string',
  equals_cmp = function(state, ic, str1, str1_position, str2, str2_position)
    -- TODO
  end,
-- }}}3
})

-- {{{2 List
list = {
-- {{{3 New
  type_number = BASE_TYPE_LIST,
  new = add_type_table(function(state, ...)
    local ret = {...}
    ret.iterators = {}
    ret.locked = false
    ret.fixed = false
    return ret
  end),
-- {{{3 Locks support
  can_modify = function(state, lst, lst_position)
    if (lst.fixed) then
      -- TODO error out
      return nil
    elseif (lst.locked) then
      -- TODO error out
      return nil
    end
    return true
  end,
-- {{{3 Modification support
  insert = function(state, lst, lst_position, pos, pos_position,
                           val, val_position)
    if not list.can_modify(state, lst, lst_positioon) then
      return nil
    end
    table.insert(self, pos, val)
    return lst
  end,
  append = function(state, lst, lst_position, pos, pos_position)
    if not list.can_modify(state, lst, lst_positioon) then
      return nil
    end
    table.insert(self, val)
  end,
-- {{{3 Assignment support
  raw_assign_subscript = function(lst, idx, val)
    lst[idx + 1] = val
  end,
  assign_subscript = function(state, lst, lst_position, idx, idx_position,
                                     val, val_position)
    -- TODO check for locks, check for out of range
    raw_assign_subscript(lst, idx, val)
    return true
  end,
  assign_slice = function(state, dct,  dct_position,
                                 idx1, idx1_position,
                                 idx2, idx2_position,
                                 val,  val_position)
    if is_func(val) then
      return err.err(state, lst_position, true,
                     'E475: Cannot assign function to a slice')
    end
    -- TODO slice assignment
    return true
  end,
-- {{{3 Querying support
  length = table.maxn,
  raw_subscript = function(lst, idx)
    return lst[idx + 1]
  end,
  subscript = function(state, lst, lst_position, idx, idx_position)
    idx = get_number(idx)
    if idx == nil then
      return nil
    end
    local length = list.length(lst)
    if (idx < 0) then
      idx = length + idx
    end
    if (idx >= length) then
      return err.err(state, lst_position, true,
                     'E684: list index out of range: %i', idx)
    end
    return list.raw_subscript(lst, idx)
  end,
  slice = function(state, lst, lst_position, idx1, idx1_position,
                                             idx2, idx2_position)
    idx1 =          get_number(idx1)
    idx2 = idx1 and get_number(idx2)
    if idx1 == nil then
      return nil
    end
    local length = list.length(lst)
    if idx1 < 0 then
      idx1 = idx1 + length
    end
    idx1 = idx1 + 1
    if idx2 < 0 then
      idx2 = idx2 + length
    end
    idx2 = idx2 + 1
    local ret = list:new(state)
    for i = idx1,idx2 do
      list.raw_assign_subscript(ret, i - idx1, list.raw_subscript(lst, i))
    end
    return ret
  end,
  raw_slice_to_end = function(lst, idx)
    local ret = list:new(state)
    for i = idx,list.length(lst) do
      ret[i - idx + 1] = lst[i]
    end
    return ret
  end,
-- {{{3 Support for iterations
  next = function(it_state, _)
    local i = it_state.i
    if i >= it_state.maxi then
      return nil
    end
    it_state.i = it_state.i + 1
    return i, list.raw_subscript(it_state.lst, i)
  end,
  iterator = function(state, lst)
    local it_state = {
      i = 0,
      maxi = list.length(lst),
      lst = lst,
    }
    table.insert(lst.iterators, it_state)
    return list.next, it_state, lst
  end,
-- {{{3 string()
  to_string_echo = function(state, lst, refs)
    local ret = '['
    local length = list.length(lst)
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
-- {{{3 Type conversions
  as_number = function(state, lst, lst_position)
    return err.err(state, lst_position, true, 'E745: Using List as a Number')
  end,
  as_string = function(state, lst, lst_position)
    return err.err(state, lst_position, true, 'E730: Using List as a String')
  end,
  as_list = function(state, lst, lst_position)
    return lst
  end,
-- {{{3 Operators support
  less_greater_cmp_conv_priority = 10,
  less_greater_cmp_conv = nil,
  less_greater_cmp_invalid_message = 'E692: Invalid operation for Lists',
  equals_cmp_conv_priority = 10,
  equals_cmp_conv = 'as_list',
  equals_cmp = function(state, ic, lst1, lst1_position, lst2, lst2_position)
    -- TODO
  end,
-- }}}3
}

-- {{{2 Dictionary
dict = {
-- {{{3 New
  type_number = BASE_TYPE_DICTIONARY,
  new = add_type_table(function(state, ...)
    local ret = {}
    if select('#', ...) % 2 ~= 0 then
      return err.err(state, position, true,
                     'Internal error: odd number of arguments to dict:new')
    end
    for i = 1,(select('#', ...)/2) do
      local key = select(2*i - 1, ...)
      local val = select(2*i, ...)
      if not (key and val) then
        return nil
      end
      ret[get_string(state, key)] = val
    end
    return ret
  end),
-- {{{3 Modification support
-- {{{3 Assignment support
  assign_subscript = function(state, dct, dct_position, key, key_position,
                                     val, val_position)
    if key == '' then
      return err.err(state, key_position, true,
                     'E713: Cannot use empty key for dictionary')
    end
    -- TODO check for locks
    dct[key] = val
    return true
  end,
  non_unique_function_message = 'E717: Dictionary entry already exists: %s',
  assign_subscript_function = function(state, unique, dct, dct_position,
                                                      key, key_position,
                                                      val, val_position)
    if unique and dct[key] ~= nil then
      return err.err(state, key_position, true,
                     dct[type_idx].non_unique_function_message,
                     key)
    end
    return dict.assign_subscript(state, dct, dct_position, key, key_position,
                                        val, val_position)
  end,
  assign_slice = function(state, dct,  dct_position, ...)
    return err.err(state, dct_position, true,
                   'E719: Cannot use [:] with a Dictionary')
  end,
-- {{{3 Querying support
  missing_key_message = 'E716: Key not present in Dictionary',
  subscript = function(state, dct, dct_position, key, key_position)
    key = get_string(state, key, key_position)
    if key == nil then
      return nil
    end
    local ret = dct[key]
    if (ret == nil) then
      local message
      message = dct[type_idx].missing_key_message
      return err.err(state, val_position, true, message .. ': %s', key)
    end
    return ret
  end,
-- {{{3 Type conversions
  as_number = function(state, dct, dct_position)
    return err.err(state, dct_position, true,
                   'E728: Using Dictionary as a Number')
  end,
  as_string = function(state, dct, dct_position)
    return err.err(state, dct_position, true,
                   'E731: Using Dictionary as a String')
  end,
  as_dict = function(state, dct, dct_position)
    return dct
  end,
-- {{{3 Operators support
  less_greater_cmp_conv_priority = 10,
  less_greater_cmp_conv = nil,
  less_greater_cmp_invalid_message = 'E736: Invalid operation for Dictionaries',
  equals_cmp_conv_priority = 10,
  equals_cmp_conv = 'as_dict',
  equals_cmp = function(state, ic, dct1, dct1_position, dct2, dct2_position)
    -- TODO
  end,
-- }}}3
}

-- {{{2 Float
float = join_tables(scalar, {
-- {{{3 New
  type_number = BASE_TYPE_FLOAT,
  new = add_type_table(function(state, f)
    return {[val_idx] = f}
  end),
-- {{{3 string()
  to_string_echo = function(state, flt, flt_position)
    return tostring(flt[val_idx])
  end,
-- {{{3 Type conversions
  as_number = function(state, flt, flt_position)
    return err.err(state, flt_position, true, 'E805: Using Float as a Number')
  end,
  as_string = function(state, flt, flt_position)
    return err.err(state, flt_position, true, 'E806: Using Float as a String')
  end,
  as_float = function(state, flt, flt_position)
    return flt[val_idx]
  end,
-- {{{3 Operators support
  less_greater_cmp_conv_priority = 2,
  less_greater_cmp_conv = 'as_float',
  equals_cmp_conv_priority = 2,
  equals_cmp_conv = 'as_float',
-- }}}3
})

-- {{{2 Function reference
func = join_tables(scalar, {
  type_number = BASE_TYPE_FUNCREF,
-- {{{3 Querying support
  subscript = function(state, fun, fun_position, ...)
    return err.err(state, fun_position, true, 'E695: Cannot index a Funcref')
  end,
  slice = function(...)
    return func.subscript(...)
  end,
-- {{{3 Type conversions
  as_number = function(state, fun, fun_position)
    return err.err(state, fun_position, true, 'E703: Using Funcref as a Number')
  end,
  as_string = function(state, fun, fun_position)
    return err.err(state, fun_position, true, 'E729: Using Funcref as a String')
  end,
  as_func = function(state, fun, fun_position)
    return fun
  end,
-- {{{3 Operators support
  less_greater_cmp_conv_priority = 10,
  less_greater_cmp_conv = nil,
  less_greater_cmp_invalid_message = 'E694: Invalid operation for Funcrefs',
  equals_cmp_conv_priority = 10,
  equals_cmp_conv = 'as_func',
-- }}}3
})

-- {{{2 Scope dictionaries
scope = join_tables(dict, {
  new = add_type_table(function(state, scope_name, ...)
    local ret = dict:new(state, ...)
    ret[scope_name_idx] = scope_name
    return ret
  end),
  special_keys = {},
  subscript = function(state, dct, dct_position, key, key_position)
    local special = dct[type_idx].special_keys[key]
    if special then
      return special(state)
    else
      return dict.subscript(state, dct, dct_position, key, key_position)
    end
  end,
  missing_key_message = 'E121: Undefined variable',
  assign_subscript = function(state, dct, dct_position, key, key_position,
                                     val, val_position)
    if not key:match('^%a[%w_]*$') then
      return err.err(state, key_position, true,
                     'E461: Illegal variable name: %s', key)
    end
    return dict.assign_subscript(state, dct, dct_position, key, key_position,
                                        val, val_position)
  end,
})

-- {{{2 g: and l: dictionaries
def_scope = join_tables(scope, {
  assign_subscript = function(state, dct, dct_position, key, key_position,
                                     val, val_position)
    if is_func(val) then
      if state.global.user_functions[key] then
        return err.err(
          state, key_position, true,
          'E705: Variable name conflicts with existing function: %s', key
        )
      end
    end
    return scope.assign_subscript(state, dct, dct_position, key, key_position,
                                         val, val_position)
  end
})

-- {{{2 b: scope dictionary
b_scope = join_tables(scope, {
  new = add_type_table(function(state, ...)
    return scope:new(state, 'b', ...)
  end),
})

-- {{{2 a: scope dictionary
a_scope = join_tables(scope, {
  new = add_type_table(function(state, ...)
    return scope:new(state, 'a', ...)
  end),
})

-- {{{2 v: scope dictionary
v_scope = join_tables(scope, {
  new = add_type_table(function(state, ...)
    return scope:new(state, 'v', ...)
  end),
  special_keys = {
    exception = function(state)
      return state.exception
    end,
    throwpoint = function(state)
      return state.throwpoint
    end,
  },
})

-- {{{2 Functions dictionary
f_scope = join_tables(dict, {
  missing_key_message = 'E117: Unknown function',
  non_unique_function_message =
                        'E122: Function %s already exists, add ! to replace it',
  new = add_type_table(function(state, ...)
    return scope:new(state, nil, ...)
  end),
})

-- {{{2 Define types table
types = {
  number = number,
  string = string,
  list = list,
  dict = dict,
  float = float,
}

-- {{{1 Error manipulation
err = {
  matches = function(state, err, regex)
    if (err:match(regex)) then
      return true
    else
      return false
    end
  end,

  propagate = function(state, err)
    error(err)
  end,

  err = function(state, position, vim_error, message, ...)
    local formatted_message
    formatted_message = (message):format(...)
    -- TODO show context
    if state.is_trying then
      -- TODO if vim_error == true then prepend message with Vim(cmd):
      error(formatted_message, 0)
    else
      io.stderr:write(formatted_message .. '\n')
    end
  end,

  throw = function(...)
  end,
}

-- {{{1 Built-in function implementations
local functions = f_scope:new(nil)
functions.type = function(state, self, ...)
  return vim_type(state, ...)['type_number']
end

-- {{{1 Built-in commands implementations
local string_echo = function(state, v, v_position, refs)
  t = vim_type(state, v, v_position)
  if t == nil then
    return nil
  end
  if refs ~= nil then
    -- TODO
  end
  return t.to_string_echo(state, v, refs)
end

local commands = {
  append = function(state, range, bang, lines)
  end,
  abclear = function(state, buffer)
  end,
  echo = non_nil(function(state, ...)
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

-- {{{1 User commands implementation
local parse = function(state, command, range, bang, args)
  return args, nil
end

local run_user_command = function(state, command, range, bang, args)
  command = state.global.user_commands[command]
  if (command == nil) then
    -- TODO Record line number
    return err.err(state, position, true, 'E492: Not a editor command: %s',
                   command)
  end
  args, after = parse(state, command, range, bang, args)
  command.func(state, range, bang, args)
  if (after) then
    after(state)
  end
end

-- {{{1 Assign support
assign = {
  dict = non_nil(function(state, val, dct, key)
    local t = vim_type(state, dct, dct_position)
    return t.assign_subscript(state, dct, dct_position, key, key_position,
                                     val, val_position)
  end),

  dict_function = non_nil(function(state, unique, val, dct, key)
    local t = vim_type(state, dct, dct_position)
    return t.assign_subscript_function(
      state, unique,
      dct, dct_position,
      key, key_position,
      val, val_position
    )
  end),

  slice = non_nil(function(state, val, lst, idx1, idx2)
    local t = vim_type(state, lst, lst_position)
    return t.assign_slice(
      state,
      lst, lst_position,
      idx1, idx1_position,
      idx2, idx2_position,
      val, val_position
    )
  end),

  slice_function = non_nil(function(state, unique, ...)
    return assign.slice(state, ...)
  end),
}

-- {{{1 Range handling
local range = {
  compose = function(state, ...)
  end,
  apply_followup = function(state, followup_type, followup_data, lnr)
  end,

  mark = function(state, mark)
  end,
  forward_search = function(state, regex)
  end,
  backward_search = function(state, regex)
  end,
  last = function(state)
  end,
  current = function(state)
  end,
  substitute_search = function(state)
  end,
  forward_previous_search = function(state)
  end,
  backward_previous_search = function(state)
  end,
}

-- {{{1 vim_type and is_* functions
vim_type = function(state, val, position)
  if (val == nil) then
    return nil
  end
  t = type(val)
  if (t == 'string') then
    return string
  elseif (t == 'number') then
    return number
  elseif (t == 'table') then
    local typ = val[type_idx]
    if typ == nil then
      return err.err(state, position, true,
                     'Internal error: table with unknown type')
    end
    return typ
  elseif t == 'function' then
    return func
  else
    return err.err(state, position, true, 'Internal error: unknown type')
  end
  return nil
end

is_func = function(val)
  return type(val) == 'function'
end

is_dict = function(val)
  return (type(val) == 'table'
          and val[type_idx]
          and type(val[type_idx]) == 'table'
          and val[type_idx].type_number == BASE_TYPE_DICTIONARY)
end

is_list = function(val)
  return (type(val) == 'table' and val[type_idx] == list)
end

is_float = function(val)
  return (type(val) == 'table' and val[type_idx] == float)
end

-- {{{1 Operators
local iterop = non_nil(function(state, converter, opfunc, ...)
  local result
  if select('#', ...) < 2 then
    return nil
  end
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
  return result
end)

local vim_true = 1
local vim_false = 0

local different_complex_type_messages = {
  [BASE_TYPE_LIST] = 'E691: Can only compare List with List',
  [BASE_TYPE_DICTIONARY] = 'E735: Can only compare Dictionary with Dictionary',
  [BASE_TYPE_FUNCREF] = 'E693: Can only compare Funcref with Funcref',
}

local same_complex_types = function(state, t1, t2, arg2_position)
  if (t1 == nil or t2 == nil) then
    return nil
  end
  local bt1 = t1.type_number
  if different_complex_type_messages[bt1] then
    if bt1 ~= t2.type_number then
      return err.err(state, arg2_position, true,
                     different_complex_type_messages[bt1])
    end
    return 1
  end
  return 2
end

local get_compared_values = function(state, t1, t2, key, arg1, arg1_position,
                                                         arg2, arg2_position)
  local conv, cmp
  if t1[key .. '_conv_priority'] > t2[key .. '_conv_priority'] then
    conv = t1[key .. '_conv']
    cmp = t1[key]
  else
    conv = t2[key .. '_conv']
    cmp = t2[key]
  end
  if conv == nil then
    return err.err(state, position, true,
                   (t1[key .. '_invalid_message'] or
                    t2[key .. '_invalid_message'])), nil
  end
  local v1 =        t1[conv](state, arg1, arg1_position)
  local v2 = v1 and t2[conv](state, arg2, arg2_position)
  return v1, v2, conv, cmp
end

local less_greater_cmp = function(state, ic, cmp, string_icmp,
                                  arg1, arg1_position,
                                  arg2, arg2_position)
  local t1 = vim_type(state, arg1, arg1_position)
  local t2 = vim_type(state, arg2, arg2_position)
  if (same_complex_types(state, t1, t2, arg2_position) == nil) then
    return nil
  end
  local v1, v2, conv
  v1, v2, conv = get_compared_values(state, t1, t2, 'less_greater_cmp',
                                     arg1, arg1_position,
                                     arg2, arg2_position)
  if v1 == nil then
    return nil
  end
  if conv == 'as_string' and ic then
    return string_icmp(v1, v2) and vim_true or vim_false
  else
    return cmp(v1, v2) and vim_true or vim_false
  end
end

local simple_identical = {
  [BASE_TYPE_LIST] = true,
  [BASE_TYPE_DICTIONARY] = true,
  [BASE_TYPE_FUNCREF] = true,
}

local op = {
  add = function(state, ...)
    return iterop(state, get_number, function(state, result, a2, a2_position)
      return result + a2
    end, ...)
  end,

  subtract = function(state, ...)
    return iterop(state, get_number, function(state, result, a2, a2_position)
      return result - a2
    end, ...)
  end,

  multiply = function(state, ...)
    return iterop(state, get_number, function(state, result, a2, a2_position)
      return result * a2
    end, ...)
  end,

  divide = function(state, ...)
    return iterop(state, get_number, function(state, result, a2, a2_position)
      return result / a2
    end, ...)
  end,

  modulo = function(state, ...)
    return iterop(state, get_number, function(state, result, a2, a2_position)
      return result % a2
    end, ...)
  end,

  concat = function(state, ...)
    return iterop(state, get_string, function(state, result, a2, a2_position)
      return result .. a2
    end, ...)
  end,

  negate = function(state, val)
    val = get_number(state, val, position)
    if (val == nil) then
      return nil
    else
      return -val
    end
  end,

  negate_logical = non_nil(function(state, val)
    val = get_number(state, val, position)
    if (val == nil) then
      return nil
    elseif (val == 0) then
      return vim_true
    else
      return vim_false
    end
  end),

  promote_integer = function(state, val)
    return get_number(state, val, position)
  end,

  equals = function(state, ic, arg1, arg2)
    local t1 = vim_type(state, arg1, arg1_position)
    local t2 = vim_type(state, arg2, arg2_position)
    if (same_complex_types(state, t1, t2, arg2_position) == nil) then
      return nil
    end
    local v1, v2
    v1, v2, conv, cmp = get_compared_values(state, t1, t2, 'equals_cmp',
                                            arg1, arg1_position,
                                            arg2, arg2_position)
    if v1 == nil then
      return nil
    end
    if cmp then
      return (cmp(state, ic, v1, arg1_position, v2, arg2_position)
              and vim_true or vim_false)
    else
      return (v1 == v2) and vim_true or vim_false
    end
  end,

  identical = non_nil(function(state, ic, arg1, arg2)
    local t1 = vim_type(state, arg1, arg1_position)
    local t2 = vim_type(state, arg2, arg2_position)
    if (t1 ~= t2) then
      return vim_false
    elseif simple_identical[t1.type_number] then
      return (arg1 == arg2) and vim_true or vim_false
    else
      return op.equals(state, ic, arg1, arg2)
    end
  end),

  matches = non_nil(function(state, ic, arg1, arg2)
    -- TODO
  end),

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
  end,

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
  end,
}

-- {{{1 Subscripting
local subscript = {
  func = non_nil(function(state, val, idx)
    local f = subscript.subscript(state, val, idx)
    if not is_func(f) then
      -- TODO echo error message
      return nil
    end
    if is_dict(val) then
      return function(state, self, ...)
        return f(state, val, ...)
      end
    else
      return function(state, self, ...)
        return f(state, nil, ...)
      end
    end
  end),

  subscript = function(state, val, idx)
    local t = vim_type(state, val, val_position)
    return t.subscript(state, val, val_position, idx, idx_position)
  end,

  slice = function(state, val, idx1, idx2)
    if not (val and idx1 and idx2) then
      return nil
    end
    local t = vim_type(state, val, val_position)
    return t.slice(state, val, val_position, idx1 or  0, idx1_position,
                                             idx2 or -1, idx2_position)
  end,

  call = non_nil(function(state, callee, ...)
    return callee(state, ...)
  end),
}

local func_concat_or_subscript = non_nil(function(state, dct, key)
  local f
  local dct_is_dict = is_dict(dct)
  local f_is_func
  if dct_is_dict then
    f = dict.subscript(state, dct, dct_position, key, key_position)
    f_is_func = is_func(f)
  else
    local real_f
    local scope
    scope, key = get_scope_and_key(state, key)
    real_f = dict.subscript(state, scope, key_position, key, key_position)
    f_is_func = is_func(real_f)
    f = function(state, self, ...)
      return op.concat(state, dct, f(state, nil, ...))
    end
  end
  if f_is_func then
    -- TODO echo error message
    return nil
  end
  if dct_is_dict then
    return function(state, self, ...)
      return f(state, dct, ...)
    end
  else
    return f
  end
end)

local concat_or_subscript = non_nil(function(state, dct, key)
  if is_dict(dct) then
    return dict.subscript(state, dct, dct_position, key, key_position)
  else
    return op.concat(state, dct, key)
  end
end)

local get_scope_and_key = non_nil(function(state, key)
  -- TODO
end)

-- {{{1 return
local zero = 0
return {
  state = state,
  new_script_scope = new_script_scope,
  set_script_locals = set_script_locals,
  commands = commands,
  functions = functions,
  range = range,
  run_user_command = run_user_command,
  iter_start = iter_start,
  subscript = subscript,
  list = list,
  dict = dict,
  concat_or_subscript = concat_or_subscript,
  get_scope_and_key = get_scope_and_key,
  op = op,
  number = number,
  string = string,
  float = float,
  err = err,
  assign = assign,
  get_boolean = get_boolean,
  zero_ = zero,
  false_ = zero,
  true_ = 1,
  type = vim_type,
  types = types,
  is_func = is_func,
  is_dict = is_dict,
  is_list = is_list,
  is_float = is_float,
  get_local_option = get_local_option,
}
