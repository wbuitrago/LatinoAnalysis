#ifndef multidraw_CompiledExpr_h
#define multidraw_CompiledExpr_h

#include "TTreeFormulaCached.h"
#include "TTreeFunction.h"

#include <memory>

namespace multidraw {

  class FormulaLibrary;
  class FunctionLibrary;
  class CompiledExpr;

  class CompiledExprSource {
  public:
    CompiledExprSource() {}
    CompiledExprSource(char const* formula) : formula_(formula) {}
    CompiledExprSource(TTreeFunction const& function) : function_(function.clone()) {}
    CompiledExprSource(CompiledExprSource const& orig) : formula_(orig.formula_), function_(orig.function_ ? orig.function_->clone() : nullptr) {}

    TString const& getFormula() const { return formula_; }
    TTreeFunction const* getFunction() const { return function_.get(); }

    std::unique_ptr<CompiledExpr> compile(FormulaLibrary&, FunctionLibrary&) const;

  private:
    TString formula_{};
    std::unique_ptr<TTreeFunction> function_{};
  };

  class CompiledExpr {
  public:
    CompiledExpr(TTreeFormulaCached& formula) : formula_(&formula) {}
    CompiledExpr(TTreeFunction&);
    ~CompiledExpr() {}

    TTreeFormulaCached* getFormula() const { return formula_; }
    TTreeFunction* getFunction() const { return function_; }

    unsigned getNdata();
    double evaluate(unsigned);

  private:
    TTreeFormulaCached* formula_{};
    TTreeFunction* function_{};
  };

  typedef std::unique_ptr<CompiledExpr> CompiledExprPtr;

}

#endif
