local serviceId

require "Timer"
require "JsonParser"
require "ProtobufParser"

local function test_monitor()
	local function cb_test()
        print("!!! --- [lua]  [create_timer_test callback success] --- !!! ")
        while true do
        	-- 死循環 測試 monitor是否可以有效監聽并且終止服務 讓worker綫程正常完成流程
        end
    end
    local timer_id = AddTimer(serviceId, 3, cb_test)
end

local function test()
	
	print("!!! --- [lua] test start --- !!!: " .. id)
	
	-- test_monitor()
	-- create_timer_test(serviceId)
	-- del_timer_test(serviceId)
	-- json_parse_test()
	-- protobuf_parse_test()

	print("!!! --- [lua] test  end  --- !!!: " .. id)

end

function OnInit(id)
	serviceId = id
	print("[lua] test OnInit id: " .. id)
	test()
end

function OnAcceptMsg(listenfd, clientfd)
	print("[lua] test OnAcceptMsg: " .. clientfd)
end

function OnSocketData(fd, buff)
	print("[lua] test OnSocketData: " .. fd)
end

function OnSocketClose(fd)
	print("[lua] test OnSocketClose: " .. fd)
end

function OnServiceMsg(source, buff)
	print("[lua] test OnServiceMsg: " .. source .. " " .. buff)
end

