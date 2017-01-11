local helpers = require('test.unit.helpers')
local eval_helpers = require('test.unit.eval.helpers')

local eq = helpers.eq
local neq = helpers.neq
local ffi = helpers.ffi
local NULL = helpers.NULL
local cimport = helpers.cimport
local alloc_log_new = helpers.alloc_log_new

local a = eval_helpers.alloc_logging_helpers
local list = eval_helpers.list
local lst2tbl = eval_helpers.lst2tbl
local type_key  = eval_helpers.type_key
local li_alloc  = eval_helpers.li_alloc
local dict_type  = eval_helpers.dict_type
local list_type  = eval_helpers.list_type
local lua2typvalt  = eval_helpers.lua2typvalt

local lib = cimport('./src/nvim/eval/typval.h', './src/nvim/memory.h')

local function list_index(l, idx)
  return tv_list_find(l, idx)
end

local function list_items(l)
  local lis = {}
  local li = l.lv_first
  for i = 1, l.lv_len do
    lis[i] = ffi.gc(li, nil)
    li = li.li_next
  end
  return lis
end

local function list_watch_alloc(li)
  return ffi.cast('listwatch_T*', ffi.new('listwatch_T[1]', {{lw_item=li}}))
end

local function list_watch(l, li)
  local lw = list_watch_alloc(li or l.lv_first)
  lib.tv_list_watch_add(l, lw)
  return lw
end

local function get_alloc_rets(exp_log, res)
  for i = 1,#exp_log do
    if ({malloc=true, calloc=true})[exp_log[i].func] then
      res[#res + 1] = exp_log[i].ret
    end
  end
  return exp_log
end

local alloc_log
local restore_allocators

local to_cstr_nofree = function(v) return lib.xstrdup(v) end

local alloc_log = alloc_log_new()

before_each(function()
  alloc_log:before_each()
end)

after_each(function()
  alloc_log:after_each()
end)

describe('typval.c', function()
  describe('list', function()
    describe('item', function()
      describe('alloc()/free()', function()
        it('works', function()
          local li = li_alloc(true)
          neq(nil, li)
          lib.tv_list_item_free(li)
          alloc_log:check({
            a.li(li),
            a.freed(li),
          })
        end)
        it('also frees the value', function()
          local li
          local s
          local l
          local tv
          li = li_alloc(true)
          li.li_tv.v_type = lib.VAR_NUMBER
          li.li_tv.vval.v_number = 10
          lib.tv_list_item_free(li)
          alloc_log:check({
            a.li(li),
            a.freed(li),
          })

          li = li_alloc(true)
          li.li_tv.v_type = lib.VAR_FLOAT
          li.li_tv.vval.v_float = 10.5
          lib.tv_list_item_free(li)
          alloc_log:check({
            a.li(li),
            a.freed(li),
          })

          li = li_alloc(true)
          li.li_tv.v_type = lib.VAR_STRING
          li.li_tv.vval.v_string = nil
          lib.tv_list_item_free(li)
          alloc_log:check({
            a.li(li),
            a.freed(alloc_log.null),
            a.freed(li),
          })

          li = li_alloc(true)
          li.li_tv.v_type = lib.VAR_STRING
          s = to_cstr_nofree('test')
          li.li_tv.vval.v_string = s
          lib.tv_list_item_free(li)
          alloc_log:check({
            a.li(li),
            a.str(s, #('test')),
            a.freed(s),
            a.freed(li),
          })

          li = li_alloc(true)
          li.li_tv.v_type = lib.VAR_LIST
          l = ffi.gc(list(), nil)
          l.lv_refcount = 2
          li.li_tv.vval.v_list = l
          lib.tv_list_item_free(li)
          alloc_log:check({
            a.li(li),
            a.list(l),
            a.freed(li),
          })
          eq(1, l.lv_refcount)

          li = li_alloc(true)
          tv = lua2typvalt({[type_key]=dict_type})
          tv.vval.v_dict.dv_refcount = 2
          li.li_tv = tv
          lib.tv_list_item_free(li)
          alloc_log:check({
            a.li(li),
            a.dict(tv.vval.v_dict),
            a.freed(li),
          })
          eq(1, tv.vval.v_dict.dv_refcount)
        end)
      end)
      describe('remove()', function()
        it('works', function()
          local l = list(1, 2, 3, 4, 5, 6, 7)
          neq(nil, l)
          local lis = list_items(l)
          alloc_log:check({
            a.list(l),
            a.li(lis[1]),
            a.li(lis[2]),
            a.li(lis[3]),
            a.li(lis[4]),
            a.li(lis[5]),
            a.li(lis[6]),
            a.li(lis[7]),
          })

          lib.tv_list_item_remove(l, lis[1])
          alloc_log:check({
            a.freed(table.remove(lis, 1)),
          })
          eq(lis, list_items(l))

          lib.tv_list_item_remove(l, lis[6])
          alloc_log:check({
            a.freed(table.remove(lis)),
          })
          eq(lis, list_items(l))

          lib.tv_list_item_remove(l, lis[3])
          alloc_log:check({
            a.freed(table.remove(lis, 3)),
          })
          eq(lis, list_items(l))
        end)
        it('works and adjusts watchers correctly', function()
          local l = ffi.gc(list(1, 2, 3, 4, 5, 6, 7), nil)
          neq(nil, l)
          local lis = list_items(l)
          -- Three watchers: pointing to first, middle and last elements.
          local lws = {
            list_watch(l, lis[1]),
            list_watch(l, lis[4]),
            list_watch(l, lis[7]),
          }

          lib.tv_list_item_remove(l, lis[4])
          ffi.gc(lis[4], lib.tv_list_item_free)
          eq({lis[1], lis[5], lis[7]}, {lws[1].lw_item, lws[2].lw_item, lws[3].lw_item})

          lib.tv_list_item_remove(l, lis[2])
          ffi.gc(lis[2], lib.tv_list_item_free)
          eq({lis[1], lis[5], lis[7]}, {lws[1].lw_item, lws[2].lw_item, lws[3].lw_item})

          lib.tv_list_item_remove(l, lis[7])
          ffi.gc(lis[7], lib.tv_list_item_free)
          eq({lis[1], lis[5], nil}, {lws[1].lw_item, lws[2].lw_item, lws[3].lw_item == nil and nil})

          lib.tv_list_item_remove(l, lis[1])
          ffi.gc(lis[1], lib.tv_list_item_free)
          eq({lis[3], lis[5], nil}, {lws[1].lw_item, lws[2].lw_item, lws[3].lw_item == nil and nil})

          alloc_log:clear()
          lib.tv_list_free(l)
          alloc_log:check({
            a.freed(lis[3]),
            a.freed(lis[5]),
            a.freed(lis[6]),
            a.freed(l),
          })
        end)
      end)
    end)
    describe('watch', function()
      describe('remove()', function()
        it('works', function()
          local l = ffi.gc(list(1, 2, 3, 4, 5, 6, 7), nil)
          eq(nil, l.lv_watch)
          local lw = list_watch(l)
          neq(nil, l.lv_watch)
          alloc_log:clear()
          lib.tv_list_watch_remove(l, lw)
          eq(nil, l.lv_watch)
          alloc_log:check({
            -- Does not free anything.
          })
          local lws = { list_watch(l), list_watch(l), list_watch(l) }
          alloc_log:clear()
          lib.tv_list_watch_remove(l, lws[2])
          eq(lws[3], l.lv_watch)
          eq(lws[1], l.lv_watch.lw_next)
          lib.tv_list_watch_remove(l, lws[1])
          eq(lws[3], l.lv_watch)
          eq(nil, l.lv_watch.lw_next)
          lib.tv_list_watch_remove(l, lws[3])
          eq(nil, l.lv_watch)
          alloc_log:check({
            -- Does not free anything.
          })
        end)
        it('ignores not found watchers', function()
          local l = list(1, 2, 3, 4, 5, 6, 7)
          local lw = list_watch_alloc()
          lib.tv_list_watch_remove(l, lw)
        end)
      end)
    end)
    -- add() and fix() were tested when testing tv_list_item_remove()
    describe('alloc()/free()', function()
      it('recursively frees list with recurse=true', function()
        local l1 = ffi.gc(list(1, 'abc'), nil)
        local l2 = ffi.gc(list({[type_key]=dict_type}), nil)
        local l3 = ffi.gc(list({[type_key]=list_type}), nil)
        local alloc_rets = {}
        alloc_log:check(get_alloc_rets({
          a.list(l1),
          a.li(l1.lv_first),
          a.str(l1.lv_last.li_tv.vval.v_string, #('abc')),
          a.li(l1.lv_last),
          a.list(l2),
          a.dict(l2.lv_first.li_tv.vval.v_dict),
          a.li(l2.lv_first),
          a.list(l3),
          a.list(l3.lv_first.li_tv.vval.v_list),
          a.li(l3.lv_first),
        }, alloc_rets))
        lib.tv_list_free(l1)
        alloc_log:check({
          {func='free', args={alloc_rets[2]}},
          {func='free', args={alloc_rets[3]}},
          {func='free', args={alloc_rets[4]}},
          {func='free', args={alloc_rets[1]}},
        })
        lib.tv_list_free(l2)
        alloc_log:check({
          {func='free', args={alloc_rets[6]}},
          {func='free', args={alloc_rets[7]}},
          {func='free', args={alloc_rets[5]}},
        })
        lib.tv_list_free(l3)
        alloc_log:check({
          {func='free', args={alloc_rets[9]}},
          {func='free', args={alloc_rets[10]}},
          {func='free', args={alloc_rets[8]}},
        })
      end)
      it('does not free container items with recurse=false', function()
        local l1 = ffi.gc(list('abc', {[type_key]=dict_type}, {[type_key]=list_type}), nil)
        local alloc_rets = {}
        alloc_log:check(get_alloc_rets({
          a.list(l1),
          a.str(l1.lv_first.li_tv.vval.v_string, #('abc')),
          a.li(l1.lv_first),
          a.dict(l1.lv_first.li_next.li_tv.vval.v_dict),
          a.li(l1.lv_first.li_next),
          a.list(l1.lv_last.li_tv.vval.v_list),
          a.li(l1.lv_last),
        }, alloc_rets))
        lib.tv_list_free(l1)
        alloc_log:check({
          {func='free', args={alloc_rets[2]}},
          {func='free', args={alloc_rets[3]}},
          {func='free', args={alloc_rets[5]}},
          {func='free', args={alloc_rets[7]}},
          {func='free', args={alloc_rets[1]}},
        })
        lib.tv_dict_free(alloc_rets[4])
        lib.tv_list_free(alloc_rets[6])
      end)
    end)
    describe('unref()', function()
      it('recursively frees list when reference count goes to 0', function()
        local l = ffi.gc(list({[type_key]=list_type}), nil)
        local alloc_rets = {}
        alloc_log:check(get_alloc_rets({
          a.list(l),
          a.list(l.lv_first.li_tv.vval.v_list),
          a.li(l.lv_first),
        }, alloc_rets))
        l.lv_refcount = 2
        lib.tv_list_unref(l)
        alloc_log:check({})
        lib.tv_list_unref(l)
        alloc_log:check({
          {func='free', args={alloc_rets[2]}},
          {func='free', args={alloc_rets[3]}},
          {func='free', args={alloc_rets[1]}},
        })
      end)
    end)
  end)
end)
