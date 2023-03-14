#ifndef multidraw_Plot2DFiller_h
#define multidraw_Plot2DFiller_h

#include "ExprFiller.h"

#include "TH2.h"
#include "TObjArray.h"

namespace multidraw {

  //! A wrapper class for TH2
  /*!
   * The class is to be used within MultiDraw, and is instantiated by addPlot().
   * Arguments:
   *  hist     The actual histogram object (the user is responsible for creating it)
   *  xexpr    Expression whose evaluated value gets filled to the plot
   *  yexpr    Expression whose evaluated value gets filled to the plot
   *  reweight If provided, evalutaed and used as weight for filling the histogram
   */
  class Plot2DFiller : public ExprFiller {
  public:
    Plot2DFiller(TH2& hist, CompiledExprSource const& xsource, CompiledExprSource const& ysource, char const* reweight = "");
    Plot2DFiller(TObjArray& histlist, CompiledExprSource const& xsource, CompiledExprSource const& ysource, char const* reweight = "");
    Plot2DFiller(Plot2DFiller const&);
    ~Plot2DFiller() {}

    TH2 const& getHist(int icat = -1) const { return static_cast<TH2 const&>(getObj(icat)); }
    TH2& getHist(int icat = -1) { return static_cast<TH2&>(getObj(icat)); }

    unsigned getNdim() const override { return 2; }

  private:
    Plot2DFiller(TH2& hist, Plot2DFiller const&);
    Plot2DFiller(TObjArray& histlist, Plot2DFiller const&);

    void doFill_(unsigned, int icat = -1) override;
    ExprFiller* clone_() override;
    void mergeBack_() override;
  };

}

#endif
