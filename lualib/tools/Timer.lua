
--[[定时器接口]]
-- 封装了调用C++接口 sunnet.AddTimer
-- 封装了调用C++接口 sunnet.DelTimer
-- 时间到了之后 C++直接通过func_name调用lua的_G[func_name],即t的元方法__call,详细看代码


id = 0
local function generate_func_id()
	id = id + 1
	return id
end

local function del_callback(name)
	_G[name] = nil
end

local function create_callback_table (func, name)
    local t = {}
    t.callback = func
    --创建元表。元方法__call。目的是在c++层，可以直接通过func_name调用_G[func_name](即t)，然后执行__call里的函数
    setmetatable (t, {__call =  -- 关注__call
        function (func, ...) -- 在t(xx)时，将调用到这个函数
            func.callback(...) -- 真正的回调
            del_callback(name) -- 回调完毕，清除wrap建立的数据
        end })
    return t
end

local function wrap (func)
    local id = generate_func_id() -- 产生唯一的id
    local fn_s = "_callback_fn".. id
    _G[fn_s] = create_callback_table(func, fn_s) -- _G[fn_s]对应的是一个表
    return fn_s
end

function AddTimer(serviceId, expire, func)
	local func_name = wrap(func)	
    --调c++函数
	return sunnet.AddTimer(serviceId, expire, func_name)
end

function DelTimer(timer_id)
    --调c++函数
    return sunnet.DelTimer(timer_id)
end



-------------------- 测试接口 ---------------------------

-- 创建定时器测试
function create_timer_test(serviceId)
    local function test()
        print("!!! --- [lua]  [create_timer_test callback success] --- !!! ")
    end
    local timer_id = AddTimer(serviceId, 3, test)
    return timer_id
end

-- 取消定时器测试
function del_timer_test(serviceId)
    local timer_id = create_timer_test(serviceId)
    local is_ok = DelTimer(timer_id)
    print("!!! --- [lua]  [del_timer_test timer_id] --- !!! ", timer_id, is_ok)
end