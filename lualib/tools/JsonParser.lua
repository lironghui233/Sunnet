local cjson = require "cjson" --用到了'../luaclib/cjson.so'文件 在loader.lua中配置好了 并且在c++中直接开启服务之前直接执行了loader.lua文件 这样require就能找到对应的文件了

function json_pack(cmd, msg)
	msg._cmd = cmd
	local body = cjson.encode(msg) --协议体字节流
	local namelen = string.len(cmd) --协议名长度
	local bodylen = string.len(body) --协议体长度
	local len = namelen + bodylen + 2 --协议总长度
	local format = string.format("> i2 i2 c%d c%d", namelen, bodylen)
	local buff = string.pack(format, len, namelen, cmd, body)
	return buff
end

function json_unpack(buff) --buff表示去掉“长度信息”后的消息体（netpack模块会把前两字节切掉）
	local len = string.len(buff)
	local namelen_format = string.format("> i2 c%d", len-2)
	local namelen, other = string.unpack(namelen_format, buff)
	local bodylen = len-2-namelen
	local format = string.format("> c%d c%d", namelen, bodylen)
	local cmd, bodybuff = string.unpack(format, other)

	local isok, msg = pcall(cjson.decode, bodybuff)
	if not isok or not msg or not msg._cmd or not cmd == msg._cmd then
		print("error")
		return
	end

	return cmd, msg
end


-------------------- 测试接口 ---------------------------

--json编码解码测试
function json_parse_test()
	local msg = {
		_cmd = "playerinfo",
		coin = 100,
		bag = {
			[1] = {1001, 1},
			[2] = {1005, 5},
		}
	}	
	--编码
	local buff_with_len = json_pack("playerinfo", msg)
	local len = string.len(buff_with_len)
	print("len:" .. len)
	print(buff_with_len)
	--解码
	local format = string.format(">i2 c%d", len-2)
	local _, buff = string.unpack(format, buff_with_len)
	local cmd, umsg = json_unpack(buff)
	print("cmd:" .. cmd)
	print("coin:" .. umsg.coin)
	print("sword:" .. umsg.bag[1][2])
end