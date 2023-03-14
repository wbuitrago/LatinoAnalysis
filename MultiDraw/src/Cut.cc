#include "../interface/Cut.h"
#include "../interface/ExprFiller.h"
#include "../interface/FormulaLibrary.h"
#include "../interface/FunctionLibrary.h"
#include "../interface/TTreeFormulaCached.h"

#include "TTreeFormulaManager.h"
#include "TTree.h"

#include <iostream>

multidraw::Cut::Cut(char const* _name, char const* _expr/* = ""*/) :
  name_(_name),
  cutExpr_(_expr)
{
}

multidraw::Cut::~Cut()
{
  unlinkTree();

  for (auto& filler : fillers_)
    filler->mergeBack();
}

TString
multidraw::Cut::getName() const
{
  if (name_.Length() == 0)
    return "[Event filter]";
  else
    return name_;
}

void
multidraw::Cut::setPrintLevel(int _l)
{
  printLevel_ = _l;
  for (auto& filler : fillers_)
    filler->setPrintLevel(_l);
}

void
multidraw::Cut::setCategorization(char const* _expr)
{
  categoryExprs_.clear();
  categorizationExpr_ = _expr;
}

int
multidraw::Cut::getNCategories() const
{
  if (categorizationExpr_.Length() != 0)
    return -1; // unknown
  else
    return categoryExprs_.size();
}

void
multidraw::Cut::bindTree(FormulaLibrary& _formulaLibrary, FunctionLibrary& _functionLibrary)
{
  counter_ = 0;

  unlinkTree();

  if (cutExpr_.Length() != 0)
    compiledCut_ = &_formulaLibrary.getFormula(cutExpr_);

  if (categorizationExpr_.Length() != 0)
    compiledCategorization_ = &_formulaLibrary.getFormula(categorizationExpr_);
  else {
    for (auto& expr : categoryExprs_)
      compiledCategories_.push_back(&_formulaLibrary.getFormula(expr));
  }

  for (auto& filler : fillers_)
    filler->bindTree(_formulaLibrary, _functionLibrary);
}

void
multidraw::Cut::unlinkTree()
{
  compiledCut_ = nullptr;

  compiledCategorization_ = nullptr;
  compiledCategories_.clear();

  for (auto& filler : fillers_)
    filler->unlinkTree();
}

multidraw::CutPtr
multidraw::Cut::threadClone(FormulaLibrary& _formulaLibrary, FunctionLibrary& _functionLibrary) const
{
  auto clone(std::make_unique<Cut>(name_, cutExpr_));
  clone->setPrintLevel(-1);

  if (categorizationExpr_.Length() != 0)
    clone->setCategorization(categorizationExpr_);
  else {
    for (auto& expr : categoryExprs_)
      clone->addCategory(expr);
  }

  for (auto& filler : fillers_)
    clone->addFiller(filler->threadClone(_formulaLibrary, _functionLibrary));

  clone->bindTree(_formulaLibrary, _functionLibrary);

  return clone;
}

bool
multidraw::Cut::dependsOn(TTree const& _tree) const
{
  if (compiledCut_ == nullptr)
    return false;

  auto doesDepend([&_tree](TTreeFormulaCached& _form)->bool {
    for (int iL(0); iL != _form.GetListOfLeaves()->GetEntriesFast(); ++iL) {
      auto* leaf(_form.GetLeaf(iL));
      if (leaf == nullptr)
        continue;

      if (leaf->GetBranch()->GetTree() == &_tree)
        return true;
    }

    return false;
    });

  if (doesDepend(*compiledCut_))
    return true;

  if (compiledCategorization_ != nullptr && doesDepend(*compiledCategorization_))
    return true;

  for (auto* cat : compiledCategories_) {
    if (doesDepend(*cat))
      return true;
  }

  return false;
}

void
multidraw::Cut::initialize()
{
  if (compiledCut_ == nullptr)
    return;

  // Each formula object has a default manager
  auto* formulaManager(compiledCut_->GetManager());
  if (compiledCategorization_ != nullptr)
    formulaManager->Add(compiledCategorization_);
  else {
    for (auto* cat : compiledCategories_)
      formulaManager->Add(cat);
  }

  formulaManager->Sync();

  // It's probably more correct to pass the manager to filler here and synchronize all at the same time
  // Currently Cut and ExprFiller use independent formula managers
  for (auto& filler : fillers_)
    filler->initialize();
}

bool
multidraw::Cut::evaluate()
{
  unsigned nD(1);
  
  if (compiledCut_ != nullptr)
    nD = compiledCut_->GetNdata();

  if (compiledCategorization_ != nullptr) {
    compiledCategorization_->GetNdata();
    compiledCategorization_->EvalInstance(0);
  }
  else {
    for (auto* cat : compiledCategories_) {
      cat->GetNdata();
      cat->EvalInstance(0);
    }
  }

  categoryIndex_.assign(nD, -1);

  if (printLevel_ > 2)
    std::cout << "        " << getName() << " has " << nD << " iterations" << std::endl;

  bool any(false);

  for (unsigned iD(0); iD != nD; ++iD) {
    if (compiledCut_ != nullptr && compiledCut_->EvalInstance(iD) == 0.)
      continue;

    if (compiledCategorization_ != nullptr)
      categoryIndex_[iD] = int(compiledCategorization_->EvalInstance(iD));
    else if (!compiledCategories_.empty()) {
      for (unsigned icat(0); icat != compiledCategories_.size(); ++icat) {
        if (compiledCategories_[icat]->EvalInstance(iD) != 0.) {
          categoryIndex_[iD] = icat;
          break;
        }
      }
    }
    else
      categoryIndex_[iD] = 0;

    any = true;

    if (printLevel_ > 2)
      std::cout << "        " << getName() << " iteration " << iD << " pass (cat. index " << categoryIndex_[iD] << ")" << std::endl;
  }
  
  return any;
}

void
multidraw::Cut::fillExprs(std::vector<double> const& _eventWeights)
{
  ++counter_;

  for (auto& filler : fillers_)
    filler->fill(_eventWeights, categoryIndex_);
}
