# ASTSlicer
`ASTSlicer`工具主要通过利用`DG`（一个基于LLVM IR的切片工具）的切片结果，即`DG`切片后指令对应源码中的位置信息或切片后函数的函数名信息，实现基于Clang AST的指令级和函数级切片功能。

为了获得`DG`的切片信息，我们对`DG`的代码进行了一定的修改，使其输出对应的指令级和函数级切片信息文件`xxx-instsInfo.txt`和`xxx-funcsInfo.txt`作为`ASTSlicer`的输入。具体请见`DG`项目中的`readme-cy.md`文件中的说明。

`ASTSlicer`通过对相同源码编译生成的Clang AST进行分析，利用Clang Tool中的`Rewrite`进行源码的修改，并输出切片后的C程序。

此外，由于IR和C代码之间的差异性，`ASTSlicer`的切片精度有一定损失。

## 环境依赖
1. 请在LLVM 14环境下运行。
2. LLVM14 和LLVM 12不完全兼容，如果有需要在LLVM 12下运行工具，请对main函数中的代码做如下修改。
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
1. 指令级
```shell
./astslicer [compile_commands.json PATH] inst [xxx-instsInfo.txt PATH]
# 示例
./astslicer ../tests/compile_commands.json inst /home/cy/Downloads/sort-instsInfo.txt 
# 利用compile_commands.json将源程序编译为AST。
# inst参数表示为指令级切片。
# inst参数后为源程序对应DG指令级切片输出的切片信息文件的路径。
```

2. 函数级
```shell
./astslicer [compile_commands.json PATH] func [xxx-funcsInfo.txt PATH]
# 示例
./astslicer ../tests/compile_commands.json func /home/cy/Downloads/sort-funcsInfo.txt 
# 利用compile_commands.json将源程序编译为AST。
# func参数表示为指令级切片。
# func参数后为源程序对应DG函数级切片输出的切片信息文件的路径。
```
3. 其他

    以上两种运行命令，利用compile_commands.json生成AST，如果对于一般的简单使用的情况下，如对单文件进行切片，可以使用以下方法，无需生成compile_commands.json，直接将源文件的路径作为参数。

    （注：此方法涉及代码修改，可临时使用。）
```c++
    // 将main文件中的以下代码
                       
    clang::tooling::ClangTool tool(
        options_parser.getCompilations(),
        options_parser.getCompilations().getAllFiles());
    // 修改为以下代码
    clang::tooling::ClangTool tool(options_parser.getCompilations(),
        options_parser.getSourcePathList());
```
进行如上修改，重新编译，即可使用以下命令运行。
```shell
# 指令级切片
./astslicer [xxx.c FILE PATH] inst [xxx-instsInfo.txt PATH]
# 示例
 ./astslicer ../tests/example.c inst /home/cy/test-files/example-instsInfo.txt

# 函数级切片
./astslicer [xxx.c FILE PATH] func [xxx-funcsInfo.txt PATH]
# 示例
 ./astslicer ../tests/example.c func /home/cy/test-files/example-funcsInfo.txt
```


## Clang编译命令
```
clang -Xclang -ast-dump -c example.c
```