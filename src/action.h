#ifndef FCOSAL_ACTION_H
#define FCOSAL_ACTION_H

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"

namespace obfuscator
{
class ASTVisitor : public clang::RecursiveASTVisitor<ASTVisitor>
{
  public:
    explicit ASTVisitor(clang::ASTContext *ctx) : _ctx(ctx) {}

    bool VisitFunctionDecl(clang::FunctionDecl *func_decl)
    {
        // skip the function from included file
        if (_ctx->getSourceManager().isInMainFile(func_decl->getLocation()))
        {
            // For debugging, dumping the AST nodes will show which nodes are already
            // being visited.
            func_decl->dump();
        }
        return true;
    }

  private:
    clang::ASTContext *_ctx;
};

class ASTConsumer : public clang::ASTConsumer
{
  public:
    explicit ASTConsumer(clang::ASTContext *ctx) : _visitor(ctx) {}

    virtual void HandleTranslationUnit(clang::ASTContext &ctx)
    {
        _visitor.TraverseDecl(ctx.getTranslationUnitDecl());
    }

  private:
    ASTVisitor _visitor;
};

class FrontendAction : public clang::ASTFrontendAction
{
  public:
    virtual std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance &compiler,
                          llvm::StringRef in_file)
    {
        return std::make_unique<ASTConsumer>(&compiler.getASTContext());
    }
};
}  // namespace obfuscator
#endif