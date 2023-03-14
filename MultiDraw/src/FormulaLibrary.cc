#include "../interface/FormulaLibrary.h"

#include <cstring>
#include <iostream>
#include <sstream>

multidraw::FormulaLibrary::FormulaLibrary(TTree& _tree) :
  tree_(_tree)
{
}

TTreeFormulaCached&
multidraw::FormulaLibrary::getFormula(char const* _expr, bool _silent/* = false*/)
{
  if (_expr == nullptr || std::strlen(_expr) == 0)
    throw std::invalid_argument("Empty expression");
  
  std::shared_ptr<TTreeFormulaCached::Cache> cache;

  auto fItr(caches_.find(_expr));
  if (fItr != caches_.end())
    cache = fItr->second;

  auto* formula(NewTTreeFormulaCached("formula", _expr, &tree_, cache, _silent));
  if (formula == nullptr) {
    std::stringstream ss;
    ss << "Failed to compile expression \"" << _expr << "\"";
    if (!_silent)
      std::cerr << ss.str() << std::endl;
    throw std::invalid_argument(ss.str());
  }

  if (fItr == caches_.end())
    caches_.emplace(std::string(_expr), formula->GetCache());

  formulas_.emplace_back(formula);

  return *formula;
}

void
multidraw::FormulaLibrary::updateFormulaLeaves()
{
  for (auto& form : formulas_)
    form->UpdateFormulaLeaves();
}

void
multidraw::FormulaLibrary::resetCache()
{
  for (auto& ec : caches_)
    ec.second->fValues.clear();
}

void
multidraw::FormulaLibrary::replaceAll(char const* _from, char const* _to)
{
  bool replaced{false};
  for (auto& formula : formulas_) {
    if (formula->ReplaceLeaf(_from, _to))
      replaced = true;
  }

  if (replaced)
    resetCache();
}
