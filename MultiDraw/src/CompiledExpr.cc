#include "../interface/CompiledExpr.h"
#include "../interface/FormulaLibrary.h"
#include "../interface/FunctionLibrary.h"

std::unique_ptr<multidraw::CompiledExpr>
multidraw::CompiledExprSource::compile(FormulaLibrary& _formulaLibrary, FunctionLibrary& _functionLibrary) const
{
  if (formula_.Length() != 0)
    return std::make_unique<CompiledExpr>(_formulaLibrary.getFormula(formula_));
  else
    return std::make_unique<CompiledExpr>(_functionLibrary.getFunction(*function_));
}

multidraw::CompiledExpr::CompiledExpr(TTreeFunction& _function) :
  function_(&_function)
{
  if (!function_->isLinked())
    throw std::runtime_error("Unlinked TTreeFunction used to construct CompiledExpr");
}

unsigned
multidraw::CompiledExpr::getNdata()
{
  if (formula_ != nullptr)
    return formula_->GetNdata();
  else
    return function_->getNdata();
}

double
multidraw::CompiledExpr::evaluate(unsigned _iD)
{
  if (formula_ != nullptr)
    return formula_->EvalInstance(_iD);
  else
    return function_->evaluate(_iD);
}
