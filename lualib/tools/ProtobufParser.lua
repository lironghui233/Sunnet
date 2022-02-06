local pb = require "protobuf" --用到了'../luaclib/protobuf.so'和'../lualib/protobuf.lua'文件 在loader.lua中配置好了 并且在c++中直接开启服务之前直接执行了loader.lua文件 这样require就能找到对应的文件了

pb.register_file("../service/proto/all.bytes")

local proto_id_map = nil
local get_proto_name = function (msgid)
	if not proto_id_map then
		local file = io.open("../service/proto/message_define.bytes", "rb")
		local source = file:read("*a")
		proto_id_map = load(source)()
		file:close()
		source = nil
	end
	return proto_id_map[msgid]
end

local get_proto_id = function (msg_name)
	if not proto_id_map then
		local file = assert(io.open("../service/proto/message_define.bytes", "rb")) 
		local source = file:read("*a")
		proto_id_map = load(source)()
		file:close()
		source = nil
	end
	for k,v in pairs(proto_id_map) do
		if v == tostring(msg_name) then
			return tonumber(k) 
		end
	end 
end

local process_msg = function (fd, msgstr)
	local cmd, msg = decode_protobuf_msg(msgstr)
	print("process_msg" , cmd, table.PrintTable(msg) )
end

local get_complete_msg = function(buf)
	if not buf or buf == "" or #buf == o then
		return nil
	end

    --取得头部2字节的内容，此为后续数据的长度
    local s = buf:byte(1) * 256 + buf:byte(2)

    --不足以一个包
    if #buf < (s+2) then
        return nil
    end

    --完整包
    return buf:sub(3, 2+s), buf:sub(3+s, #buf)
end

function encode_protobuf_msg(msgid, data)
	local msg_name = get_proto_name(msgid)						-- 根据协议id获取协议名
    local stringbuffer =  pb.encode(msg_name, data)         	-- 根据协议名和协议数据  protobuf序列化 返回lua_string
    local body = string.pack(">I2s2", msgid, stringbuffer)      -- 打包包体 协议id + 协议数据
    local head = string.pack(">I2", #body)                    	-- 打包包体长度
    return head .. body                                       	-- 包体长度 + 协议id + 协议数据
end

function decode_protobuf_msg(msgstr)
	local msgid, stringbuffer = string.unpack(">I2s2", msgstr)	-- 解包包体 协议id + 协议数据
	local proto_name = get_proto_name(msgid)					-- 根据协议id获取协议名
    local body = pb.decode(proto_name, stringbuffer)			-- 根据协议名和协议数据  protobuf反序列化  返回lua_table

	return proto_name, body
end

function process_buff(readbuff)
	while true do
		local msgstr, rest = get_complete_msg(readbuff)
		if msgstr then
			readbuff = rest
			process_msg(fd, msgstr)
		else
			return readbuff
		end	
	end
end



-------------------- 测试接口 ---------------------------

-- --protobuf编码解码测试
function protobuf_parse_test()
	--编码
	local msg = {
		id = 101,
		pw = "123",
	}
	local buff = encode_protobuf_msg(get_proto_id("login_req"), msg)
	print("protobuf_parse_test buff_len:" .. string.len(buff))

	--解码
	process_buff(buff)
end