#ifndef multidraw_FormulaLibrary_h
#define multidraw_FormulaLibrary_h

#include "TTreeFormulaCached.h"

#include "TString.h"

#include <unordered_map>
#include <list>
#include <memory>

class TTree;

namespace multidraw {

  class FormulaLibrary {
  public:
    FormulaLibrary(TTree&);
    ~FormulaLibrary() {}

    //! Create a new formula object with a shared cache.
    /*!
     * ExprFiller uses a TTreeFormulaManager to aggregate formulas. Adding a formula to a manager overwrites
     * an attribute of the formula object itself. Since the grouping can be different for each filler, this
     * means we cannot cache the formulas themselves and reuse among the fillers.
     */
    TTreeFormulaCached& getFormula(char const* expr, bool silent = false);

    void updateFormulaLeaves();
    void resetCache();

    void replaceAll(char const* from, char const* to);

    unsigned size() const { return formulas_.size(); }

  private:
    TTree& tree_;

    std::unordered_map<std::string, TTreeFormulaCached::CachePtr> caches_{};
    std::list<std::unique_ptr<TTreeFormulaCached>> formulas_{};
  };

}

#endif
