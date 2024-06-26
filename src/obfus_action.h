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
                            std::string &fn1,
                            std::string &fn2)
        : _ctx(ctx), _rewriter(R), info_path(fn1), func_path(fn2), data(), data1(), data2(), data3(),count_func(0), count_var(0), flag(false)
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
                // data1.insert(std::pair<std::string, std::string>("struct " + pre_name, "struct " + after_name));
                // data1.insert(std::pair<std::string, std::string>("struct " + pre_name + "*", "struct " + after_name + "*"));
            }
            else if(op == "Field")
            {
                std::string belong;
                fin >> pre_name >> belong >> after_name;
                std::pair<std::string, std::string> tmp1(pre_name, belong);
                std::pair<std::string, std::string> tmp2(pre_name, "struct " + belong);
                if(data2.count(tmp1)==0)data2.insert(std::pair<std::pair<std::string, std::string>, std::string>(tmp1, after_name));
                if(data2.count(tmp2)==0)data2.insert(std::pair<std::pair<std::string, std::string>, std::string>(tmp2, after_name));
            }
            else if(op == "Anonymous")
            {
                std::string belong1, belong2; //belong1是最近的有名结构体名，belong2是匿名结构体的第一个变量实例名
                fin >> pre_name >> belong1 >> belong2 >> after_name;
                std::string belong = belong1 + " " + belong2;
                std::pair<std::string, std::string> tmp(pre_name, belong);
                if(data2.count(tmp)==0)data2.insert(std::pair<std::pair<std::string, std::string>, std::string>(tmp, after_name));
            }
            else if(op == "Typedef")
            {
                fin >> pre_name >> after_name;
                count_var++;
                // data.insert(std::pair<std::string, std::string>(pre_name, after_name));
                // data.insert(std::pair<std::string, std::string>("~"+pre_name, "~" + after_name));
                data1.insert(std::pair<std::string, std::string>(pre_name, "struct " + after_name));
            }
        }
        fin.close();
    }

    bool VisitFunctionDecl(clang::FunctionDecl *FD) //处理函数名混淆, 要处理返回值类型
    {
        
        // skip the function from included file
        if(!flag)
            return true;
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = FD->getLocation();
        clang::SourceLocation N_StartLoc = SM.getSpellingLoc(StartLoc);
        if(SM.isInSystemHeader(StartLoc))
            return true;
        if(!SM.isInMainFile(StartLoc))
            return true;
        // std::cout << "Start1\n";
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

            _rewriter.ReplaceText(SR, data[func_name]);
        }
        
        ret_type = type_change(ret_type);
        std::string replace_type = ret_type;
        for (auto it = data1.begin(); it != data1.end(); ++it) 
        {
            std::string pre_name = it->first;
            std::string after_name = it->second;
            replace_type = find_replace(replace_type, pre_name, after_name);
        }
        size_t pos = 0;
        while ((pos = replace_type.find("enum ", pos)) != std::string::npos)
        {
            if ((pos == 0 || (!std::isalnum(replace_type[pos - 1]) && replace_type[pos - 1] != '_')))
            {
                size_t end = replace_type.find(" ", pos + 5);
                if (end == std::string::npos)
                {
                    replace_type.replace(pos, replace_type.length() - pos, "int");
                }
                else
                    replace_type.replace(pos, end - pos - 1, "int");
            }
            else
            {
                ++pos;
            }
        }
        clang::SourceRange T_SR = FD->getReturnTypeSourceRange();
        if(ret_type != replace_type)
            _rewriter.ReplaceText(T_SR, replace_type);

        if (FD->hasBody())
        {
            std::ofstream fout(func_path, std::ios::app);
            if(!fout)
            {   
                std::cout << "[error]open file:" << func_path << " failed!\n";
                exit(-1);
            }
            fout << replace_type << " " << data[func_name] << "(";
            bool flag = false;
            // 获取函数参数列表
            for (const clang::ParmVarDecl *Param : FD->parameters())
            {
                if(flag==false)
                {
                    flag = true;
                }
                else
                {
                    fout << ", ";
                }
                std::string p_type = Param->getType().getAsString();
                std::string p_name = Param->getNameAsString();
                std::string replace_type = p_type;
                for (auto it = data1.begin(); it != data1.end(); ++it)
                {
                    std::string pre_name = it->first;
                    std::string after_name = it->second;
                    replace_type = find_replace(replace_type, pre_name, after_name);
                }
                if(data.count(p_name)==1)
                    p_name = data[p_name];
                fout << replace_type << " " << p_name;
            }
            fout << ");\n";
            fout.close();
        }
        return true;
    }

    bool VisitVarDecl(clang::VarDecl *VD) //处理变量名混淆，要处理变量类型
    {
        if(!flag)
            return true;
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = VD->getLocation();
        clang::SourceLocation N_StartLoc = SM.getSpellingLoc(StartLoc);
        if(SM.isInSystemHeader(StartLoc))
            return true;
        // std::cout << "Start2\n";
        
        
        std::string var_name = VD->getNameAsString();
        clang::QualType QT = VD->getType();
        std::string var_type = QT.getAsString();
        if(const clang::TypedefType* TT = QT->getAs<clang::TypedefType>())
        {
            clang::TypedefDecl *Typedef= clang::dyn_cast<clang::TypedefDecl>(TT->getDecl());
            if(isFunctionPointerTypedef(Typedef))
                return true;
            
        }
        var_type = type_change(var_type);
        std::string replace_type = var_type;
        // std::cout << var_name << " " << var_type << "\n";
        // std::cout << getDecl_realType(VD->getType()).getAsString() <<"\n";
        for (auto it = data1.begin(); it != data1.end(); ++it) 
        {
            std::string pre_name = it->first;
            std::string after_name = it->second;
            replace_type = find_replace(replace_type, pre_name, after_name);
        }
        size_t pos = 0;
        while ((pos = replace_type.find("enum ", pos)) != std::string::npos)
        {
            if ((pos == 0 || (!std::isalnum(replace_type[pos - 1]) && replace_type[pos - 1] != '_')))
            {
                size_t end = replace_type.find(" ", pos + 5);
                if (end == std::string::npos)
                {
                    replace_type.replace(pos, replace_type.length() - pos, "int");
                }
                else
                    replace_type.replace(pos, end - pos - 1, "int");
            }
            else
            {
                ++pos;
            }
        }
        if(data.count(var_name)==1 && data[var_name]!="ignore")
        {
            clang::SourceLocation N_EndLoc = N_StartLoc.getLocWithOffset(var_name.length()-1);
            clang::SourceRange SR(N_StartLoc, N_EndLoc);
            if(_rewriter.getRewrittenText(SR)==var_name)
                _rewriter.ReplaceText(SR, data[var_name]);
        }
        clang::SourceLocation T_StartLoc = VD->getTypeSpecStartLoc();
        clang::SourceLocation T_EndLoc = T_StartLoc.getLocWithOffset(var_type.length()-1);
        clang::SourceRange T_SR(T_StartLoc, T_EndLoc);
        _rewriter.ReplaceText(T_SR, replace_type);
        
        return true;
    }
    bool VisitDeclRefExpr(clang::DeclRefExpr* DRE) 
    {
        if(!flag)
            return true;
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
        
        if(data.count(expr_name)==1&&data[expr_name]!="ignore")
        {
            clang::SourceRange SR = DRE->getNameInfo().getSourceRange();
            if(_rewriter.getRewrittenText(SR)==expr_name)
                _rewriter.ReplaceText(SR ,data[expr_name]);
        }
        if(const clang::EnumConstantDecl *EnumConst = clang::dyn_cast<clang::EnumConstantDecl>(DRE->getDecl())){
            int value = getEnumValue(EnumConst);
            std::string str1=std::to_string(value);
            auto SR= DRE->getSourceRange();//(const_cast<clang::DeclRefExpr *>(DRE));
            std::cout<<get_stmt_string(DRE)<<"  replaceEnum slice insert "<<str1<<"\n";
            if (SR.isValid()) {
                clang::SourceLocation EndLoc = clang::Lexer::getLocForEndOfToken(SR.getEnd(), 0, _rewriter.getSourceMgr(), _rewriter.getLangOpts());
                if (EndLoc.isValid()) {
                    std::string TokenText = clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(EndLoc), _rewriter.getSourceMgr(),_rewriter.getLangOpts()).str();
                    if (TokenText == ")" || TokenText == "]") {
                        ;//条件里 || &&  == 目前看没问题
                    }
                    else SR.setEnd(EndLoc.getLocWithOffset(-1));
                }
                clang::SourceManager &SM = _rewriter.getSourceMgr();
                // llvm::outs()<< "????SourceRange: " << SR.getBegin().printToString(SM) << " - " << SR.getEnd().printToString(SM) << "\n";
                // _rewriter.RemoveText(SR);
                // _rewriter.InsertText(SR.getEnd(),str1);
                _rewriter.ReplaceText(SR,str1);
            }
        }
        return true;
    }

    bool VisitRecordDecl(clang::RecordDecl *RD) //处理自定义类的类名混淆
    {
        return true;
    }
    bool VisitFieldDecl(clang::FieldDecl *FD) // 要处理变量类型
    {
        if(!flag)
            return true;
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = FD->getLocation();
        if(SM.isInSystemHeader(StartLoc))
            return true;
        clang::QualType fieldType = FD->getType();
        const clang::RecordType *recordType = fieldType->getAsStructureType();
        if (recordType == nullptr)
        {
            recordType = fieldType->getAsUnionType();
        }
        if (recordType)
        {
            const clang::RecordDecl *RD = recordType->getDecl();
            // std::cout << FD->getNameAsString() << "\n";
            if (data3.count(RD) == 0)
            {
                const clang::DeclContext *parent = RD->getParent();
                while (parent)
                {
                    if (clang::isa<clang::RecordDecl>(parent))
                    {
                        const clang::RecordDecl *N_RD = llvm::dyn_cast<clang::RecordDecl>(parent);
                        std::string tmp_name = N_RD->getNameAsString();
                        if(data3.count(N_RD)!=0)
                        {
                            tmp_name = data3[N_RD];
                        }
                        if (!tmp_name.empty())
                        {
                            std::string alias = tmp_name + " " + FD->getNameAsString();
                            // std::cout << alias << "\n";
                            data3.insert(std::pair<const clang::RecordDecl *, std::string>(RD, alias));
                            break;
                        }
                    }
                    parent = parent->getParent();
                }
            }
        }
        return true;
    }
    bool VisitMemberExpr(clang::MemberExpr *ME)
    {
        if(!flag)
            return true;
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
        // std::cout << SM.getFileEntryForID(SM.getFileID(ME->getMemberDecl()->getLocation()))->getName().str() << "\n";
        std::string parent_name;
        if(clang::FieldDecl* FD = llvm::dyn_cast<clang::FieldDecl>(ME->getMemberDecl()))
        {
            parent_name = FD->getParent()->getNameAsString();
            // 
            if(parent_name.length()==0)
            {
                clang::RecordDecl *RD = FD->getParent();
                if(data3.count(RD)!=0)
                {
                    parent_name = data3[RD];
                }
            }
            // std::cout << FD->getNameAsString() << " " << parent_name << "\n";
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
        if(!flag)
            return true;
        clang::SourceManager &SM = _ctx->getSourceManager();
        if(SM.isInSystemHeader(CCD->getLocation()))
            return true;
        // std::cout << "Start6\n";
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
        if(!flag)
            return true;
        clang::SourceManager &SM = _ctx->getSourceManager();
        if(SM.isInSystemHeader(ILE->getBeginLoc()))
            return true;
        // std::cout << "Start6\n";
        std::string parent_name = ILE->getType().getAsString();
        for(auto tmp: ILE->inits())
        {
            clang::SourceLocation startLoc = tmp->getBeginLoc();
            clang::SourceRange SR = tmp->getSourceRange();
            std::string init_name = _rewriter.getRewrittenText(SR);
            if(init_name[0]=='.')
            {
                size_t dotPos = init_name.find('.');
                size_t equalPos = init_name.find('=');
                init_name = init_name.substr(dotPos + 1, equalPos - dotPos - 1);
                if (!init_name.empty()) 
                {
                    init_name.erase(0, init_name.find_first_not_of(" "));
                    init_name.erase(init_name.find_last_not_of(" ") + 1);
                }
                if (data2.count(std::pair<std::string, std::string>(init_name, parent_name)) == 1 && data2[std::pair<std::string, std::string>(init_name, parent_name)] != "ignore")
                {
                    clang::SourceLocation endLoc = startLoc.getLocWithOffset(init_name.size());
                    clang::SourceRange N_SR = clang::SourceRange(startLoc, endLoc);
                    _rewriter.ReplaceText(N_SR, "." + data2[std::pair<std::string, std::string>(init_name, parent_name)]);
                }
            }
        }
        return true;
    }
    bool VisitOffsetOfExpr(clang::OffsetOfExpr *OOE) //现在不需要跳宏了也许需要考虑一下
    {
        return true;
    }
    bool VisitCStyleCastExpr(clang::CStyleCastExpr *CSCE) // 处理C风格强制类型转换
    {

        if(!flag)
            return true;
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = CSCE->getBeginLoc();
        clang::SourceLocation N_StartLoc = SM.getSpellingLoc(StartLoc);
        if(SM.isInSystemHeader(StartLoc))
            return true;
        std::string cast_name = CSCE->getTypeAsWritten().getAsString();
        cast_name = type_change(cast_name);
        std::string replace_name = cast_name;
        for (auto it = data1.begin(); it != data1.end(); ++it) 
        {
            std::string pre_name = it->first;
            std::string after_name = it->second;
            replace_name = find_replace(replace_name, pre_name, after_name);
        }
        size_t pos = 0;
        while ((pos = replace_name.find("enum ", pos)) != std::string::npos)
        {
            if ((pos == 0 || (!std::isalnum(replace_name[pos - 1]) && replace_name[pos - 1] != '_')))
            {
                size_t end = replace_name.find(" ", pos + 5);
                if (end == std::string::npos)
                {
                    replace_name.replace(pos, replace_name.length() - pos, "int");
                }
                else
                    replace_name.replace(pos, end - pos - 1, "int");
            }
            else
            {
                ++pos;
            }
        }

        clang::SourceLocation N_EndLoc = N_StartLoc.getLocWithOffset(cast_name.length()+1);
        clang::SourceRange T_SR(N_StartLoc, N_EndLoc);
        if(cast_name!=replace_name)
        {
            _rewriter.ReplaceText(T_SR, "(" + replace_name + ")");
        }
        return true;
    }
    bool VisitTypedefDecl(clang::TypedefDecl *TD)
    {
        if(!flag)
            return true;
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = TD->getBeginLoc();

        if(SM.isInSystemHeader(StartLoc))
            return true;

        clang::QualType TypedefType = TD->getUnderlyingType();
        if (TypedefType->isFunctionPointerType()) //函数指针typedef
        {
            if (const auto *FunctionPointerType = TypedefType->getAs<clang::PointerType>())
            {
                const auto *FunctionType = FunctionPointerType->getPointeeType()->getAs<clang::FunctionProtoType>();
                if (!FunctionType)
                    return true;
                std::string s = "typedef ";
                std::string ret_type = FunctionType->getReturnType().getAsString();
                ret_type = type_change(ret_type);
                std::string replace_type = ret_type;
                for (auto it = data1.begin(); it != data1.end(); ++it)
                {
                    std::string pre_name = it->first;
                    std::string after_name = it->second;
                    replace_type = find_replace(replace_type, pre_name, after_name);
                }
                s += replace_type + " (*";

                s += TD->getNameAsString() + ")(";
                bool f_t = false;
                for (const auto &Param : FunctionType->param_types())
                {
                    if (f_t)
                        s += ",";
                    else
                        f_t = true;
                    std::string param_type = Param.getAsString();
                    param_type = type_change(param_type);
                    std::string replace_type = param_type;
                    for (auto it = data1.begin(); it != data1.end(); ++it)
                    {
                        std::string pre_name = it->first;
                        std::string after_name = it->second;
                        replace_type = find_replace(replace_type, pre_name, after_name);
                    }
                    s += replace_type;
                }
                s += ")";
                // std::cout << s << "\n";
                clang::SourceRange SR = TD->getSourceRange();
                /*SR.dump(SM);
                std::cout << _rewriter.getRewrittenText(SR) << "\n";
                _rewriter.RemoveText(SR);
                _rewriter.ReplaceText(SR, s);*/
            }
        }
        else
        {
            std::string after_name = TD->getNameAsString();
            std::string before_name = TD->getUnderlyingType().getAsString();
            // std::cout << after_name << " " << before_name << "\n";
            const clang::TypeSourceInfo *TInfo = TD->getTypeSourceInfo();
            if (!TInfo)
                return true;
            clang::QualType QT = TInfo->getType();
            const clang::Type *T = QT.getTypePtrOrNull();
            if (!T)
                return true;

            // 检查类型是否为 record 类型
            if (const clang::RecordType *RT = T->getAs<clang::RecordType>())
            {
                // 获取 record 类型对应的 recorddecl 节点
                const clang::RecordDecl *RD = RT->getDecl();
                if (RD)
                {
                    before_name = RD->getNameAsString();
                    if (before_name.length() == 0) // 是匿名结构体
                    {
                        if (data3.count(RD) == 0)
                            data3.insert(std::pair<const clang::RecordDecl *, std::string>(RD, after_name));
                    }
                }
            }
            // std::cout << before_name << " " << after_name << "\n";
            if (data1.count(before_name) != 0)
            {
                if (data1.count(after_name) == 0)
                    data1.insert(std::pair<std::string, std::string>(after_name, data1[before_name]));
            }
            else if (before_name.length() != 0)
            {
                data1.insert(std::pair<std::string, std::string>(after_name, before_name));
            }
            clang::SourceRange SR = TD->getSourceRange();
            // _rewriter.RemoveText(SR);
        }
        return true;
    }
    bool VisitUnaryExprOrTypeTraitExpr(clang::UnaryExprOrTypeTraitExpr *UEOTTE)
    {
        if(!flag)
            return true;
        clang::SourceManager &SM = _ctx->getSourceManager();
        clang::SourceLocation StartLoc = UEOTTE->getArgumentTypeInfo()->getTypeLoc().getBeginLoc();
        if(SM.isInSystemHeader(StartLoc))
            return true;
        if(!UEOTTE->isArgumentType())
            return true;
        std::string argu_name = UEOTTE->getArgumentType().getAsString();
        if(data1.count(argu_name)!=0)
        {
            clang::SourceLocation EndLoc = StartLoc.getLocWithOffset(argu_name.length()-1);
            clang::SourceRange SR(StartLoc, EndLoc);
            _rewriter.ReplaceText(SR, data1[argu_name]);
        }
        return true;
    }
    void setFlag()
    {
        flag = true;
    }
  private:
    clang::ASTContext *_ctx;
    clang::Rewriter &_rewriter;
    std::string &info_path;
    std::string &func_path;
    std::map<std::string, std::string> data; //储存函数名和变量名的替换信息
    std::map<std::string, std::string> data1; //储存结构体名混淆信息
    std::map<std::pair<std::string, std::string>, std::string> data2; //储存结构体成员混淆信息
    std::map<const clang::RecordDecl*, std::string> data3; //储存匿名结构体相关信息
    int count_func;
    int count_var;
    bool flag;
};

class ObfusASTConsumer : public clang::ASTConsumer
{
  public:
    explicit ObfusASTConsumer(clang::ASTContext *ctx,
                             clang::Rewriter &R,
                             std::string &fn1,
                             std::string &fn2)
        : _visitor(ctx, R, fn1, fn2)
    {
        // std::cout << "gouzao FuncASTConsumer\n";
    }

    virtual void HandleTranslationUnit(clang::ASTContext &ctx)
    {
        // llvm::errs()
        //     << "in function FuncASTConsumer::HandleTranslationUnit()\n";
        _visitor.TraverseDecl(ctx.getTranslationUnitDecl());
        _visitor.setFlag();
        _visitor.TraverseDecl(ctx.getTranslationUnitDecl());
    }

  private:
    ObfusASTVisitor _visitor;
};

class ObfusFrontendAction : public clang::ASTFrontendAction
{
  public:
    ObfusFrontendAction(std::string &fn1, std::string &fn2) : info_path{ fn1 }, func_path { fn2 }
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
            &compiler.getASTContext(), _rewriter, info_path, func_path);
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
        replace_suffix(file_name, "-obfuscated");
        llvm::raw_fd_stream fd(file_name, ec);
        // _rewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
        _rewriter.getEditBuffer(SM.getMainFileID()).write(fd);
        /* for (auto it = SM.fileinfo_begin(); it != SM.fileinfo_end(); ++it) {
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
        } */
    }

  private:
    clang::Rewriter _rewriter;
    std::string &info_path;
    std::string &func_path;
};

class ObfusFactory : public clang::tooling::FrontendActionFactory
{
  public:
    ObfusFactory(std::string &fn1, std::string &fn2) : info_path{ fn1 }, func_path{ fn2 } {}
    std::unique_ptr<clang::FrontendAction> create() override
    {
        // llvm::errs() << "in function FuncFactory::create()\n";
        return std::make_unique<ObfusFrontendAction>(info_path, func_path);
    }
  private:
    std::string &info_path;
    std::string &func_path;
};
}
#endif