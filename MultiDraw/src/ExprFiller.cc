#include "../interface/ExprFiller.h"

#include "TTree.h"
#include "TTreeFormulaManager.h"

#include <iostream>
#include <cstring>

multidraw::ExprFiller::ExprFiller(TObject& _tobj, char const* _reweight/* = ""*/) :
  tobj_(_tobj)
{
  if (_reweight != nullptr && std::strlen(_reweight) != 0)
    reweightSource_ = std::make_unique<ReweightSource>(_reweight);

  if (_tobj.IsA() == TObjArray::Class())
    categorized_ = true;
}

multidraw::ExprFiller::ExprFiller(ExprFiller const& _orig) :
  tobj_(_orig.tobj_),
  sources_(_orig.sources_),
  printLevel_(_orig.printLevel_),
  categorized_(_orig.categorized_)
{
  if (_orig.reweightSource_)
    reweightSource_ = std::make_unique<ReweightSource>(*_orig.reweightSource_);
}

multidraw::ExprFiller::ExprFiller(TObject& _tobj, ExprFiller const& _orig) :
  tobj_(_tobj),
  sources_(_orig.sources_),
  printLevel_(_orig.printLevel_),
  categorized_(_orig.categorized_)
{
  if (_orig.reweightSource_)
    reweightSource_ = std::make_unique<ReweightSource>(*_orig.reweightSource_);
}

multidraw::ExprFiller::~ExprFiller()
{
  unlinkTree();

  if (cloneSource_ != nullptr)
    delete &tobj_;
}

TObject const&
multidraw::ExprFiller::getObj(int _icat/* = -1*/) const
{
  if (categorized_) {
    auto& array(static_cast<TObjArray const&>(tobj_));
    if (_icat >= array.GetEntriesFast())
      throw std::runtime_error(TString::Format("Category index out of bounds: index %d >= maximum %d", _icat, array.GetEntriesFast()).Data());
    return *array.UncheckedAt(_icat);
  }
  else
    return tobj_;
}

TObject&
multidraw::ExprFiller::getObj(int _icat/* = -1*/)
{
  if (categorized_) {
    auto& array(static_cast<TObjArray&>(tobj_));
    if (_icat >= array.GetEntriesFast())
      throw std::runtime_error(TString::Format("Category index out of bounds: index %d >= maximum %d", _icat, array.GetEntriesFast()).Data());
    return *array.UncheckedAt(_icat);
  }
  else
    return tobj_;
}

void
multidraw::ExprFiller::bindTree(FormulaLibrary& _formulaLibrary, FunctionLibrary& _functionLibrary)
{
  unlinkTree();

  for (auto& source : sources_)
    compiledExprs_.emplace_back(source.compile(_formulaLibrary, _functionLibrary));

  if (reweightSource_)
    compiledReweight_ = reweightSource_->compile(_formulaLibrary, _functionLibrary);

  counter_ = 0;
}

void
multidraw::ExprFiller::unlinkTree()
{
  compiledExprs_.clear();
  compiledReweight_ = nullptr;
}

multidraw::ExprFillerPtr
multidraw::ExprFiller::threadClone(FormulaLibrary& _formulaLibrary, FunctionLibrary& _functionLibrary)
{
  ExprFillerPtr clone(clone_());
  clone->cloneSource_ = this;
  clone->setPrintLevel(-1);
  clone->bindTree(_formulaLibrary, _functionLibrary);
  return clone;
}

void
multidraw::ExprFiller::initialize()
{
  // Manage all dimensions with a single manager
  // manager instance will be owned collectively by the managed formulas (will be deleted when the last formula is deleted)
  auto* manager{new TTreeFormulaManager()};
  for (auto& expr : compiledExprs_) {
    if (expr->getFormula() == nullptr)
      continue;

    manager->Add(expr->getFormula());
  }

  manager->Sync();
}

void
multidraw::ExprFiller::fill(std::vector<double> const& _eventWeights, std::vector<int> const& _categories)
{
  // All exprs and reweight exprs share the same manager
  unsigned nD(compiledExprs_.at(0)->getNdata());

  if (printLevel_ > 3)
    std::cout << "          " << getObj().GetName() << "::fill() => " << nD << " iterations" << std::endl;

  if (_categories.size() < nD)
    nD = _categories.size();

  bool loaded(false);

  for (unsigned iD(0); iD != nD; ++iD) {
    if (_categories[iD] < 0)
      continue;

    ++counter_;

    if (!loaded) {
      for (auto& expr : compiledExprs_) {
        expr->getNdata();
        if (iD != 0) // need to always call EvalInstance(0)
          expr->evaluate(0);
      }
    }

    loaded = true;

    if (iD < _eventWeights.size())
      entryWeight_ = _eventWeights[iD];
    else
      entryWeight_ = _eventWeights.back();

    if (compiledReweight_ != nullptr)
      entryWeight_ *= compiledReweight_->evaluate(iD);

    doFill_(iD, _categories[iD]);
  }
}

void
multidraw::ExprFiller::mergeBack()
{
  if (cloneSource_ == nullptr)
    return;

  mergeBack_();
}
