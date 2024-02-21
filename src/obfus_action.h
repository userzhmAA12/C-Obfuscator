#ifndef OBFUS_ACTION_H
#define OBFUS_ACTION_H

#include "scan_action.h"
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

    bool VisitFunctionDecl(clang::FunctionDecl *FD) //处理函数名混淆
    {
        
        // skip the function from included file
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = FD->getLocation();
        clang::SourceLocation N_StartLoc = SM.getSpellingLoc(StartLoc);
        if(SM.isInSystemHeader(StartLoc))
            return true;
        // std::cout << "Start1\n";
        std::filesystem::path path(info_path);// variable_replace.txt path
        std::filesystem::path folder_path = path.parent_path();// project path
        if(SM.getFileID(N_StartLoc).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(N_StartLoc)))
                return true;
            std::string loc_file = SM.getFileEntryRefForID(SM.getFileID(N_StartLoc))->getName().str();
            if(!is_prefix(loc_file, folder_path))
                return true;
        }
        std::string func_name = FD->getNameAsString();
        if(func_name.length()==0)return true;
        if (!can_obfuscate(func_name))
        {
            return true;
        }
        if(data.count(func_name)==1 && data[func_name]!="ignore")
        {
            // std::cout << func_name << "\n";
            clang::SourceLocation N_EndLoc = N_StartLoc.getLocWithOffset(func_name.length()-1);
            clang::SourceRange SR(N_StartLoc, N_EndLoc);
            if(_rewriter.getRewrittenText(SR)==func_name)
                _rewriter.ReplaceText(SR, data[func_name]);
        }
        return true;
    }

    bool VisitVarDecl(clang::VarDecl *VD) //处理变量名混淆
    {
        
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = VD->getLocation();
        clang::SourceLocation N_StartLoc = SM.getSpellingLoc(StartLoc);
        if(SM.isInSystemHeader(StartLoc))
            return true;
        // std::cout << "Start2\n";
        
        std::filesystem::path path(info_path);// variable_replace.txt path
        std::filesystem::path folder_path = path.parent_path();// project path
        if(SM.getFileID(N_StartLoc).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(N_StartLoc)))
                return true;
            std::string loc_file = SM.getFileEntryRefForID(SM.getFileID(N_StartLoc))->getName().str();
            if(!is_prefix(loc_file, folder_path))
                return true;
        }
        
        std::string var_name = VD->getNameAsString();
        std::string var_type = VD->getType().getAsString();
        if(var_name.length()==0)return true;
        if(data.count(var_name)==1 && data[var_name]!="ignore")
        {
            clang::SourceLocation N_EndLoc = N_StartLoc.getLocWithOffset(var_name.length()-1);
            clang::SourceRange SR(N_StartLoc, N_EndLoc);
            if(_rewriter.getRewrittenText(SR)==var_name)
                _rewriter.ReplaceText(SR, data[var_name]);
        }
        return true;
    }
    bool VisitDeclRefExpr(clang::DeclRefExpr* DRE) 
    {
        
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = DRE->getLocation();
        clang::SourceLocation N_StartLoc = SM.getSpellingLoc(StartLoc);
        clang::SourceLocation DeclLoc = DRE->getDecl()->getLocation();
        clang::SourceLocation N_DeclLoc = SM.getSpellingLoc(DeclLoc);
        if(SM.isInSystemHeader(StartLoc))
            return true;
        if(SM.isInSystemHeader(DeclLoc))
            return true;
        // std::cout << "Start3\n";
        std::string expr_name = DRE->getNameInfo().getAsString();
        
        std::filesystem::path path(info_path);// variable_replace.txt path
        std::filesystem::path folder_path = path.parent_path();// project path
        // std::cout << SM.getFileID(DRE->getLocation()).isInvalid() << "\n";
        if(SM.getFileID(N_StartLoc).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(N_StartLoc)))
                return true;
            std::string loc_file1 = SM.getFileEntryRefForID(SM.getFileID(N_StartLoc))->getName().str();
            if(!is_prefix(loc_file1, folder_path))
                return true;
        }
        if(SM.getFileID(N_DeclLoc).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(N_DeclLoc)).has_value())
                return true;
            std::string loc_file2 = SM.getFileEntryRefForID(SM.getFileID(N_DeclLoc))->getName().str();
            if(!is_prefix(loc_file2, folder_path))
                return true;
        }
        
        if(expr_name.length()==0)return true;
        // std::cout << expr_name << "\n";
        //DRE->dump();
        if(data.count(expr_name)==1&&data[expr_name]!="ignore")
        {
            // std::cout << expr_name << " " << data[expr_name] <<"\n";
            clang::SourceRange SR = DRE->getNameInfo().getSourceRange();
            // SR.getBegin().dump(SM);
            clang::SourceLocation N_EndLoc = N_StartLoc.getLocWithOffset(expr_name.size()-1);
            clang::SourceRange N_SR(N_StartLoc, N_EndLoc);
            // std::cout << _ctx->getSourceManager().isMacroArgExpansion(SR.getBegin()) << "\n";
            // std::cout << _rewriter.getRewrittenText(N_SR) << "\n";
            if(_rewriter.getRewrittenText(N_SR)==expr_name)
                _rewriter.ReplaceText(N_SR ,data[expr_name]);
        }
        return true;
    }

    bool VisitCXXRecordDecl(clang::CXXRecordDecl *RD) //处理自定义类的类名混淆
    {
        return true;
    }
    bool VisitFieldDecl(clang::FieldDecl *FD) 
    {
        
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = FD->getLocation();
        clang::SourceLocation N_StartLoc = SM.getSpellingLoc(StartLoc);
        if(SM.isInSystemHeader(StartLoc))
            return true;
        // std::cout << "Start4\n";
        std::filesystem::path path(info_path);// variable_replace.txt path
        std::filesystem::path folder_path = path.parent_path();// project path
        if(SM.getFileID(N_StartLoc).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(N_StartLoc)))
                return true;
            std::string loc_file = SM.getFileEntryRefForID(SM.getFileID(N_StartLoc))->getName().str();
            if(!is_prefix(loc_file, folder_path))
                return true;
        }
        std::string record_name = FD->getNameAsString();
        std::string var_type = FD->getType().getAsString();
        if(record_name.length()==0)return true;
        if(data.count(record_name)==1 && data[record_name]!="ignore")
        {
            clang::SourceLocation N_EndLoc = N_StartLoc.getLocWithOffset(record_name.length()-1);
            clang::SourceRange SR(N_StartLoc, N_EndLoc);
            if(_rewriter.getRewrittenText(SR)==record_name)
                _rewriter.ReplaceText(SR, data[record_name]);
        }
        return true;
    }
    bool VisitMemberExpr(clang::MemberExpr *ME)
    {
        
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = ME->getMemberLoc();
        clang::SourceLocation N_StartLoc = SM.getSpellingLoc(StartLoc);
        clang::SourceLocation DeclLoc = ME->getMemberDecl()->getLocation();
        clang::SourceLocation N_DeclLoc = SM.getSpellingLoc(DeclLoc);
        if(SM.isInSystemHeader(StartLoc))
            return true;
        if(SM.isInSystemHeader(DeclLoc))
            return true;
        
        // std::cout << "Start5\n";
        // std::cout << SM.getSpellingLineNumber(StartLoc) << "\n";
        std::string mem_name = ME->getMemberNameInfo().getAsString();
        // std::cout << mem_name << "\n";
        std::filesystem::path path(info_path);// variable_replace.txt path
        std::filesystem::path folder_path = path.parent_path();// project path
        if(SM.getFileID(N_StartLoc).isInvalid())
            return true;
        else
        {
            
            if(!SM.getFileEntryRefForID(SM.getFileID(N_StartLoc)))
                return true;
            std::string loc_file1 = SM.getFileEntryRefForID(SM.getFileID(N_StartLoc))->getName().str();
            if(!is_prefix(loc_file1, folder_path))
                return true;
        }
        if(SM.getFileID(N_DeclLoc).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(N_DeclLoc)))
                return true;
            std::string loc_file2 = SM.getFileEntryRefForID(SM.getFileID(N_DeclLoc))->getName().str();
            if(!is_prefix(loc_file2, folder_path))
                return true;
        }
        // std::cout << SM.getFileEntryForID(SM.getFileID(ME->getMemberDecl()->getLocation()))->getName().str() << "\n";
        
        if(mem_name.length()==0)return true;
        if (!can_obfuscate(mem_name))
        {
            return true;
        }
        if(data.count(mem_name)==1&&data[mem_name]!="ignore")
        {
            // StartLoc.dump(SM);
            // std::cout << mem_name << "\n";
            // std::cout << SM.getFileEntryForID(SM.getFileID(N_StartLoc))->getName().str() << "\n";
            clang::SourceLocation N_EndLoc = N_StartLoc.getLocWithOffset(mem_name.size()-1);
            clang::SourceRange N_SR(N_StartLoc, N_EndLoc);
            // std::cout << _ctx->getSourceManager().isMacroArgExpansion(SR.getBegin()) << "\n";
            // std::cout << _rewriter.getRewrittenText(N_SR) << "\n";
            if(_rewriter.getRewrittenText(N_SR)==mem_name)
                _rewriter.ReplaceText(N_SR, data[mem_name]);
        }
        return true;
    }
    bool VisitCXXConstructorDecl(clang::CXXConstructorDecl *CCD)
    {
        
        clang::SourceManager &SM = _ctx->getSourceManager();
        if(SM.isInSystemHeader(CCD->getLocation()))
            return true;
        // std::cout << "Start6\n";
        std::filesystem::path path(info_path);// variable_replace.txt path
        std::filesystem::path folder_path = path.parent_path();// project path
        if(SM.getFileID(CCD->getLocation()).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(CCD->getLocation()))))
                return true;
            std::string loc_file = SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(CCD->getLocation())))->getName().str();
            if(!is_prefix(loc_file, folder_path))
                return true;
        }
        clang::SourceLocation Loc = CCD->getBeginLoc();
        // clang::SourceManager &SM = _ctx->getSourceManager();
        // Loc.dump(SM);
        for (auto initializer : CCD->inits()) {
            if (initializer->isAnyMemberInitializer())
            {
                std::string mem_name = initializer->getMember()->getNameAsString();
                // std::cout << mem_name << "\n";
                if(data.count(mem_name)==1&&data[mem_name]!="ignore")
                {
                    //std::cout << mem_name << "\n";
                    clang::SourceLocation startLoc = initializer->getMemberLocation();
                    
                    if(startLoc.isValid())
                    {
                        // startLoc.dump(SM);
                        clang::SourceLocation endLoc = startLoc.getLocWithOffset(mem_name.size()-1);
                        clang::SourceRange N_SR = clang::SourceRange(startLoc, endLoc);
                        if(_rewriter.getRewrittenText(N_SR)==mem_name)
                            _rewriter.ReplaceText(N_SR, data[mem_name]);
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
        std::filesystem::path path(info_path);// variable_replace.txt path
        std::filesystem::path folder_path = path.parent_path();// project path
        std::cout << "finish2\n";
        for (auto it = SM.fileinfo_begin(); it != SM.fileinfo_end(); ++it) {
            const clang::FileEntryRef& fileEntryRef = it->first;
            std::string file_name = fileEntryRef.getName().str();
            if(is_prefix(file_name, folder_path))
            {
                std::string repalced;
                repalced = file_name.substr(file_name.rfind("."), file_name.length());
                // std::cout << repalced << "\n";
                replace_suffix(file_name, "-obfuscated"+repalced);
                llvm::raw_fd_stream fd(file_name, ec);
                // _rewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
                _rewriter.getEditBuffer(SM.translateFile(fileEntryRef)).write(fd);
            }
        }
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