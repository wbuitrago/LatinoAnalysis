#include "../interface/Plot2DFiller.h"
#include "../interface/FormulaLibrary.h"

#include <iostream>
#include <sstream>
#include <thread>

multidraw::Plot2DFiller::Plot2DFiller(TH2& _hist, CompiledExprSource const& _xsource, CompiledExprSource const& _ysource, char const* _reweight/* = ""*/) :
  ExprFiller(_hist, _reweight)
{
  sources_.push_back(_xsource);
  sources_.push_back(_ysource);
}

multidraw::Plot2DFiller::Plot2DFiller(TObjArray& _histlist, CompiledExprSource const& _xsource, CompiledExprSource const& _ysource, char const* _reweight/* = ""*/) :
  ExprFiller(_histlist, _reweight)
{
  sources_.push_back(_xsource);
  sources_.push_back(_ysource);
}

multidraw::Plot2DFiller::Plot2DFiller(Plot2DFiller const& _orig) :
  ExprFiller(_orig)
{
}

multidraw::Plot2DFiller::Plot2DFiller(TH2& _hist, Plot2DFiller const& _orig) :
  ExprFiller(_hist, _orig)
{
}

multidraw::Plot2DFiller::Plot2DFiller(TObjArray& _histlist, Plot2DFiller const& _orig) :
  ExprFiller(_histlist, _orig)
{
}

void
multidraw::Plot2DFiller::doFill_(unsigned _iD, int _icat/* = -1*/)
{
  double x(compiledExprs_[0]->evaluate(_iD));
  double y(compiledExprs_[1]->evaluate(_iD));

  if (printLevel_ > 3)
    std::cout << "            Fill(" << x << ", " << y << "; " << entryWeight_ << ")" << std::endl;

  auto& hist(getHist(_icat));

  hist.Fill(x, y, entryWeight_);
}

multidraw::ExprFiller*
multidraw::Plot2DFiller::clone_()
{
  if (categorized_) {
    auto& myArray(static_cast<TObjArray&>(tobj_));

    // this array will be deleted in the clone ctor
    auto* array(new TObjArray());
    array->SetOwner(true);

    for (auto* obj : myArray) {
      std::stringstream name;
      name << obj->GetName() << "_thread" << std::this_thread::get_id();

      array->Add(obj->Clone(name.str().c_str()));
    }

    return new Plot2DFiller(*array, *this);
  }
  else {
    auto& myHist(getHist());

    std::stringstream name;
    name << myHist.GetName() << "_thread" << std::this_thread::get_id();

    auto* hist(static_cast<TH2*>(myHist.Clone(name.str().c_str())));

    return new Plot2DFiller(*hist, *this);
  }
}

void
multidraw::Plot2DFiller::mergeBack_()
{
  if (categorized_) {
    auto& cloneSource(static_cast<Plot2DFiller&>(*cloneSource_));
    auto& myArray(static_cast<TObjArray&>(tobj_));

    for (int icat(0); icat < myArray.GetEntries(); ++icat)
      cloneSource.getHist(icat).Add(&getHist(icat));
  }
  else {
    auto& sourceHist(static_cast<TH2&>(cloneSource_->getObj()));
    sourceHist.Add(&getHist());
  }
}
