
--可以优化成从配置读取 这里仅作一个演示

--加载lua脚本的路径 
--加载C/C++动态库.so的路径


package.path = package.path .. ';../lualib/extension/?.lua';
package.path = package.path .. ';../lualib/tools/?.lua';
package.path = package.path .. ';../lualib/?.lua';
package.cpath = package.cpath .. ';../luaclib/?.so';

require ".table"
