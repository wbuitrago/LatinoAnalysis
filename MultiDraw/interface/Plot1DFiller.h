#ifndef multidraw_Plot1DFiller_h
#define multidraw_Plot1DFiller_h

#include "ExprFiller.h"

#include "TH1.h"
#include "TObjArray.h"

namespace multidraw {

  //! A wrapper class for TH1
  /*!
   * The class is to be used within MultiDraw, and is instantiated by addPlot().
   * Arguments:
   *  hist      The actual histogram object (the user is responsible for creating it)
   *  expr      Expression whose evaluated value gets filled to the plot
   *  reweight  If provided, evalutaed and used as weight for filling the histogram
   *  mode      kDefault: overflow is not handled explicitly (i.e. TH1 fills the n+1-st bin)
   *            kDedicated: an overflow bin with size (original width)*overflowBinSize is created
   *            kMergeLast: overflow is added to the last bin
   */
  class Plot1DFiller : public ExprFiller {
  public:
    enum OverflowMode {
      kDefault,
      kDedicated,
      kMergeLast
    };

    Plot1DFiller(TH1& hist, CompiledExprSource const& expr, char const* reweight = "", OverflowMode mode = kDefault);
    Plot1DFiller(TObjArray& histlist, CompiledExprSource const& expr, char const* reweight = "", OverflowMode mode = kDefault);
    Plot1DFiller(Plot1DFiller const&);
    ~Plot1DFiller() {}

    TH1 const& getHist(int icat = -1) const { return static_cast<TH1 const&>(getObj(icat)); }
    TH1& getHist(int icat = -1) { return static_cast<TH1&>(getObj(icat)); }

    unsigned getNdim() const override { return 1; }

  private:
    Plot1DFiller(TH1& hist, Plot1DFiller const&);
    Plot1DFiller(TObjArray& hist, Plot1DFiller const&);

    void doFill_(unsigned, int icat = -1) override;
    ExprFiller* clone_() override;
    void mergeBack_() override;

    OverflowMode overflowMode_{kDefault};
  };

}

#endif
