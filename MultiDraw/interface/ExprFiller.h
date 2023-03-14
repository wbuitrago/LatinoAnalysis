#ifndef multidraw_ExprFiller_h
#define multidraw_ExprFiller_h

#include "TTreeFormulaCached.h"
#include "Reweight.h"
#include "CompiledExpr.h"

#include "TString.h"

#include <vector>
#include <memory>

namespace multidraw {

  //! Filler object base class with expressions, a cut, and a reweight.
  /*!
   * Inherited by Plot (histogram) and Tree (tree). Does not own any of
   * the TTreeFormula objects by default.
   * Has a function to reweight but only through simple expressions. Can
   * in principle expand to allow reweight through histograms and graphs.
   */
  class ExprFiller {
  public:
    ExprFiller(TObject&, char const* reweight = "");
    ExprFiller(ExprFiller const&);
    virtual ~ExprFiller();

    void setPrintLevel(int l) { printLevel_ = l; }

    TObject const& getObj(int icat = -1) const;
    TObject& getObj(int icat = -1);

    virtual unsigned getNdim() const = 0;
    TTreeFormulaCached* getFormula(unsigned i = 0) const { return compiledExprs_.at(i)->getFormula(); }
    void setReweight(ReweightSource const& source) { reweightSource_ = std::make_unique<ReweightSource>(source); }
    ReweightSource const* getReweightSource() const { return reweightSource_.get(); }
    Reweight const* getReweight() const { return compiledReweight_.get(); }

    void bindTree(FormulaLibrary&, FunctionLibrary&);
    void unlinkTree();
    std::unique_ptr<ExprFiller> threadClone(FormulaLibrary&, FunctionLibrary&);

    void initialize();
    void fill(std::vector<double> const& eventWeights, std::vector<int> const& categories);

    //! Merge the underlying object into the main-thread object
    void mergeBack();

    unsigned getCount() const { return counter_; }

  protected:
    // Special copy constructor for cloning
    ExprFiller(TObject&, ExprFiller const&);

    virtual void doFill_(unsigned, int = -1) = 0;
    virtual ExprFiller* clone_() = 0;
    virtual void mergeBack_() = 0;

    TObject& tobj_;

    std::vector<CompiledExprSource> sources_{};
    ReweightSourcePtr reweightSource_{nullptr};
    double entryWeight_{1.};
    unsigned counter_{0};

    ExprFiller* cloneSource_{nullptr};

    int printLevel_{0};

    std::vector<CompiledExprPtr> compiledExprs_{};
    ReweightPtr compiledReweight_{nullptr};

    bool categorized_{false};
  };

  typedef std::unique_ptr<ExprFiller> ExprFillerPtr;

}

#endif
