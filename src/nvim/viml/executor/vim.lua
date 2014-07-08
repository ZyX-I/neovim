-- {{{1 Global declarations
local err
local op
local vim_type
local is_func, is_dict, is_list, is_float
local scalar, container
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
    state.call_stack = copy_table(call_stack)
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
  local t = vim_type(val)
  return t.as_number(state, val, position)
end

get_string = function(state, val, position)
  local t = vim_type(val)
  return t.as_string(state, val, position)
end

get_float = function(state, val, position)
  local t = vim_type(val)
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

local num_convert_2 = function(func, converter)
  return function(state, val1, val1_position, val2, val2_position)
    local n1 = converter(state, val1, val1_position)
    if n1 == nil then
      return nil
    end
    local n2 = converter(state, val2, val2_position)
    if n2 == nil then
      return nil
    end
    return func(n1, n2)
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
-- {{{3 Operators support
  num_op_priority = 1,
  add = num_convert_2(function(n1, n2)
    return n1 + n2
  end, get_number),
  subtract = num_convert_2(function(n1, n2)
    return n1 - n2
  end, get_number),
  multiply = num_convert_2(function(n1, n2)
    return n1 * n2
  end, get_number),
  divide = num_convert_2(function(n1, n2)
    local ret = n1 / n2
    return math.floor(math.abs(ret)) * ((ret >= 0) and 1 or -1)
  end, get_number),
  modulo = num_convert_2(function(n1, n2)
    return n1 % n2
  end, get_number),
  negate = function(state, val, val_position)
    local n = get_number(state, val, val_position)
    return n and -n
  end,
  promote_integer = get_number,
-- }}}3
}

-- {{{2 Container type base
local numop = function(state, val1, val1_position, val2, val2_position)
  -- One of the following calls must fail for container type
  return (get_float(state, val1, val1_position)
          and get_float(state, val2, val2_position))
end

container = {
-- {{{3 Operators support
  num_op_priority = 10,
  add = numop,
  subtract = numop,
  multiply = numop,
  divide = numop,
  modulo = numop,
  negate = numop,
  promote_integer = numop,
-- }}}3
}

-- {{{2 Basic types
-- {{{3 Number
number = join_tables(scalar, {
  type_number = BASE_TYPE_NUMBER,
-- {{{4 string()
  to_string_echo = function(state, num, num_position)
    return tostring(num)
  end,
-- {{{4 Type conversions
  as_number = function(state, num, num_position)
    return num
  end,
  as_string = function(state, num, num_position)
    return tostring(num)
  end,
  as_float = function(state, num, num_position)
    return num
  end,
-- {{{4 Operators support
  cmp_priority = 1,
  cmp = function(state, ic, eq, val1, val1_position, val2, val2_position)
    local n1 = get_number(state, val1, val1_position)
    if n1 == nil then
      return nil
    end
    local n2 = get_number(state, val2, val2_position)
    if n2 == nil then
      return nil
    end
    return (n1 > n2 and 1) or ((n1 == n2 and 0) or -1)
  end,
-- }}}4
})

-- {{{3 String
string = join_tables(scalar, {
  type_number = BASE_TYPE_STRING,
-- {{{4 string()
  to_string_echo = function(state, str, str_position)
    return str
  end,
-- {{{4 Type conversions
  as_number = function(state, str, str_position)
    return tonumber(str:match('^%-?%d+')) or 0
  end,
  as_string = function(state, str, str_position)
    return str
  end,
  as_float = function(state, num, num_position)
    return num
  end,
-- {{{4 Operators support
  cmp_priority = 0,
  cmp = function(state, ic, eq, val1, val1_position, val2, val2_position)
    -- TODO Handle ic
    local s1 = get_string(state, val1, val1_position)
    if s1 == nil then
      return nil
    end
    local s2 = get_string(state, val2, val2_position)
    if s2 == nil then
      return nil
    end
    return (s1 > s2 and 1) or ((s1 == s2 and 0) or -1)
  end,
-- }}}4
})

-- {{{3 List
list = join_tables(container, {
-- {{{4 New
  type_number = BASE_TYPE_LIST,
  new = add_type_table(function(state, ...)
    local ret = {...}
    ret.iterators = {}
    ret.locked = false
    ret.fixed = false
    return ret
  end),
-- {{{4 Locks support
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
-- {{{4 Modification support
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
-- {{{4 Assignment support
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
-- {{{4 Querying support
  length = table.maxn,
  raw_subscript = function(lst, idx)
    return lst[idx + 1]
  end,
  subscript = function(state, lst, lst_position, idx, idx_position)
    idx = get_number(idx)
    if idx == nil then
      return nil
    end
    local length = list.raw_length(lst)
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
    local length = list.raw_length(lst)
    if idx1 < 0 then
      idx1 = idx1 + length
    end
    idx1 = idx1 + 1
    if idx2 < 0 then
      idx2 = idx2 + length
    end
    idx2 = idx2 + 1
    ret = list:new(state)
    for i = idx1,idx2 do
      list.raw_assign_subscript(ret, i - idx1, list.raw_subscript(lst, i))
    end
    return ret
  end,
-- {{{4 Support for iterations
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
      maxi = list.raw_length(lst),
      lst = lst,
    }
    table.insert(lst.iterators, it_state)
    return list.next, it_state, lst
  end,
-- {{{4 string()
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
-- {{{4 Type conversions
  as_number = function(state, lst, lst_position)
    return err.err(state, lst_position, true, 'E745: Using List as a Number')
  end,
  as_string = function(state, lst, lst_position)
    return err.err(state, lst_position, true, 'E730: Using List as a String')
  end,
-- {{{4 Operators support
  cmp_priority = 10,
  cmp = function(state, ic, eq, val1, val1_position, val2, val2_position)
    -- TODO Handle ic
    if not (is_list(val1) and is_list(val2)) then
      return err.err(state, val2_position, true,
                     'E691: Can only compare List with List')
    elseif not eq then
      return err.err(state, val1_position, true,
                     'E692: Invalid operation for Lists')
    end
    local length = list.length(val1)
    if length ~= list.length(val2) then
      return 1
    elseif length == 0 then
      return 0
    else
      for i = 1,length do
        local subv1 = val1[i]
        local subv2 = val2[i]
        local st1 = vim_type(subv1)
        local st2 = vim_type(subv2)
        if st1.type_number ~= st2.type_number then
          return 1
        elseif st1.cmp(state, ic, true, subv1, val1_position,
                                        subv2, val2_position) ~= 0 then
          return 1
        end
      end
      return 0
    end
  end,
  num_op_priority = 3,
  add = function(state, val1, val1_position, val2, val2_position)
    if not (is_list(val1) and is_list(val2)) then
      -- Error out
      if is_list(val1) then
        return list.as_number(state, val1, val1_position)
      else
        return list.as_number(state, val2, val2_position)
      end
    end
    local length1 = list.length(val1)
    local length2 = list.length(val2)
    local ret = copy_table(val1)
    for i, v in ipairs(val2) do
      ret[length1 + i] = v
    end
    return ret
  end,
-- }}}4
})

-- {{{3 Dictionary
dict = join_tables(container, {
-- {{{4 New
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
-- {{{4 Modification support
-- {{{4 Assignment support
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
-- {{{4 Querying support
  missing_key_message = 'E716: Key not present in Dictionary',
  subscript = function(state, dct, dct_position, key, key_position)
    key = get_string(state, key, key_position)
    if key == nil then
      return nil
    end
    ret = dct[key]
    if (ret == nil) then
      local message
      message = dct[type_idx].missing_key_message
      return err.err(state, val_position, true, message .. ': %s', key)
    end
    return ret
  end,
-- {{{4 Type conversions
  as_number = function(state, dct, dct_position)
    return err.err(state, dct_position, true,
                   'E728: Using Dictionary as a Number')
  end,
  as_string = function(state, dct, dct_position)
    return err.err(state, dct_position, true,
                   'E731: Using Dictionary as a String')
  end,
-- {{{4 Operators support
  cmp_priority = 10,
  cmp = function(state, ic, eq, val1, val1_position, val2, val2_position)
    -- TODO Handle ic
    if not (is_dict(val1) and is_dict(val2)) then
      return err.err(state, val2_position, true,
                     'E735: Can only compare Dictionary with Dictionary')
    elseif not eq then
      return err.err(state, val1_position, true,
                     'E736: Invalid operation for Dictionaries')
    end
    for k, subv2 in pairs(val2) do
      if type(k) == 'string' then
        if val1[k] == nil then
          return 1
        end
      end
    end
    for k, subv1 in pairs(val1) do
      if type(k) == 'string' then
        local subv2 = val2[k]
        if subv2 == nil then
          return 1
        end
        local st1 = vim_type(subv1)
        local st2 = vim_type(subv2)
        if st1.type_number ~= st2.type_number then
          return 1
        elseif st1.cmp(state, ic, true, subv1, val1_position,
                                        subv2, val2_position) ~= 0 then
          return 1
        end
      end
    end
    return 0
  end,
-- }}}4
})

-- {{{3 Float
local return_float = function(func)
  return function(state, ...)
    local ret = func(state, ...)
    return float:new(state, ret)
  end
end

float = join_tables(scalar, {
-- {{{4 New
  type_number = BASE_TYPE_FLOAT,
  new = add_type_table(function(state, f)
    return {[val_idx] = f}
  end),
-- {{{4 string()
  to_string_echo = function(state, flt, flt_position)
    return tostring(flt[val_idx])
  end,
-- {{{4 Type conversions
  as_number = function(state, flt, flt_position)
    return err.err(state, flt_position, true, 'E805: Using Float as a Number')
  end,
  as_string = function(state, flt, flt_position)
    return err.err(state, flt_position, true, 'E806: Using Float as a String')
  end,
  as_float = function(state, flt, flt_position)
    return flt[val_idx]
  end,
-- {{{4 Operators support
  cmp_priority = 2,
  cmp = function(state, ic, eq, val1, val1_position, val2, val2_position)
    local f1 = get_float(state, val1, val1_position)
    if f1 == nil then
      return nil
    end
    local f2 = get_float(state, val2, val2_position)
    if f2 == nil then
      return nil
    end
    return (f1 > f2 and 1) or ((f1 == f2 and 0) or -1)
  end,
  num_op_priority = 2,
  add = return_float(num_convert_2(function(f1, f2)
    return f1 + f2
  end, get_float)),
  subtract = return_float(num_convert_2(function(f1, f2)
    return f1 - f2
  end, get_float)),
  multiply = return_float(num_convert_2(function(f1, f2)
    return f1 * f2
  end, get_float)),
  divide = return_float(num_convert_2(function(f1, f2)
    return f1 / f2
  end, get_float)),
  modulo = return_float(num_convert_2(function(f1, f2)
    return f1 % f2
  end, get_float)),
  negate = return_float(function(state, flt, flt_position)
    return -flt[val_idx]
  end),
  promote_integer = function(state, flt, flt_position)
    return flt
  end,
-- }}}4
})

-- {{{3 Function reference
func = join_tables(scalar, {
  type_number = BASE_TYPE_FUNCREF,
-- {{{4 Querying support
  subscript = function(state, fun, fun_position, ...)
    return err.err(state, val_position, true, 'E695: Cannot index a Funcref')
  end,
  slice = function(...)
    return func.subscript(...)
  end,
-- {{{4 Type conversions
  as_number = function(state, fun, fun_position)
    return err.err(state, fun_position, true, 'E703: Using Funcref as a Number')
  end,
  as_string = function(state, fun, fun_position)
    return err.err(state, fun_position, true, 'E729: Using Funcref as a String')
  end,
-- {{{4 Operators support
  cmp_priority = 10,
  cmp = function(state, ic, eq, val1, val1_position, val2, val2_position)
    if not (is_func(val1) and is_func(val2)) then
      return err.err(state, val2_position, true,
                     'E693: Can only compare Funcref with Funcref')
    elseif not eq then
      return err.err(state, val1_position, true,
                     'E694: Invalid operation for Funcrefs')
    end
    return val1 == val2 and 0 or 1
  end,
-- }}}4
})

-- {{{2 Scopes
-- {{{3 Base type for scope dictionaries
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
  non_unique_function_message =
                        'E122: Function %s already exists, add ! to replace it',
})

-- {{{3 g: and l: dictionaries
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

-- {{{3 b: scope dictionary
b_scope = join_tables(scope, {
  new = add_type_table(function(state, ...)
    return scope:new(state, 'b', ...)
  end),
})

-- {{{3 a: scope dictionary
a_scope = join_tables(scope, {
  new = add_type_table(function(state, ...)
    return scope:new(state, 'a', ...)
  end),
})

-- {{{3 v: scope dictionary
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

-- {{{3 Functions dictionary
f_scope = join_tables(dict, {
  missing_key_message = 'E117: Unknown function',
  new = add_type_table(function(state, ...)
    return scope:new(state, nil, ...)
  end),
})

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
functions.type = function(state, self, val)
  return vim_type(val)['type_number']
end

-- {{{1 Built-in commands implementations
local string_echo = function(state, v, v_position, refs)
  local t = vim_type(v)
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
  echo = function(state, ...)
    local mes = ''
    for i = 1,select('#', ...) do
      s = select(i, ...)
      if s == nil then
        return nil
      end
      mes = mes .. ' '
      local chunk = string_echo(state, s, s_position, {})
      if chunk == nil then
        return nil
      end
      mes = mes .. chunk
    end
    print (mes:sub(2))
  end,
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
  dict = function(state, val, dct, key)
    if not (val and dct and key) then
      return nil
    end
    local t = vim_type(dct)
    return t.assign_subscript(state, dct, dct_position, key, key_position,
                                     val, val_position)
  end,

  dict_function = function(state, unique, val, dct, key)
    if not (val and dct and key) then
      return nil
    end
    local t = vim_type(dct)
    return t.assign_subscript_function(
      state, unique,
      dct, dct_position,
      key, key_position,
      val, val_position
    )
  end,

  slice = function(state, val, lst, idx1, idx2)
    if not (val and lst and idx1 and idx2) then
      return nil
    end
    local t = vim_type(lst)
    return t.assign_slice(
      state,
      lst, lst_position,
      idx1, idx1_position,
      idx2, idx2_position,
      val, val_position
    )
  end,

  slice_function = function(state, unique, ...)
    return assign.slice(state, ...)
  end,
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
type_table = {
  ['string'] = string,
  ['number'] = number,
  ['function'] = func,
}

vim_type = function(val)
  return val and (type_table[type(val)] or val[type_idx])
end

is_func = function(val)
  return type(val) == 'function'
end

is_dict = function(val)
  return (type(val) == 'table'
          and val[type_idx].type_number == BASE_TYPE_DICTIONARY)
end

is_list = function(val)
  return (type(val) == 'table' and val[type_idx] == list)
end

is_float = function(val)
  return (type(val) == 'table' and val[type_idx] == float)
end

-- {{{1 Operators
local iterop = function(state, op, ...)
  local result = select(1, ...)
  if result == nil then
    return nil
  end
  local tr = vim_type(result)
  for i = 2,select('#', ...) do
    local v = select(i, ...)
    if v == nil then
      return nil
    end
    local ti = vim_type(v)
    if tr.num_op_priority >= ti.num_op_priority then
      result = tr[op](state, result, nil, v, nil)
    else
      result = ti[op](state, result, nil, v, nil)
    end
    tr = vim_type(result)
  end
  return result
end

local vim_true = 1
local vim_false = 0

local cmp = function(state, ic, eq, arg1, arg1_position, arg2, arg2_position)
  if not (arg1 and arg2) then
    return nil
  end
  local t1 = vim_type(arg1)
  local t2 = vim_type(arg2)
  local cmp
  if t1.cmp_priority >= t2.cmp_priority then
    cmp = t1.cmp
  else
    cmp = t2.cmp
  end
  return cmp(state, ic, eq, arg1, arg1_position,
                            arg2, arg2_position)
end

local simple_identical = {
  [BASE_TYPE_LIST] = true,
  [BASE_TYPE_DICTIONARY] = true,
  [BASE_TYPE_FUNCREF] = true,
}

op = {
  add = function(state, ...)
    return iterop(state, 'add', ...)
  end,

  subtract = function(state, ...)
    return iterop(state, 'subtract', ...)
  end,

  multiply = function(state, ...)
    return iterop(state, 'multiply', ...)
  end,

  divide = function(state, ...)
    return iterop(state, 'divide', ...)
  end,

  modulo = function(state, ...)
    return iterop(state, 'modulo', ...)
  end,

  concat = function(state, ...)
    local result = select(1, ...)
    result = result and get_string(state, result)
    if result == nil then
      return nil
    end
    for i = 2,select('#', ...) do
      local v = select(i, ...)
      v = v and get_string(state, v)
      if v == nil then
        return nil
      end
      result = result .. v
    end
    return result
  end,

  negate = function(state, val)
    local t = vim_type(val)
    return t.negate(state, val, position)
  end,

  negate_logical = function(state, val)
    return val and (get_number(state, val, position) == 0
                    and vim_true or vim_false)
  end,

  promote_integer = function(state, val)
    local t = vim_type(val)
    return t.promote_integer(state, val, position)
  end,

  identical = function(state, ic, arg1, arg2)
    if not (arg1 or arg2) then
      return nil
    end
    local t1 = vim_type(arg1)
    local t2 = vim_type(arg2)
    if (t1.type_number ~= t2.type_number) then
      return vim_false
    elseif simple_identical[t1.type_number] then
      return (arg1 == arg2) and vim_true or vim_false
    else
      return op.equals(state, ic, arg1, arg2)
    end
  end,

  matches = function(state, ic, arg1, arg2)
    if not (arg1 or arg2) then
      return nil
    end
    -- TODO
  end,

  less = function(state, ic, arg1, arg2)
    return (cmp(state, ic, false, arg1, arg1_position, arg2, arg2_position)
            == -1) and vim_true or vim_false
  end,

  greater = function(state, ic, arg1, arg2)
    return (cmp(state, ic, false, arg1, arg1_position, arg2, arg2_position)
            == 1) and vim_true or vim_false
  end,

  equals = function(state, ic, arg1, arg2)
    return (cmp(state, ic, true, arg1, arg1_position, arg2, arg2_position)
            == 0) and vim_true or vim_false
  end,
}

-- {{{1 Subscripting
local subscript = {
  func = function(state, val, idx)
    if not (val and idx) then
      return nil
    end
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
  end,

  subscript = function(state, val, idx)
    if not (val and idx) then
      return nil
    end
    local t = vim_type(val)
    return t.subscript(state, val, val_position, idx, idx_position)
  end,

  slice = function(state, val, idx1, idx2)
    if not (val and idx1 and idx2) then
      return nil
    end
    local t = vim_type(val)
    return t.slice(state, val, val_position, idx1 or  0, idx1_position,
                                             idx2 or -1, idx2_position)
  end,

  call = non_nil(function(state, callee, ...)
    return callee(state, nil, ...)
  end),
}

local func_concat_or_subscript = function(state, dct, key)
  if not (dct or key) then
    return nil
  end
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
end

local concat_or_subscript = function(state, dct, key)
  if is_dict(dct) then
    return dict.subscript(state, dct, dct_position, key, key_position)
  else
    return op.concat(state, dct, key)
  end
end

local get_scope_and_key = function(state, key)
  if not key then
    return nil
  end
  -- TODO
end

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
  types = {
    number = number,
    string = string,
    list = list,
    dict = dict,
    float = float,
  },
  is_func = is_func,
  is_dict = is_dict,
  is_list = is_list,
  is_float = is_float,
  get_local_option = get_local_option,
}
