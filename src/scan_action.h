#ifndef SCAN_ACTION_H
#define SCAN_ACTION_H


#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"

#include <clang/AST/Decl.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace obfuscator
{
static void replace_suffix(std::string &fn, std::string with)
{
    int index = fn.find_last_of(".");
    fn        = fn.substr(0, index) + with;
}
static bool can_obfuscate(std::string &fn)
{
    if(fn=="main")return false;
    return true; 
}
bool is_prefix(const std::string &str, const std::string &prefix) {
    if (prefix.size() > str.size()) {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (str[i] != prefix[i]) {
            return false;
        }
    }
    return true;
}

class ScanASTVisitor : public clang::RecursiveASTVisitor<ScanASTVisitor>
{
  public:
    explicit ScanASTVisitor(clang::ASTContext *ctx,
                            clang::Rewriter &R,
                            std::string &fn)
        : _ctx(ctx), _rewriter(R), info_path(fn), data(), count_func(0), count_var(0)
    {
        std::ifstream fin(info_path);
        if(!fin)
        {
            std::cout << "[error]open file:" << info_path << " failed!\n";
            exit(-1);
        }
        while(!fin.eof())
        {
            std::string op, pre_name, after_name;
            fin >> op;
            if(op == "Var")
            {
                fin >> pre_name >> after_name;
                count_var++;
                data.insert(std::pair<std::string, std::string>(pre_name, after_name));
            }
            else if(op == "Func")
            {
                fin >> pre_name >> after_name;
                count_func++;
                data.insert(std::pair<std::string, std::string>(pre_name, after_name));
            }
            else if(op == "Class")
            {
                fin >> pre_name >> after_name;
                count_var++;
                data.insert(std::pair<std::string, std::string>(pre_name, "ignore"));
                data.insert(std::pair<std::string, std::string>("~"+pre_name, "ignore"));
            }
        }
        fin.close();
    }

    bool VisitFunctionDecl(clang::FunctionDecl *FD)
    {
        // skip useless part in ast
        clang::SourceManager &SM = _ctx->getSourceManager();
        if(SM.isInSystemHeader(FD->getLocation()))
            return true;
        std::filesystem::path path(info_path);// variable_replace.txt path
        std::filesystem::path folder_path = path.parent_path();// project path
        if(SM.getFileID(FD->getLocation()).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(FD->getLocation()))).has_value())
                return true;
            std::string loc_file = SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(FD->getLocation()))).getPointer()->getName().str();
            if(!is_prefix(loc_file, folder_path))
                return true;
        }
        std::string func_name = FD->getNameAsString();
        if(func_name.length()==0)return true;
        if (!can_obfuscate(func_name))
        {
            return true;
        }
        if(data.count(func_name) == 0)
        {
            ++count_func;
            if(func_name.substr(0, 8) != "operator")
            {
                data.insert(std::pair<std::string, std::string>(func_name, "Function"+std::to_string(count_func)));
                std::cout << "func: "<< func_name << " is replaced by " << "Function" << std::to_string(count_func) << "\n";
                std::ofstream fout(info_path, std::ios::app);
                if(!fout)
                {   
                    std::cout << "[error]open file:" << info_path << " failed!\n";
                    exit(-1);
                }
                fout << "Func " << func_name << " " << "Function" << std::to_string(count_func) << "\n";
                fout.close();
            }
        }
        return true;
    }

    bool VisitVarDecl(clang::VarDecl *VD) //处理变量名混淆
    {
        clang::SourceManager &SM = _ctx->getSourceManager();
        if(SM.isInSystemHeader(VD->getLocation()))
            return true;
        std::filesystem::path path(info_path);// variable_replace.txt path
        std::filesystem::path folder_path = path.parent_path();// project path
        if(SM.getFileID(VD->getLocation()).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(VD->getLocation()))).has_value())
                return true;
            std::string loc_file = SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(VD->getLocation()))).getPointer()->getName().str();
            if(!is_prefix(loc_file, folder_path))
                return true;
        }
        
        std::string var_name = VD->getNameAsString();
        std::string var_type = VD->getType().getAsString();
        if(var_name.length()==0)return true;
        if(data.count(var_name)==0)
        {
            ++count_var;
            data.insert(std::pair<std::string, std::string>(VD->getNameAsString(), "Variable"+std::to_string(count_var)));
            std::cout << "var: " << var_name << " is replaced by " << "Variable" << std::to_string(count_var) << "\n";
            std::ofstream fout(info_path, std::ios::app);
            if(!fout)
            {   
                std::cout << "[error]open file:" << info_path << " failed!\n";
                exit(-1);
            }
            fout << "Var " << var_name << " " << "Variable" << std::to_string(count_var) << std::endl;
            fout.close();
        }
        return true;
    }
    bool VisitDeclRefExpr(clang::DeclRefExpr* DRE) 
    {
        return true;
    }
    bool VisitCXXRecordDecl(clang::CXXRecordDecl *RD) //处理自定义类的类名混淆
    {
        clang::SourceManager &SM = _ctx->getSourceManager();
        if(SM.isInSystemHeader(RD->getLocation()))
            return true;
        std::filesystem::path path(info_path);// variable_replace.txt path
        std::filesystem::path folder_path = path.parent_path();// project path
        if(SM.getFileID(RD->getLocation()).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(RD->getLocation()))).has_value())
                return true;
            std::string loc_file = SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(RD->getLocation()))).getPointer()->getName().str();
            if(!is_prefix(loc_file, folder_path))
                return true;
        }
        std::string record_name = RD->getNameAsString();
        if(record_name.length()==0)return true;
        if (data.count(record_name)==0) {
            ++count_var;
            data.insert(std::pair<std::string, std::string>(RD->getNameAsString(), "ignore"));
            data.insert(std::pair<std::string, std::string>("~"+RD->getNameAsString(), "ignore"));
            std::ofstream fout(info_path, std::ios::app);
            if(!fout)
            {   
                std::cout << "[error]open file:" << info_path << " failed!\n";
                exit(-1);
            }
            fout << "Class " << record_name << " " << "Variable" << std::to_string(count_var) << "\n";
            fout.close();
        }
        return true;
    }
    bool VisitFieldDecl(clang::FieldDecl *FD) 
    {
        clang::SourceManager &SM = _ctx->getSourceManager();
        if(SM.isInSystemHeader(FD->getLocation()))
            return true;
        std::filesystem::path path(info_path);// variable_replace.txt path
        std::filesystem::path folder_path = path.parent_path();// project path
        if(SM.getFileID(FD->getLocation()).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(FD->getLocation()))).has_value())
                return true;
            std::string loc_file = SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(FD->getLocation()))).getPointer()->getName().str();
            if(!is_prefix(loc_file, folder_path))
                return true;
        }
        std::string record_name = FD->getNameAsString();
        std::string var_type = FD->getType().getAsString();
        if(record_name.length()==0)return true;
        if (data.count(record_name)==0) 
        {
            ++count_var;
            data.insert(std::pair<std::string, std::string>(FD->getNameAsString(), "Variable"+std::to_string(count_var)));
            std::cout << "var: " << record_name << " is repalced by " << "Variable" << std::to_string(count_var) << "\n";
            std::ofstream fout(info_path, std::ios::app);
            if(!fout)
            {   
                std::cout << "[error]open file:" << info_path << " failed!\n";
                exit(-1);
            }
            fout << "Var " << record_name << " " << "Variable" << std::to_string(count_var) << "\n";
            fout.close();
        }
        return true;
    }
    bool VisitMemberExpr(clang::MemberExpr *ME)
    {
        return true;
    }
    bool VisitCXXConstructorDecl(clang::CXXConstructorDecl *CCD)
    {
        return true;
    }
  private:
    clang::ASTContext *_ctx;
    clang::Rewriter &_rewriter;
    std::string &info_path;
    std::map<std::string, std::string> data; //储存函数名和变量名的替换信息
    int count_func;
    int count_var;
    
};

class ScanASTConsumer : public clang::ASTConsumer
{
  public:
    explicit ScanASTConsumer(clang::ASTContext *ctx,
                             clang::Rewriter &R,
                             std::string &fn)
        : _visitor(ctx, R, fn)
    {
        // std::cout << "gouzao FuncASTConsumer\n";
    }

    virtual void HandleTranslationUnit(clang::ASTContext &ctx)
    {
        // llvm::errs()
        //     << "in function FuncASTConsumer::HandleTranslationUnit()\n";
        _visitor.TraverseDecl(ctx.getTranslationUnitDecl());
    }

  private:
    ScanASTVisitor _visitor;
};

class ScanFrontendAction : public clang::ASTFrontendAction
{
  public:
    ScanFrontendAction(std::string &fn) : info_path{ fn }
    {
        // std::cout << "构造函数 my frontend action\n ";
    }
    virtual std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance &compiler,
                          llvm::StringRef in_file)
    {
        // llvm::errs() << "in function FuncFrontendAction::createASTConsumer()\n";
        _rewriter.setSourceMgr(compiler.getSourceManager(),
                               compiler.getLangOpts());
        // std::cout << "befor compiler.getASTContext()\n";
        compiler.getASTContext();
        // std::cout << "after compiler.getASTContext()\n";

        return std::make_unique<ScanASTConsumer>(
            &compiler.getASTContext(), _rewriter, info_path);
    }

    void EndSourceFileAction() override
    {
    }

  private:
    clang::Rewriter _rewriter;
    std::string &info_path;
};

class ScanFactory : public clang::tooling::FrontendActionFactory
{
  public:
    ScanFactory(std::string &fn) : info_path{ fn } {}
    std::unique_ptr<clang::FrontendAction> create() override
    {
        // llvm::errs() << "in function FuncFactory::create()\n";
        return std::make_unique<ScanFrontendAction>(info_path);
    }
  private:
    std::string &info_path;
};
}
#endif