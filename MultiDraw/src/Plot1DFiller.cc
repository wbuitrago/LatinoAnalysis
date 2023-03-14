#include "../interface/Plot1DFiller.h"
#include "../interface/FormulaLibrary.h"

#include <iostream>
#include <sstream>
#include <thread>

multidraw::Plot1DFiller::Plot1DFiller(TH1& _hist, CompiledExprSource const& _source, char const* _reweight/* = ""*/, Plot1DFiller::OverflowMode _mode/* = kDefault*/) :
  ExprFiller(_hist, _reweight),
  overflowMode_(_mode)
{
  sources_.push_back(_source);
}

multidraw::Plot1DFiller::Plot1DFiller(TObjArray& _histlist, CompiledExprSource const& _source, char const* _reweight/* = ""*/, Plot1DFiller::OverflowMode _mode/* = kDefault*/) :
  ExprFiller(_histlist, _reweight),
  overflowMode_(_mode)
{
  sources_.push_back(_source);
}

multidraw::Plot1DFiller::Plot1DFiller(Plot1DFiller const& _orig) :
  ExprFiller(_orig),
  overflowMode_(_orig.overflowMode_)
{
}

multidraw::Plot1DFiller::Plot1DFiller(TH1& _hist, Plot1DFiller const& _orig) :
  ExprFiller(_hist, _orig),
  overflowMode_(_orig.overflowMode_)
{
}

multidraw::Plot1DFiller::Plot1DFiller(TObjArray& _histlist, Plot1DFiller const& _orig) :
  ExprFiller(_histlist, _orig),
  overflowMode_(_orig.overflowMode_)
{
}

void
multidraw::Plot1DFiller::doFill_(unsigned _iD, int _icat/* = -1*/)
{
  double x(compiledExprs_[0]->evaluate(_iD));

  if (printLevel_ > 3)
    std::cout << "            Fill(" << x << "; " << entryWeight_ << ")" << std::endl;

  auto& hist(getHist(_icat));

  switch (overflowMode_) {
  case OverflowMode::kDefault:
    break;
  case OverflowMode::kDedicated:
    if (x > hist.GetXaxis()->GetBinLowEdge(hist.GetNbinsX()))
      x = hist.GetXaxis()->GetBinLowEdge(hist.GetNbinsX());
    break;
  case OverflowMode::kMergeLast:
    if (x > hist.GetXaxis()->GetBinUpEdge(hist.GetNbinsX()))
      x = hist.GetXaxis()->GetBinLowEdge(hist.GetNbinsX());
    break;
  }

  hist.Fill(x, entryWeight_);
}

multidraw::ExprFiller*
multidraw::Plot1DFiller::clone_()
{
  if (categorized_) {
    auto& myArray(static_cast<TObjArray&>(tobj_));

    // this array will be deleted in the clone dtor
    auto* array(new TObjArray());
    array->SetOwner(true);

    for (auto* obj : myArray) {
      std::stringstream name;
      name << obj->GetName() << "_thread" << std::this_thread::get_id();

      array->Add(obj->Clone(name.str().c_str()));
    }

    return new Plot1DFiller(*array, *this);
  }
  else {
    auto& myHist(getHist());

    std::stringstream name;
    name << myHist.GetName() << "_thread" << std::this_thread::get_id();

    auto* hist(static_cast<TH1*>(myHist.Clone(name.str().c_str())));

    return new Plot1DFiller(*hist, *this);
  }
}

void
multidraw::Plot1DFiller::mergeBack_()
{
  if (categorized_) {
    auto& cloneSource(static_cast<Plot1DFiller&>(*cloneSource_));
    auto& myArray(static_cast<TObjArray&>(tobj_));

    for (int icat(0); icat < myArray.GetEntries(); ++icat)
      cloneSource.getHist(icat).Add(&getHist(icat));
  }
  else {
    auto& sourceHist(static_cast<TH1&>(cloneSource_->getObj()));
    sourceHist.Add(&getHist());
  }
}
