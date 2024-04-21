// #include "action.h"
#include "obfus_action.h"
#include "scan_action.h"

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>

#include <llvm/Support/CommandLine.h>


static llvm::cl::OptionCategory MyASTSlicer_category("myastslicer options");
static llvm::cl::extrahelp
    common_help(clang::tooling::CommonOptionsParser::HelpMessage);

int main(int argc, const char **argv)
{
    /*
    fs::path file_path(argv[1]);
    if (!fs::exists(file_path))
    {
        std::cout << "compile_database not exist"
                  << "\n";
        return 0;
    }
    fs::path dir_path = file_path.parent_path();
    */
   std::string file_path(argv[1]);
   std::string dir_path = obfuscator::getparentdir(file_path);

    int argc_f = argc - 1; // don't include return file path
    auto expected_parser = clang::tooling::CommonOptionsParser::create(
        argc_f,
        argv,
        MyASTSlicer_category
        // llvm::cl::NumOccurrencesFlag::ZeroOrMore
    );
    if (!expected_parser)
    {
        // Fail gracefully for unsupported options.
        llvm::errs() << expected_parser.takeError();
        return 1;
    }
    clang::tooling::CommonOptionsParser &options_parser = expected_parser.get();
    // clang::tooling::ClangTool tool(options_parser.getCompilations(),
    // options_parser.getSourcePathList());
    clang::tooling::ClangTool tool(
        options_parser.getCompilations(),
        options_parser.getCompilations().getAllFiles());
    clang::tooling::ArgumentsAdjuster ardj = clang::tooling::getInsertArgumentAdjuster("-I/usr/local/lib/clang/18/include");
    tool.appendArgumentsAdjuster(ardj);
    for (auto it : tool.getSourcePaths())
    {
        std::cout << "** test!!! " << it << "\n"; // it is "mytest.c"
    }                                             // std::string output_file = argv[argc - 1];

    // std::unique_ptr<astslicer::FuncFactory> func_factory =
    // std::make_unique<astslicer::FuncFactory>(func_info);

    // std::cout << "** befor tool.run()!!!\n";
    std::string info_file = argv[argc - 2];
    std::string func_file = argv[argc - 1];
    std::unique_ptr<obfuscator::ScanFactory> my_factory1 =
        std::make_unique<obfuscator::ScanFactory>(info_file);
    std::unique_ptr<obfuscator::ObfusFactory> my_factory2 =
        std::make_unique<obfuscator::ObfusFactory>(info_file, func_file);
    
    tool.run(my_factory1.get());
    // std::cout << "finish1\n";
    tool.run(my_factory2.get());
    std::cout << "[obfuscator exit]\n";
    return 0;
}