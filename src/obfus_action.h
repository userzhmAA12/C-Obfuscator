#ifndef OBFUS_ACTION_H
#define OBFUS_ACTION_H


#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"

#include <clang/AST/Decl.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>
#include <fstream>

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
    //else if(fn == "at")return false;
    //else if(fn == "size")return false;
    //else if (fn == "append")return false;
    //else if(fn == "remove_prefix")return false;
    return true; 
}

class ObfusASTVisitor : public clang::RecursiveASTVisitor<ObfusASTVisitor>
{
  public:
    explicit ObfusASTVisitor(clang::ASTContext *ctx,
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
                //data1.insert(std::pair<std::string, std::string>(pre_name, after_name));
                //data.insert(std::pair<std::string, std::string>("struct " + pre_name + "*", "struct " + after_name + "*"));
            }
        }
        fin.close();
    }

    bool VisitFunctionDecl(clang::FunctionDecl *func_decl) //处理函数名混淆
    {
        // skip the function from included file
        if (!_ctx->getSourceManager().isInMainFile(func_decl->getLocation()))
            return true;
        if (_ctx->getSourceManager().isInSystemHeader(func_decl->getLocation()))
            return true;
        std::string func_name = func_decl->getNameAsString();
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
                clang::SourceRange SR = func_decl->getNameInfo().getSourceRange();
                std::cout << "func: "<< func_name << " is replaced by " << "Function" << std::to_string(count_func) << "\n";
                _rewriter.ReplaceText(SR, "Function"+std::to_string(count_func));
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
        else if(data[func_name]!="ignore")
        {
            clang::SourceRange SR = func_decl->getNameInfo().getSourceRange();
            _rewriter.ReplaceText(SR, data[func_name]);
        }
        /*
        //处理返回值
        std::string ret_type = func_decl->getReturnType().getAsString();
        std::cout << ret_type << "\n";
        //去除前缀const 和 数组后缀
        int pos = ret_type.find("[");
        if(pos!=std::string::npos)
            ret_type = ret_type.substr(0, pos);
        pos = ret_type.find("const ");
        if(pos==0)
            ret_type = ret_type.substr(pos+6);
        pos = ret_type.find("_Bool");
        while(pos!=std::string::npos)
        {
            ret_type.replace(pos, 5, "bool");
            pos = ret_type.find("_Bool");
        }
        int length = ret_type.length();
        for(auto key : data1)
        {
            int findpos = ret_type.find(key.first);
            while(findpos!=std::string::npos)
            {
                ret_type.replace(findpos, key.first.size(), key.second);
                findpos = ret_type.find(key.first);
            }
        }
        _rewriter.ReplaceText(func_decl->getReturnTypeSourceRange(), ret_type);
        */
        //func_decl->dump();
        return true;
    }

    bool VisitVarDecl(clang::VarDecl *var_decl) //处理变量名混淆
    {
        if (!_ctx->getSourceManager().isInMainFile(var_decl->getLocation()))
            return true;
        if (_ctx->getSourceManager().isInSystemHeader(var_decl->getLocation()))
            return true;
        
        std::string var_name = var_decl->getNameAsString();
        std::string var_type = var_decl->getType().getAsString();
        if(var_name.length()==0)return true;
        if(data.count(var_name)==0)
        {
            ++count_var;
            clang::SourceLocation ST = var_decl->getLocation();
            data.insert(std::pair<std::string, std::string>(var_decl->getNameAsString(), "Variable"+std::to_string(count_var)));
            std::cout << "var: " << var_name << " is replaced by " << "Variable" << std::to_string(count_var) << "\n";
            _rewriter.ReplaceText(ST, var_name.length(), "Variable"+std::to_string(count_var));
            std::ofstream fout(info_path, std::ios::app);
            if(!fout)
            {   
                std::cout << "[error]open file:" << info_path << " failed!\n";
                exit(-1);
            }
            fout << "Var " << var_name << " " << "Variable" << std::to_string(count_var) << std::endl;
            fout.close();
        }
        else if(data[var_name]!="ignore")
        {
            clang::SourceLocation ST = var_decl->getLocation();
            _rewriter.ReplaceText(ST, var_name.length(), data[var_name]);
        }
        /*
        std::cout << var_type << "\n";
        //去除前缀const 和 数组后缀
        int pos = var_type.find("[");
        if(pos!=std::string::npos)
            var_type = var_type.substr(0, pos);
        pos = var_type.find("const ");
        if(pos==0)
            var_type = var_type.substr(pos+6);
        pos = var_type.find("_Bool");
        while(pos!=std::string::npos)
        {
            var_type.replace(pos, 5, "bool");
            pos = var_type.find("_Bool");
        }
        int length = var_type.length();
        for(auto key : data1)
        {
            int findpos = var_type.find(key.first);
            while(findpos!=std::string::npos)
            {
                var_type.replace(findpos, key.first.size(), key.second);
                findpos = var_type.find(key.first);
            }
        }
        
        if(var_type.find("struct")!=0)_rewriter.ReplaceText(var_decl->getTypeSpecStartLoc(), length, var_type);
        */
        //var_decl->dump();
        return true;
    }
    bool VisitDeclRefExpr(clang::DeclRefExpr* s) 
    {
        if (!_ctx->getSourceManager().isInMainFile(s->getLocation()))
            return true;
        if (_ctx->getSourceManager().isInSystemHeader(s->getLocation()))
            return true;
        std::string expr_name = s->getNameInfo().getAsString();
        if(expr_name.length()==0)return true;
        //std::cout << expr_name << "\n";
        //s->dump();
        if(data.count(expr_name)==1&&data[expr_name]!="ignore")
        {
            clang::SourceRange SR = s->getNameInfo().getSourceRange();
            _rewriter.ReplaceText(SR, data[expr_name]);
        }
        return true;
    }

    bool VisitCXXRecordDecl(clang::CXXRecordDecl *RD) //处理自定义类的类名混淆
    {
        if (!_ctx->getSourceManager().isInMainFile(RD->getLocation()))
            return true;
        if (_ctx->getSourceManager().isInSystemHeader(RD->getLocation()))
            return true;
        std::string record_name = RD->getNameAsString();
        if(record_name.length()==0)return true;
        if (data.count(record_name)==0) {
            ++count_var;
            //clang::SourceLocation ST = RD->getLocation();
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
        /*else
        {
            clang::SourceLocation ST = RD->getLocation();
            _rewriter.ReplaceText(ST, record_name.size(), data[record_name]);
        }*/
        return true;
    }
    bool VisitFieldDecl(clang::FieldDecl *FD) 
    {
        if (!_ctx->getSourceManager().isInMainFile(FD->getLocation()))
            return true;
        if (_ctx->getSourceManager().isInSystemHeader(FD->getLocation()))
            return true;
        std::string record_name = FD->getNameAsString();
        std::string var_type = FD->getType().getAsString();
        if(record_name.length()==0)return true;
        if (data.count(record_name)==0) 
        {
            ++count_var;
            clang::SourceLocation ST = FD->getLocation();
            data.insert(std::pair<std::string, std::string>(FD->getNameAsString(), "Variable"+std::to_string(count_var)));
            std::cout << "var: " << record_name << " is repalced by " << "Variable" << std::to_string(count_var) << "\n";
            _rewriter.ReplaceText(ST, record_name.size(), "Variable"+std::to_string(count_var));
            std::ofstream fout(info_path, std::ios::app);
            if(!fout)
            {   
                std::cout << "[error]open file:" << info_path << " failed!\n";
                exit(-1);
            }
            fout << "Var " << record_name << " " << "Variable" << std::to_string(count_var) << "\n";
            fout.close();
        }
        else if(data[record_name]!="ignore")
        {
            clang::SourceLocation ST = FD->getLocation();
            _rewriter.ReplaceText(ST, record_name.size(), data[record_name]);
        }
        /*
        //std::cout << var_type << "\n";
        //去除前缀const 和 数组后缀
        int pos = var_type.find("[");
        if(pos!=std::string::npos)
            var_type = var_type.substr(0, pos);
        pos = var_type.find("const ");
        if(pos==0)
            var_type = var_type.substr(pos+6);
        pos = var_type.find("_Bool");
        while(pos!=std::string::npos)
        {
            var_type.replace(pos, 5, "bool");
            pos = var_type.find("_Bool");
        }
        int length = var_type.length();
        for(auto key : data1)
        {
            int findpos = var_type.find(key.first);
            while(findpos!=std::string::npos)
            {
                var_type.replace(findpos, key.first.size(), key.second);
                findpos = var_type.find(key.first);
            }
        }
        
        _rewriter.ReplaceText(FD->getTypeSpecStartLoc(), length, var_type);
       */ 
        return true;
    }
    bool VisitMemberExpr(clang::MemberExpr *ME)
    {
        if (!_ctx->getSourceManager().isInMainFile(ME->getMemberLoc()))
            return true;
        if (_ctx->getSourceManager().isInSystemHeader(ME->getMemberLoc()))
            return true;
        if (_ctx->getSourceManager().isInSystemHeader(ME->getMemberDecl()->getLocation()))
            return true;
        std::cout << ME->getMemberNameInfo().getAsString() << "\n";
        std::string mem_name = ME->getMemberNameInfo().getAsString();
        if(mem_name.length()==0)return true;
        if (!can_obfuscate(mem_name))
        {
            return true;
        }
        if(data.count(mem_name)==1&&data[mem_name]!="ignore")
        {
            _rewriter.ReplaceText(ME->getMemberLoc(), ME->getMemberNameInfo().getAsString().length(), data[mem_name]);
        }
        return true;
    }
    bool VisitCXXConstructorDecl(clang::CXXConstructorDecl *CCD)
    {
        if (!_ctx->getSourceManager().isInMainFile(CCD->getLocation()))
            return true;
        if (_ctx->getSourceManager().isInSystemHeader(CCD->getLocation()))
            return true;
        for (auto initializer : CCD->inits()) {
            if (initializer->isAnyMemberInitializer())
            {
                std::string mem_name = initializer->getMember()->getNameAsString();
                if(data.count(mem_name)==1&&data[mem_name]!="ignore")
                {
                    //std::cout << mem_name << "\n";
                    clang::SourceLocation startLoc = initializer->getMemberLocation();
                    if(startLoc.isValid())
                    {
                        clang::SourceLocation endLoc = startLoc.getLocWithOffset(mem_name.size()-1);
                        _rewriter.ReplaceText(clang::SourceRange(startLoc, endLoc), data[mem_name]);
                    }
                }
            }
        }
        return true;
    }
  private:
    clang::ASTContext *_ctx;
    clang::Rewriter &_rewriter;
    std::string &info_path;
    std::map<std::string, std::string> data; //储存函数名和变量名的替换信息
    //std::map<std::string, std::string> data1; //储存类名的替换信息
    int count_func;
    int count_var;
    
};

class ObfusASTConsumer : public clang::ASTConsumer
{
  public:
    explicit ObfusASTConsumer(clang::ASTContext *ctx,
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
    ObfusASTVisitor _visitor;
};

class ObfusFrontendAction : public clang::ASTFrontendAction
{
  public:
    ObfusFrontendAction(std::string &fn) : info_path{ fn }
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

        return std::make_unique<ObfusASTConsumer>(
            &compiler.getASTContext(), _rewriter, info_path);
    }

    void EndSourceFileAction() override
    {
        clang::SourceManager &SM = _rewriter.getSourceMgr();
        // llvm::errs() << "** EndSourceFileAction for: "
        //              << SM.getFileEntryForID(SM.getMainFileID())->getName()
        //              << "\n";
        //
        std::error_code ec;
        std::string file_name =
            SM.getFileEntryForID(SM.getMainFileID())->getName().str();
        // std::cout << "** file_name: " << file_name << "\n";
        std::string repalced;
        repalced = file_name.substr(file_name.rfind("."), file_name.length());
        std::cout << repalced << "\n";
        replace_suffix(file_name, "-obfuscated"+repalced);
        llvm::raw_fd_stream fd(file_name, ec);
        // _rewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
        _rewriter.getEditBuffer(SM.getMainFileID()).write(fd);
    }

  private:
    clang::Rewriter _rewriter;
    std::string &info_path;
};

class ObfusFactory : public clang::tooling::FrontendActionFactory
{
  public:
    ObfusFactory(std::string &fn) : info_path{ fn } {}
    std::unique_ptr<clang::FrontendAction> create() override
    {
        // llvm::errs() << "in function FuncFactory::create()\n";
        return std::make_unique<ObfusFrontendAction>(info_path);
    }
  private:
    std::string &info_path;
};
}
#endif