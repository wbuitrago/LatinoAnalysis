#ifndef multidraw_Cut_h
#define multidraw_Cut_h

#include "ExprFiller.h"

#include "TString.h"

#include <vector>
#include <memory>

class TTreeFormulaCached;

namespace multidraw {

  class FormulaLibrary;
  class FunctionLibrary;

  class Cut {
  public:
    Cut(char const* name, char const* expr = "");
    ~Cut();

    TString getName() const;
    void setPrintLevel(int);

    unsigned getNFillers() const { return fillers_.size(); }
    ExprFiller const* getFiller(unsigned i) const { return fillers_.at(i).get(); }

    void setCutExpr(char const* expr) { cutExpr_ = expr; }
    TString const& getCutExpr() const { return cutExpr_; }

    void addCategory(char const* expr) { categoryExprs_.emplace_back(expr); }
    void setCategorization(char const* expr);
    int getNCategories() const;

    void addFiller(ExprFillerPtr&& _filler) { fillers_.emplace_back(std::move(_filler)); }

    void bindTree(FormulaLibrary&, FunctionLibrary&);
    void unlinkTree();
    std::unique_ptr<Cut> threadClone(FormulaLibrary&, FunctionLibrary&) const;

    bool dependsOn(TTree const&) const;

    void initialize();
    bool evaluate();
    void fillExprs(std::vector<double> const& eventWeights);

    unsigned getCount() const { return counter_; }

  protected:
    TString name_{""};
    TString cutExpr_{""};
    std::vector<TString> categoryExprs_{};
    TString categorizationExpr_{""};
    std::vector<ExprFillerPtr> fillers_{};
    int printLevel_{0};
    unsigned counter_{0};

    std::vector<int> categoryIndex_{};
    TTreeFormulaCached* compiledCut_{};
    std::vector<TTreeFormulaCached*> compiledCategories_{};
    TTreeFormulaCached* compiledCategorization_{};
  };

  typedef std::unique_ptr<Cut> CutPtr;

}

#endif
