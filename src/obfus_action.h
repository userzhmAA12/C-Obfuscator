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
        : _ctx(ctx), _rewriter(R), info_path(fn), data(), data1(), data2(), count_func(0), count_var(0)
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
                data.insert(std::pair<std::string, std::string>(pre_name, after_name));
                data.insert(std::pair<std::string, std::string>("~"+pre_name, "~" + after_name));
                data1.insert(std::pair<std::string, std::string>(pre_name, after_name));
                data1.insert(std::pair<std::string, std::string>("struct " + pre_name, "struct " + after_name));
                data1.insert(std::pair<std::string, std::string>("struct " + pre_name + "*", "struct " + after_name + "*"));
            }
            else if(op == "Field")
            {
                std::string belong;
                fin >> pre_name >> belong >> after_name;
                std::pair<std::string, std::string> tmp(pre_name, belong);
                data2.insert(std::pair<std::pair<std::string, std::string>, std::string>(tmp, after_name));
            }
            /* else if(op == "Macro")
            {
                fin >> pre_name;
                data[pre_name] = "ignore";
            } */
        }
        fin.close();
    }

    bool VisitFunctionDecl(clang::FunctionDecl *FD) //处理函数名混淆, 要处理返回值类型
    {
        
        // skip the function from included file
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = FD->getLocation();
        clang::SourceLocation N_StartLoc = SM.getSpellingLoc(StartLoc);
        if(SM.isInSystemHeader(StartLoc))
            return true;
        // std::cout << "Start1\n";
        std::string folder_path = getparentdir(info_path);// project path
        if(SM.getFileID(N_StartLoc).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(N_StartLoc)))
                return true;
            std::string loc_file = SM.getFileEntryRefForID(SM.getFileID(N_StartLoc))->getFileEntry().tryGetRealPathName().str();
            if(!is_prefix(loc_file, folder_path))
                return true;
        }
        std::string func_name = FD->getNameAsString();
        std::string ret_type = FD->getReturnType().getAsString();
        
        // std::cout << ret_type << "\n";
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

        /* for (auto it = data1.begin(); it != data1.end(); ++it) 
        {
            std::string pre_name = it->first;
            std::string after_name = it->second;
            ret_type = find_replace(ret_type, pre_name, after_name);
        }
        clang::SourceRange T_SR = FD->getReturnTypeSourceRange();
        _rewriter.ReplaceText(T_SR, ret_type); */
        if(data1.count(ret_type)==1 && data1[ret_type]!="ignore")
        {
            clang::SourceRange T_SR = FD->getReturnTypeSourceRange();
            if(_rewriter.getRewrittenText(T_SR)==ret_type)
                _rewriter.ReplaceText(T_SR, data1[ret_type]);
        }
        return true;
    }

    bool VisitVarDecl(clang::VarDecl *VD) //处理变量名混淆，要处理变量类型
    {
        
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = VD->getLocation();
        clang::SourceLocation N_StartLoc = SM.getSpellingLoc(StartLoc);
        if(SM.isInSystemHeader(StartLoc))
            return true;
        // std::cout << "Start2\n";
        
        std::string folder_path = getparentdir(info_path);// project path
        if(SM.getFileID(N_StartLoc).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(N_StartLoc)))
                return true;
            std::string loc_file = SM.getFileEntryRefForID(SM.getFileID(N_StartLoc))->getFileEntry().tryGetRealPathName().str();
            if(!is_prefix(loc_file, folder_path))
                return true;
        }
        
        std::string var_name = VD->getNameAsString();
        std::string var_type = VD->getType().getAsString();
        
        /* clang::SourceLocation T_StartLoc = VD->getTypeSpecStartLoc();
        clang::SourceLocation T_EndLoc = T_StartLoc.getLocWithOffset(var_type.length()-1);
        clang::SourceRange T_SR(T_StartLoc, T_EndLoc);
        for (auto it = data1.begin(); it != data1.end(); ++it) 
        {
            std::string pre_name = it->first;
            std::string after_name = it->second;
            var_type = find_replace(var_type, pre_name, after_name);
        }
        
        _rewriter.ReplaceText(T_SR, var_type);
        */

        if(var_name.length()==0)return true;
        if(data.count(var_name)==1 && data[var_name]!="ignore")
        {
            clang::SourceRange SR = VD->getSourceRange();
            // clang::SourceLocation N_EndLoc = N_StartLoc.getLocWithOffset(var_name.length()-1);
            // clang::SourceRange SR(N_StartLoc, N_EndLoc);
            if(_rewriter.getRewrittenText(SR)==var_name)
                _rewriter.ReplaceText(SR, data[var_name]);
        }
        if(data1.count(var_type)==1 && data1[var_type]!="ignore")
        {
            clang::SourceLocation T_StartLoc = VD->getTypeSpecStartLoc();
            clang::SourceLocation T_EndLoc = T_StartLoc.getLocWithOffset(var_type.length()-1);
            clang::SourceRange T_SR(T_StartLoc, T_EndLoc);
            if(_rewriter.getRewrittenText(T_SR)==var_type)
                _rewriter.ReplaceText(T_SR, data1[var_type]);
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
        if(expr_name.length()==0)return true;
        
        std::string folder_path = getparentdir(info_path);// project path
        // std::cout << SM.getFileID(DRE->getLocation()).isInvalid() << "\n";
        if(SM.getFileID(N_StartLoc).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(N_StartLoc)))
                return true;
            std::string loc_file1 = SM.getFileEntryRefForID(SM.getFileID(N_StartLoc))->getFileEntry().tryGetRealPathName().str();
            if(!is_prefix(loc_file1, folder_path))
                return true;
        }
        if(SM.getFileID(N_DeclLoc).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(N_DeclLoc)).has_value())
                return true;
            std::string loc_file2 = SM.getFileEntryRefForID(SM.getFileID(N_DeclLoc))->getFileEntry().tryGetRealPathName().str();
            if(!is_prefix(loc_file2, folder_path))
                return true;
        }
        
        // std::cout << expr_name << "\n";
        
        //DRE->dump();
        if(data.count(expr_name)==1&&data[expr_name]!="ignore")
        {
            // std::cout << expr_name << " " << data[expr_name] <<"\n";
            clang::SourceRange SR = DRE->getNameInfo().getSourceRange();
            // SR.getBegin().dump(SM);
            // std::cout << _ctx->getSourceManager().isMacroArgExpansion(SR.getBegin()) << "\n";
            // std::cout << _rewriter.getRewrittenText(SR) << "\n";
            if(_rewriter.getRewrittenText(SR)==expr_name)
                _rewriter.ReplaceText(SR ,data[expr_name]);
        }
        return true;
    }

    bool VisitRecordDecl(clang::RecordDecl *RD) //处理自定义类的类名混淆
    {
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = RD->getLocation();
        if(SM.isInSystemHeader(StartLoc))
            return true;
        std::string folder_path = getparentdir(info_path);// project path
        if(SM.getFileID(StartLoc).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(StartLoc))))
                return true;
            std::string loc_file = SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(StartLoc)))->getFileEntry().tryGetRealPathName().str();
            if(!is_prefix(loc_file, folder_path))
                return true;
        }
        std::string record_name = RD->getNameAsString();
        if(record_name.length()==0)return true;
        if(data1.count(record_name)==1&&data1[record_name]!="ignore")
        {
            clang::SourceLocation EndLoc = StartLoc.getLocWithOffset(record_name.length()-1);
            clang::SourceRange SR(StartLoc, EndLoc);
            if(_rewriter.getRewrittenText(SR)==record_name)
                _rewriter.ReplaceText(SR, data[record_name]);
        }
        return true;
    }
    bool VisitFieldDecl(clang::FieldDecl *FD) // 要处理变量类型
    {
        
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = FD->getLocation();
        clang::SourceLocation N_StartLoc = SM.getSpellingLoc(StartLoc);
        if(SM.isInSystemHeader(StartLoc))
            return true;
        // std::cout << "Start4\n";
        std::string folder_path = getparentdir(info_path);// project path
        if(SM.getFileID(N_StartLoc).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(N_StartLoc)))
                return true;
            std::string loc_file = SM.getFileEntryRefForID(SM.getFileID(N_StartLoc))->getFileEntry().tryGetRealPathName().str();
            if(!is_prefix(loc_file, folder_path))
                return true;
        }
        std::string record_name = FD->getNameAsString();
        std::string var_type = FD->getType().getAsString();
        clang::RecordDecl* parent = FD->getParent();
        std::string parent_name = parent->getNameAsString();
        std::cout << record_name << " " << parent_name << "\n";
        if(record_name.length()==0)return true;

        /* clang::SourceLocation T_StartLoc = FD->getTypeSpecStartLoc();
        clang::SourceLocation T_EndLoc = T_StartLoc.getLocWithOffset(var_type.length()-1);
        clang::SourceRange T_SR(T_StartLoc, T_EndLoc);
        for (auto it = data1.begin(); it != data1.end(); ++it) 
        {
            std::string pre_name = it->first;
            std::string after_name = it->second;
            var_type = find_replace(var_type, pre_name, after_name);
        }
        _rewriter.ReplaceText(T_SR, var_type); */

        if(data2.count(std::pair<std::string, std::string>(record_name, parent_name))==1 && data2[std::pair<std::string, std::string>(record_name, parent_name)]!="ignore")
        {
            // clang::SourceLocation N_EndLoc = N_StartLoc.getLocWithOffset(record_name.length()-1);
            // clang::SourceRange SR(N_StartLoc, N_EndLoc);
            clang:: SourceRange SR = FD->getSourceRange();
            if(_rewriter.getRewrittenText(SR)==record_name)
                _rewriter.ReplaceText(SR, data2[std::pair<std::string, std::string>(record_name, parent_name)]);
        }
        if(data1.count(var_type)==1 && data1[var_type]!="ignore")
        {
            clang::SourceLocation T_StartLoc = FD->getTypeSpecStartLoc();
            clang::SourceLocation T_EndLoc = T_StartLoc.getLocWithOffset(var_type.length()-1);
            clang::SourceRange T_SR(T_StartLoc, T_EndLoc);
            if(_rewriter.getRewrittenText(T_SR)==var_type)
                _rewriter.ReplaceText(T_SR, data1[var_type]);
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
        if(mem_name.length()==0)return true;
        // std::cout << mem_name << "\n";
        std::string folder_path = getparentdir(info_path);// project path
        if(SM.getFileID(N_StartLoc).isInvalid())
            return true;
        else
        {
            
            if(!SM.getFileEntryRefForID(SM.getFileID(N_StartLoc)))
                return true;
            std::string loc_file1 = SM.getFileEntryRefForID(SM.getFileID(N_StartLoc))->getFileEntry().tryGetRealPathName().str();
            if(!is_prefix(loc_file1, folder_path))
                return true;
        }
        if(SM.getFileID(N_DeclLoc).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(N_DeclLoc)))
                return true;
            std::string loc_file2 = SM.getFileEntryRefForID(SM.getFileID(N_DeclLoc))->getFileEntry().tryGetRealPathName().str();
            if(!is_prefix(loc_file2, folder_path))
                return true;
        }
        // std::cout << SM.getFileEntryForID(SM.getFileID(ME->getMemberDecl()->getLocation()))->getName().str() << "\n";
        std::string parent_name;
        if(clang::FieldDecl* FD = llvm::dyn_cast<clang::FieldDecl>(ME->getMemberDecl()))
        {
            parent_name = FD->getParent()->getNameAsString();
        }
        
        if (!can_obfuscate(mem_name))
        {
            return true;
        }
        if(data2.count(std::pair<std::string, std::string>(mem_name, parent_name))==1 && data2[std::pair<std::string, std::string>(mem_name, parent_name)]!="ignore")
        {
            clang::SourceLocation N_EndLoc = N_StartLoc.getLocWithOffset(mem_name.length()-1);
            clang::SourceRange N_SR(N_StartLoc, N_EndLoc);
            if(_rewriter.getRewrittenText(N_SR)==mem_name)
                _rewriter.ReplaceText(N_SR, data2[std::pair<std::string, std::string>(mem_name, parent_name)]);
        }
        return true;
    }
    bool VisitCXXConstructorDecl(clang::CXXConstructorDecl *CCD)
    {
        
        clang::SourceManager &SM = _ctx->getSourceManager();
        if(SM.isInSystemHeader(CCD->getLocation()))
            return true;
        // std::cout << "Start6\n";
        std::string folder_path = getparentdir(info_path);// project path
        if(SM.getFileID(CCD->getLocation()).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(CCD->getLocation()))))
                return true;
            std::string loc_file = SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(CCD->getLocation())))->getFileEntry().tryGetRealPathName().str();
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
    bool VisitInitListExpr(clang::InitListExpr *ILE)
    {
        
        clang::SourceManager &SM = _ctx->getSourceManager();
        if(SM.isInSystemHeader(ILE->getBeginLoc()))
            return true;
        // std::cout << "Start6\n";
        std::string folder_path = getparentdir(info_path);// project path
        if(SM.getFileID(ILE->getBeginLoc()).isInvalid())
            return true;
        else
        {
            if(!SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(ILE->getBeginLoc()))))
                return true;
            std::string loc_file = SM.getFileEntryRefForID(SM.getFileID(SM.getSpellingLoc(ILE->getBeginLoc())))->getFileEntry().tryGetRealPathName().str();
            if(!is_prefix(loc_file, folder_path))
                return true;
        }
        for(auto tmp: ILE->inits())
        {
            clang::SourceLocation startLoc = tmp->getBeginLoc();
            clang::SourceRange SR = tmp->getSourceRange();
            // std::cout << _rewriter.getRewrittenText(SR) << "\n";
            // startLoc.dump(SM);
        }
        return true;
    }
    bool VisitOffsetOfExpr(clang::OffsetOfExpr *OOE) //现在不需要跳宏了也许需要考虑一下
    {
        return true;
    }
  private:
    clang::ASTContext *_ctx;
    clang::Rewriter &_rewriter;
    std::string &info_path;
    std::map<std::string, std::string> data; //储存函数名和变量名的替换信息
    std::map<std::string, std::string> data1;
    std::map<std::pair<std::string, std::string>, std::string> data2;
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
        std::string folder_path = getparentdir(info_path);// project path
        std::cout << "finish2\n";
        for (auto it = SM.fileinfo_begin(); it != SM.fileinfo_end(); ++it) {
            const clang::FileEntryRef& fileEntryRef = it->first;
            
            std::string file_name = fileEntryRef.getFileEntry().tryGetRealPathName().str();
            // std::cout << file_name << "\n";
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