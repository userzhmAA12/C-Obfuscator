// #include "action.h"
#include "obfus_action.h"

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>
#include <filesystem>
#include <llvm/Support/CommandLine.h>

namespace fs = std::filesystem;

static llvm::cl::OptionCategory MyASTSlicer_category("myastslicer options");
static llvm::cl::extrahelp
    common_help(clang::tooling::CommonOptionsParser::HelpMessage);

int main(int argc, const char **argv)
{
    fs::path file_path(argv[1]);
    if (!fs::exists(file_path))
    {
        std::cout << "compile_database not exist"
                  << "\n";
        return 0;
    }
    fs::path dir_path = file_path.parent_path();
    for (const auto &entry1 : fs::recursive_directory_iterator(dir_path))
    {
        if ((entry1.path().extension() == ".h" || entry1.path().extension() == ".hh") && entry1.path().stem().string().find("-obfuscated") == std::string::npos)
        {
            int argc_h = 2;
            std::string tmp = entry1.path();
            const char *argv_h[2] = {"./C-Obufuscator", tmp.c_str()};
            auto expected_parser = clang::tooling::CommonOptionsParser::create(
                argc_h,
                argv_h,
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
            clang::tooling::ClangTool tool(options_parser.getCompilations(),
                                           options_parser.getSourcePathList());
            clang::tooling::ArgumentsAdjuster ardj = clang::tooling::getInsertArgumentAdjuster("-I/usr/local/lib/clang/18/include");
            tool.appendArgumentsAdjuster(ardj);
            for (auto it : tool.getSourcePaths())
            {
                std::cout << "** test!!! " << it << "\n";
            }
            std::string info_file = argv[argc - 1];

            std::unique_ptr<obfuscator::ObfusFactory> my_factory =
                std::make_unique<obfuscator::ObfusFactory>(info_file);
            tool.run(my_factory.get());
        }
    }
    for (const auto &entry1 : fs::recursive_directory_iterator(dir_path))
    {
        if ((entry1.path().extension() == ".h" || entry1.path().extension() == ".hh") && entry1.path().stem().string().find("-obfuscated") == std::string::npos)
        {
            int argc_h = 2;
            std::string tmp = entry1.path();
            const char *argv_h[2] = {"./C-Obufuscator", tmp.c_str()};
            auto expected_parser = clang::tooling::CommonOptionsParser::create(
                argc_h,
                argv_h,
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
            clang::tooling::ClangTool tool(options_parser.getCompilations(),
                                           options_parser.getSourcePathList());
            clang::tooling::ArgumentsAdjuster ardj = clang::tooling::getInsertArgumentAdjuster("-I/usr/local/lib/clang/18/include");
            tool.appendArgumentsAdjuster(ardj);
            for (auto it : tool.getSourcePaths())
            {
                std::cout << "** test!!! " << it << "\n";
            }
            std::string info_file = argv[argc - 1];

            std::unique_ptr<obfuscator::ObfusFactory> my_factory =
                std::make_unique<obfuscator::ObfusFactory>(info_file);
            tool.run(my_factory.get());
        }
    }

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
    std::string info_file = argv[argc - 1];

    std::unique_ptr<obfuscator::ObfusFactory> my_factory =
        std::make_unique<obfuscator::ObfusFactory>(info_file);
    tool.run(my_factory.get());
    tool.run(my_factory.get());
    std::cout << "[obfuscator exit]\n";
    return 0;
}