### 简介：

Sunnet系统是用C++实现的模仿Skynet的游戏服务器后端。Sunnet是多线程的服务端架构，通过多线程调度充分利用了机器的性能。

### 目录说明：

 - **3rd/**： 第三方库，这里存放了Lua-5.3.6源码
 - **build/**：构建工程时的临时文件、可执行文件
 - **include/**：存放头文件（.h）
 - **service/**: 存放Lua脚本
 - **src/**：存放源文件（.cpp）
 - **CMakelist.txt**：CMake指导文件

### 编译：

1. 进入build目录
2. cmake ../
3. make

### 运行：

```
./sunnet
```