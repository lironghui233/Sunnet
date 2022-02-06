#项目名称
project(sunnet)
#CMake最低版本号要求
cmake_minimum_required(VERSION 2.8)
#头文件目录
include_directories(include)
include_directories(${cmake_CURRENT_SOURCE_DIR}/3rd/lua-5.4.4/src/) #添加lua头文件目录
#查找 ./src目录下所有的源文件，保存到DIR_SRCS变量中
aux_source_directory(./src DIR_SRCS)
#用c++11
## ubuntu
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread -Wl,--no-as-needed")
## redhat
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
add_definitions(${CMAKE_CXX_FLAGS})
#指定生成目标文件
add_executable(sunnet ${DIR_SRCS})
#Lua头文件、库
include_directories(./3rd/lua-5.4.4/src/) 
target_link_libraries(sunnet liblua.a)
target_link_libraries(sunnet dl)
#库文件
find_package(Threads)
target_link_libraries(sunnet ${cmake_THREAD_LIBS_INIT})
