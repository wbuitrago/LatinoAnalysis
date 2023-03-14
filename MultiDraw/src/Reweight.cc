#include "../interface/Reweight.h"
#include "../interface/FormulaLibrary.h"
#include "../interface/FunctionLibrary.h"

#include "TClass.h"
#include "TTreeFormulaManager.h"

multidraw::Reweight::Reweight(CompiledExprPtr&& _x, TObject const* _source/* = nullptr*/) :
  source_(_source)
{
  exprs_.emplace_back(std::move(_x));
  setEvalType_();
}
  
multidraw::Reweight::Reweight(CompiledExprPtr&& _x, CompiledExprPtr&& _y, TObject const* _source) :
  source_(_source)
{
  exprs_.emplace_back(std::move(_x));
  exprs_.emplace_back(std::move(_y));
  
  if (exprs_[0]->getFormula() != nullptr && exprs_[1]->getFormula() != nullptr) {
    auto* manager(exprs_[0]->getFormula()->GetManager());
    manager->Add(exprs_[1]->getFormula());
    manager->Sync();
  }

  setEvalType_();
}

void
multidraw::Reweight::setEvalType_()
{
  if (source_ == nullptr)
    evaluate_ = [this](unsigned i)->double { return this->evaluateRaw_(i); };
  else if (source_->InheritsFrom(TH1::Class())) {
    auto& hist(static_cast<TH1 const&>(*source_));

    if (hist.GetDimension() != int(exprs_.size()))
      throw std::runtime_error(std::string("Invalid number of formulas given for histogram source of type ") + hist.IsA()->GetName());

    evaluate_ = [this](unsigned i)->double { return this->evaluateTH1_(i); };
  }
  else if (source_->InheritsFrom(TGraph::Class())) {
    spline_.reset(new TSpline3("interpolation", static_cast<TGraph const*>(source_)));
  
    evaluate_ = [this](unsigned i)->double { return this->evaluateTGraph_(i); };
  }
  else if (source_->InheritsFrom(TF1::Class())) {
    auto& fct(static_cast<TF1 const&>(*source_));

    if (fct.GetNdim() != int(exprs_.size()))
      throw std::runtime_error(std::string("Invalid number of formulas given for function source of type ") + fct.IsA()->GetName());

    evaluate_ = [this](unsigned i)->double { return this->evaluateTF1_(i); };
  }
  else
    throw std::runtime_error(TString::Format("Object of incompatible class %s passed to Reweight", source_->IsA()->GetName()).Data());
}

TTreeFormulaCached*
multidraw::Reweight::getFormula(unsigned i/* = 0*/) const
{
  auto& expr(*exprs_.at(i));
  if (expr.getFormula() == nullptr)
    throw std::runtime_error("getFormula called on non-formula reweight");
  return expr.getFormula();
}

unsigned
multidraw::Reweight::getNdata()
{
  if (exprs_.empty())
    return 1;

  return exprs_[0]->getNdata();
}

double
multidraw::Reweight::evaluateRaw_(unsigned _iD)
{
  if (exprs_[0]->getFormula() != nullptr) {
    exprs_[0]->getNdata();
    if (_iD != 0)
      exprs_[0]->evaluate(0);
  }
  
  return exprs_[0]->evaluate(_iD);
}

double
multidraw::Reweight::evaluateTH1_(unsigned _iD)
{
  auto& hist(static_cast<TH1 const&>(*source_));

  double x[2]{};
  for (unsigned iDim(0); iDim != exprs_.size(); ++iDim) {
    auto& expr(*exprs_[iDim]);
    if (expr.getFormula() != nullptr) {
      expr.getNdata();
      if (_iD != 0)
        expr.evaluate(0);
    }
    
    x[iDim] = expr.evaluate(_iD);
  }

  int iBin(hist.FindFixBin(x[0], x[1]));

  // not handling over/underflow at the moment

  return hist.GetBinContent(iBin);
}

double
multidraw::Reweight::evaluateTGraph_(unsigned _iD)
{
  auto& graph(static_cast<TGraph const&>(*source_));

  if (exprs_[0]->getFormula() != nullptr) {
    exprs_[0]->getNdata();
    if (_iD != 0)
      exprs_[0]->evaluate(0);
  }
  
  double x(exprs_[0]->evaluate(_iD));

  return graph.Eval(x, spline_.get());
}

double
multidraw::Reweight::evaluateTF1_(unsigned _iD)
{
  auto& fct(static_cast<TF1 const&>(*source_));

  double x[2]{};
  for (unsigned iDim(0); iDim != exprs_.size(); ++iDim) {
    auto& expr(*exprs_[iDim]);
    if (expr.getFormula() != nullptr) {
      expr.getNdata();
      if (_iD != 0)
        expr.evaluate(0);
    }
      
    x[iDim] = expr.evaluate(_iD);
  }
  
  return fct.Eval(x[0], x[1]);
}


multidraw::FactorizedReweight::FactorizedReweight(ReweightPtr&& _ptr1, ReweightPtr&& _ptr2) :
  subReweights_{{std::move(_ptr1), std::move(_ptr2)}}
{
}

TTreeFormulaCached*
multidraw::FactorizedReweight::getFormula(unsigned i/* = 0*/) const
{
  if (i < subReweights_[0]->getNdim())
    return subReweights_[0]->getFormula(i);
  else
    return subReweights_[1]->getFormula(i - subReweights_[0]->getNdim());
}

multidraw::ReweightSource::ReweightSource(ReweightSource const& _orig) :
  exprs_(_orig.exprs_),
  source_(_orig.source_)
{
  if (_orig.subReweights_[0]) {
    subReweights_[0] = std::make_unique<ReweightSource>(*_orig.subReweights_[0]);
    subReweights_[1] = std::make_unique<ReweightSource>(*_orig.subReweights_[1]);
  }
}

multidraw::ReweightPtr
multidraw::ReweightSource::compile(FormulaLibrary& _formulaLibrary, FunctionLibrary& _functionLibrary) const
{
  if (subReweights_[0])
    return ReweightPtr(new FactorizedReweight(subReweights_[0]->compile(_formulaLibrary, _functionLibrary), subReweights_[1]->compile(_formulaLibrary, _functionLibrary)));

  if (exprs_.size() == 1)
    return std::make_unique<Reweight>(exprs_[0].compile(_formulaLibrary, _functionLibrary), source_);
  else
    return std::make_unique<Reweight>(exprs_[0].compile(_formulaLibrary, _functionLibrary), exprs_[1].compile(_formulaLibrary, _functionLibrary), source_);
}
