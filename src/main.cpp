// #include "action.h"
#include "obfus_action.h"
#include "scan_action.h"

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>
// #include <filesystem>
#include <llvm/Support/CommandLine.h>

#include <stdio.h>
#include <dirent.h>
#include <cstring>
#include <sys/stat.h>

// namespace fs = std::filesystem;

void recursive_directory(std::string basePath)
{
    std::string path;
    struct dirent *dp;
    DIR *dir = opendir(basePath.c_str());

    if (!dir) {
        return;
    }
    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
            path = basePath + "/" + dp->d_name;

            // 如果是目录，则递归调用自身
            struct stat statbuf;
            if (stat(path.c_str(), &statbuf) != -1)
            {
                if (S_ISDIR(statbuf.st_mode)) 
                {
                    recursive_directory(path);
                } 
                else 
                {
                    std::size_t lastDotPos = path.find_last_of('.');
                    if (lastDotPos != std::string::npos)
                    {
                        std::string extension = path.substr(lastDotPos + 1);
                        if ((extension == ".h" || extension == ".hh" || extension == ".hpp" || extension ==".c" || extension == ".cpp") )
                        {
                            size_t pos = path.find("-obfuscated");
                            if(pos!=std::string::npos)
                            {
                                std::string new_file_path = path.substr(0, pos) + extension;
                                rename(path.c_str(), new_file_path.c_str());
                            }
                        }
                    }
                }
            }
        }
    }

    closedir(dir);
}

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
    std::string info_file = argv[argc - 1];

    std::unique_ptr<obfuscator::ScanFactory> my_factory1 =
        std::make_unique<obfuscator::ScanFactory>(info_file);
    std::unique_ptr<obfuscator::ObfusFactory> my_factory2 =
        std::make_unique<obfuscator::ObfusFactory>(info_file);
    
    tool.run(my_factory1.get());
    std::cout << "finish1\n";
    tool.run(my_factory2.get());
    std::cout << "[obfuscator exit]\n";

    //std::string new_folder = dir_path.string() + "/obfuscated";
    //if(!fs::exists(new_folder))
        //fs::create_directory(new_folder);
    /* for (const auto &entry1 : fs::recursive_directory_iterator(dir_path))
    {
        if ((entry1.path().extension() == ".h" || entry1.path().extension() == ".hh" || entry1.path().extension() == ".hpp" || entry1.path().extension()==".c" || entry1.path().extension()==".cpp") )
        {
            size_t pos = entry1.path().stem().string().find("-obfuscated");
            if(pos!=std::string::npos)
            {
                std::string new_file_path = entry1.path().parent_path().string() + "/" + entry1.path().stem().string().substr(0, pos) + entry1.path().extension().string();
                fs::rename(entry1.path(), new_file_path);
            }
        }
    } */
    recursive_directory(dir_path);
    return 0;
}