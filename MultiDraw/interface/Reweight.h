#ifndef multidraw_Reweight_h
#define multidraw_Reweight_h

#include "TTreeFormulaCached.h"
#include "CompiledExpr.h"

#include "TH1.h"
#include "TGraph.h"
#include "TF1.h"
#include "TSpline.h"

#include <functional>

class TSpline3;

namespace multidraw {

  class Reweight {
  public:
    Reweight() {}
    Reweight(CompiledExprPtr&&, TObject const* = nullptr);
    Reweight(CompiledExprPtr&&, CompiledExprPtr&&, TObject const*);
    virtual ~Reweight() {}

    virtual unsigned getNdim() const { return exprs_.size(); }
    virtual TTreeFormulaCached* getFormula(unsigned i = 0) const;
    virtual TObject const* getSource(unsigned i = 0) const { return source_; }

    virtual unsigned getNdata();
    virtual double evaluate(unsigned i = 0) const { if (evaluate_) return evaluate_(i); else return 1.; }

  protected:
    void setEvalType_();
    
    double evaluateRaw_(unsigned);
    double evaluateTH1_(unsigned);
    double evaluateTGraph_(unsigned);
    double evaluateTF1_(unsigned);

    //! One entry per source dimension
    std::vector<CompiledExprPtr> exprs_{};
    TObject const* source_{nullptr};
    std::unique_ptr<TSpline3> spline_{};

    std::function<double(unsigned)> evaluate_{};
  };

  typedef std::unique_ptr<Reweight> ReweightPtr;

  class FactorizedReweight : public Reweight {
  public:
    FactorizedReweight(ReweightPtr&&, ReweightPtr&&); // combined (factorized) reweighting
    ~FactorizedReweight() {}

    unsigned getNdim() const override { return subReweights_[0]->getNdim() * subReweights_[1]->getNdim(); }
    TTreeFormulaCached* getFormula(unsigned i = 0) const override;
    TObject const* getSource(unsigned i = 0) const override { return subReweights_[i]->getSource(); }

    unsigned getNdata() override { return subReweights_[0]->getNdata(); }
    double evaluate(unsigned i = 0) const override { return subReweights_[0]->evaluate(i) * subReweights_[1]->evaluate(i); }

  private:
    std::array<ReweightPtr, 2> subReweights_{};
  };

  class FormulaLibrary;
  class FunctionLibrary;

  class ReweightSource {
  public:
    ReweightSource() {}
    ReweightSource(ReweightSource const&);
    ReweightSource(char const* expr, TObject const* source = nullptr) : exprs_{{expr}}, source_(source) {}
    ReweightSource(char const* xexpr, char const* yexpr, TObject const* source = nullptr) : exprs_{{xexpr, yexpr}}, source_(source) {}
    ReweightSource(CompiledExprSource const& xexpr, TObject const* source = nullptr) : exprs_{{xexpr}}, source_(source) {}
    ReweightSource(CompiledExprSource const& xexpr, CompiledExprSource const& yexpr, TObject const* source = nullptr) : exprs_{{xexpr, yexpr}}, source_(source) {}    
    ReweightSource(ReweightSource const& r1, ReweightSource const& r2) {
      subReweights_[0] = std::make_unique<ReweightSource>(r1);
      subReweights_[1] = std::make_unique<ReweightSource>(r2);
    }

    ReweightPtr compile(FormulaLibrary&, FunctionLibrary&) const;

  private:
    std::vector<CompiledExprSource> exprs_{};
    TObject const* source_{nullptr};

    std::array<std::unique_ptr<ReweightSource>, 2> subReweights_;
  };

  typedef std::unique_ptr<ReweightSource> ReweightSourcePtr;

}

#endif
