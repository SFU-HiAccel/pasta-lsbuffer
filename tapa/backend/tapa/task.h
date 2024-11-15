#ifndef TAPA_TASK_H_
#define TAPA_TASK_H_

#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "nlohmann/json.hpp"

#include "../target/all_targets.h"
#include "stream.h"
#include "buffer.h"

namespace tapa {
namespace internal {

const clang::ExprWithCleanups* GetTapaTask(const clang::Stmt* func_body);

std::vector<const clang::CXXMemberCallExpr*> GetTapaInvokes(
    const clang::Stmt* task);

class Visitor : public clang::RecursiveASTVisitor<Visitor> {
 public:
  explicit Visitor(
      clang::ASTContext& context,
      std::vector<const clang::FunctionDecl*>& funcs,
      std::unordered_map<const clang::FunctionDecl*, clang::Rewriter>&
          rewriters,
      std::unordered_map<const clang::FunctionDecl*, nlohmann::json>& metadata)
      : context_{context},
        funcs_{funcs},
        rewriters_{rewriters},
        metadata_{metadata} {}

  bool VisitAttributedStmt(clang::AttributedStmt* stmt);
  bool VisitFunctionDecl(clang::FunctionDecl* func);

  void VisitTask(const clang::FunctionDecl* func);

 private:
  static thread_local const clang::FunctionDecl* rewriting_func;
  static thread_local const clang::FunctionDecl* current_task;
  static thread_local Target* current_target;

  clang::ASTContext& context_;
  std::vector<const clang::FunctionDecl*>& funcs_;
  std::unordered_map<const clang::FunctionDecl*, clang::Rewriter>& rewriters_;
  std::unordered_map<const clang::FunctionDecl*, nlohmann::json>& metadata_;

  clang::Rewriter& GetRewriter() { return rewriters_[current_task]; }
  nlohmann::json& GetMetadata() {
    if (metadata_[current_task].is_null())
      metadata_[current_task] = nlohmann::json::object();
    return metadata_[current_task];
  }

  void ProcessUpperLevelTask(const clang::ExprWithCleanups* task,
                             const clang::FunctionDecl* func);

  void ProcessLowerLevelTask(const clang::FunctionDecl* func);
  std::string GetFrtInterface(const clang::FunctionDecl* func);

  clang::CharSourceRange GetCharSourceRange(const clang::Stmt* stmt);
  clang::CharSourceRange GetCharSourceRange(clang::SourceRange range);
  clang::SourceLocation GetEndOfLoc(clang::SourceLocation loc);

  int64_t EvalAsInt(const clang::Expr* expr);

  int64_t GetIntegerFromTemplateExpression(const clang::TemplateArgument &templateArgument) {
    int64_t integral = 0;
    if (templateArgument.getKind() != clang::TemplateArgument::Expression) {
      printf("GetIntegerFromTemplateExpression: Integral\n");
      return integral;
    }
    const clang::Expr *expression = templateArgument.getAsExpr();
    printf("GetIntegerFromTemplateExpression: Expr\n");
    clang::Expr::EvalResult evalResult;
    if (expression->isValueDependent()) {
      printf("Eval: %ld | Old: %ld\n", EvaluateConstantExpr(templateArgument.getAsExpr()), integral);
      return integral;
    }
    integral += evalResult.Val.getInt().getSExtValue();
    printf("Eval: %ld | Old: %ld\n", EvaluateConstantExpr(templateArgument.getAsExpr()), integral);
    return integral;
  }

  int GetTypeWidth(const clang::QualType type) {
    return context_.getTypeInfo(type).Width;
  }

  void PrintTemplateArgument(const clang::TemplateArgument &arg) {
      using namespace clang;
      using namespace llvm;

      switch (arg.getKind()) {
          case TemplateArgument::Null:
              errs() << "TemplateArgument is Null.\n";
              break;
          case TemplateArgument::Type:
              errs() << "TemplateArgument Type: "
                     << arg.getAsType().getAsString() << "\n";
              break;
          case TemplateArgument::Declaration:
              errs() << "TemplateArgument Declaration: "
                     << arg.getAsDecl()->getDeclKindName() << "\n";
              break;
          case TemplateArgument::Integral:
              errs() << "TemplateArgument Integral: "
                     << arg.getAsIntegral().toString(10) << "\n";
              break;
          case TemplateArgument::Template:
              errs() << "TemplateArgument Template: "
                     << arg.getAsTemplate().getAsTemplateDecl()->getNameAsString() << "\n";
              break;
          case TemplateArgument::TemplateExpansion:
              errs() << "TemplateArgument TemplateExpansion\n";
              break;
          case TemplateArgument::Expression:
              errs() << "TemplateArgument Expression: ";
              arg.getAsExpr()->dump();
              break;
          case TemplateArgument::Pack:
              errs() << "TemplateArgument Pack with "
                     << arg.pack_size() << " elements:\n";
              for (const auto &packedArg : arg.pack_elements()) {
                  PrintTemplateArgument(packedArg);
              }
              break;
          default:
              errs() << "Unknown TemplateArgument kind.\n";
              break;
      }
  }

  int GetTypeWidthBuffer(const clang::QualType type) {
    clang::RecordDecl* recordDecl = type->getAsRecordDecl();
    if (recordDecl) {
      //llvm::dbgs() << recordDecl->getQualifiedNameAsString() << "\n";
      int64_t totalWidth = 0;
      // this is an ap_uint or ap_int type
      if (recordDecl->getNameAsString() == "ap_uint" || recordDecl->getNameAsString() == "ap_int") {
        auto templateArgument = GetTemplateArg(type, 0);
        //PrintTemplateArgument(*templateArgument);
        if(templateArgument->getKind() == clang::TemplateArgument::Integral) {
          auto width = GetIntegerFromTemplateArg(*templateArgument);
          totalWidth += width;
        } else if(templateArgument->getKind() == clang::TemplateArgument::Expression) {
          auto width = EvaluateConstantExpr(templateArgument->getAsExpr());
          totalWidth += width;
        } else {
          printf("No template argument\n");
        }
        //auto width = GetIntegerFromTemplateArg(*templateArgument);
        //llvm::dbgs() << "Found ap_(u)int: width: " << width << "\n"; 
      } else {
        for (auto field: recordDecl->fields()) {
          auto originalQualType = field->getType();
          auto qualType = field->getType().getDesugaredType(context_);
          auto qualTypeString = qualType.getAsString();
          totalWidth += context_.getTypeInfo(qualType).Width;
          //llvm::dbgs() << "No ap_(u)int: width: " << context_.getTypeInfo(qualType).Width << "\n"; 
        }
      }
      // printf("totalWidth:: %ld\n", totalWidth);
      return totalWidth;
    } else {
      return context_.getTypeInfo(type).Width;
    }
  }

  template <typename T>
  void HandleAttrOnNodeWithBody(const T* node, const clang::Stmt* body,
                                llvm::ArrayRef<const clang::Attr*> attrs);
};

// Find for a given upper-level task, return all direct children tasks (e.g.
// tasks instanciated directly in upper).
// Lower-level tasks or non-task functions return an empty vector.
inline std::vector<const clang::FunctionDecl*> FindChildrenTasks(
    const clang::FunctionDecl* upper) {
  auto body = upper->getBody();
  if (auto task = GetTapaTask(body)) {
    auto invokes = GetTapaInvokes(task);
    std::vector<const clang::FunctionDecl*> tasks;
    for (auto invoke : invokes) {
      // Dynamic cast correctness is guaranteed by tapa.h.
      if (auto decl_ref =
              llvm::dyn_cast<clang::DeclRefExpr>(invoke->getArg(0))) {
        auto func_decl =
            llvm::dyn_cast<clang::FunctionDecl>(decl_ref->getDecl());
        if (func_decl->isTemplateInstantiation()) {
          func_decl = func_decl->getPrimaryTemplate()->getTemplatedDecl();
        }

        // skip function definitions.
        if (!func_decl->isThisDeclarationADefinition()) continue;
        tasks.push_back(func_decl);
      }
    }
    return tasks;
  }
  return {};
}

// Find all tasks instanciated using breadth-first search.
// If a task is instantiated more than once, it will only appear once.
// Lower-level tasks or non-task functions return an empty vector.
inline std::vector<const clang::FunctionDecl*> FindAllTasks(
    const clang::FunctionDecl* root_upper) {
  std::vector<const clang::FunctionDecl*> tasks{root_upper};
  std::unordered_set<const clang::FunctionDecl*> task_set{root_upper};
  std::queue<const clang::FunctionDecl*> task_queue;

  task_queue.push(root_upper);
  while (!task_queue.empty()) {
    auto upper = task_queue.front();
    for (auto child : FindChildrenTasks(upper)) {
      if (task_set.count(child) == 0) {
        tasks.push_back(child);
        task_set.insert(child);
        task_queue.push(child);
      }
    }
    task_queue.pop();
  }

  return tasks;
}

// Return the body of a loop stmt or nullptr if the input is not a loop.
inline const clang::Stmt* GetLoopBody(const clang::Stmt* loop) {
  if (loop != nullptr) {
    if (auto stmt = llvm::dyn_cast<clang::DoStmt>(loop)) {
      return stmt->getBody();
    }
    if (auto stmt = llvm::dyn_cast<clang::ForStmt>(loop)) {
      return stmt->getBody();
    }
    if (auto stmt = llvm::dyn_cast<clang::WhileStmt>(loop)) {
      return stmt->getBody();
    }
    if (auto stmt = llvm::dyn_cast<clang::CXXForRangeStmt>(loop)) {
      return stmt->getBody();
    }
  }
  return nullptr;
}

}  // namespace internal
}  // namespace tapa

#endif  // TAPA_TASK_H_
