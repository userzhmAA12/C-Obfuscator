# C-Obfuscator

`C-Obfuscator`通过对源码编译生成的Clang AST进行分析，利用Clang Tool中的`Rewrite`进行源码的修改，并输出混淆后的C程序。混淆主要进行源码中变量名和函数名的混淆，目前对宏的处理仍存在问题。


## 环境依赖
1. 请在LLVM 14以上环境下运行。
2. LLVM14 及以上版本 和LLVM 12不完全兼容，如果有需要在LLVM 12下运行工具，请对main函数中的代码做如下修改。
```c++
// 将main函数中以下代码
auto expected_parser = clang::tooling::CommonOptionsParser::create(
        argc_f,
        argv,
        MyASTSlicer_category);
// 修改为
auto expected_parser = clang::tooling::CommonOptionsParser::create(
        argc_f,
        argv,
        MyASTSlicer_category,
        llvm::cl::NumOccurrencesFlag::ZeroOrMore);
```

## 编译
```shell
mkdir build
cd build
cmake ..
make
```
## 运行
```shell
./C-Obfuscator [compile_commands.json PATH] [variable_replaced.txt PATH]
# 示例
./C-Obfuscator /home/zhm/C-Obfuscator/tests/build/compile_commands.json /home/zhm/C-Obfuscator/variable_replace.txt
# 利用compile_commands.json将源程序编译为AST，第一个参数为compile_commands.json的路径
# 最后一个参数为源程序对应的变量名替换记录的路径。
# 两个参数的路径需要在代混淆项目的最上级目录下，因为最后混淆文件替换源文件的操作需要按照这两个路径来扫描待混淆项目文件夹
```

## Clang编译命令
```
clang -Xclang -ast-dump -c example.c
```