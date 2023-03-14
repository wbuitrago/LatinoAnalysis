#include "../interface/TreeFiller.h"
#include "../interface/FormulaLibrary.h"

#include "TDirectory.h"

#include <iostream>
#include <sstream>
#include <thread>

multidraw::TreeFiller::TreeFiller(TTree& _tree, char const* _reweight) :
  ExprFiller(_tree, _reweight)
{
  if (_tree.GetBranch("weight") == nullptr)
    _tree.Branch("weight", &entryWeight_, "weight/D");
  else
    _tree.SetBranchAddress("weight", &entryWeight_);

  bvalues_.reserve(NBRANCHMAX);
}

multidraw::TreeFiller::TreeFiller(TObjArray& _treelist, char const* _reweight) :
  ExprFiller(_treelist, _reweight)
{
  //for (int iT(0); iT != _treelist.GetEntries(); ++iT) {
  for (auto* obj : _treelist) {
  //auto& tree(static_cast<TTree&>(*_treelist.At(iT)));
    auto& tree(static_cast<TTree&>(*obj));
    if (tree.GetBranch("weight") == nullptr)
      tree.Branch("weight", &entryWeight_, "weight/D");
    else
      tree.SetBranchAddress("weight", &entryWeight_);
  }

  bvalues_.reserve(NBRANCHMAX);
}

multidraw::TreeFiller::TreeFiller(TreeFiller const& _orig) :
  ExprFiller(_orig),
  bnames_(_orig.bnames_)
{
  bvalues_.reserve(NBRANCHMAX);
  bvalues_ = _orig.bvalues_;

  auto setupTree([this](TTree& tree) {
      tree.SetBranchAddress("weight", &this->entryWeight_);
  
      for (unsigned iB(0); iB != this->bvalues_.size(); ++iB)
        tree.SetBranchAddress(this->bnames_[iB], &this->bvalues_[iB]);
  });
  
  if (categorized_) {
    for (auto* obj : static_cast<TObjArray&>(tobj_))
      setupTree(static_cast<TTree&>(*obj));
  }
  else
    setupTree(static_cast<TTree&>(tobj_));
}

multidraw::TreeFiller::TreeFiller(TTree& _tree, TreeFiller const& _orig) :
  ExprFiller(_tree, _orig),
  bnames_(_orig.bnames_)
{
  auto& tree(static_cast<TTree&>(tobj_));

  tree.SetBranchAddress("weight", &entryWeight_);

  bvalues_.reserve(NBRANCHMAX);
  bvalues_ = _orig.bvalues_;

  for (unsigned iB(0); iB != bvalues_.size(); ++iB)
    tree.SetBranchAddress(bnames_[iB], &bvalues_[iB]);
}

multidraw::TreeFiller::TreeFiller(TObjArray& _treelist, TreeFiller const& _orig) :
  ExprFiller(_treelist, _orig),
  bnames_(_orig.bnames_)
{
  bvalues_.reserve(NBRANCHMAX);
  bvalues_ = _orig.bvalues_;
  
  auto& treelist(static_cast<TObjArray&>(tobj_));

  //for (int iT(0); iT != treelist.GetEntries(); ++iT) {
  for (auto* obj : treelist) {
    //auto& tree(static_cast<TTree&>(*treelist.At(iT)));
    auto& tree(static_cast<TTree&>(*obj));

    tree.SetBranchAddress("weight", &entryWeight_);

    for (unsigned iB(0); iB != bvalues_.size(); ++iB)
      tree.SetBranchAddress(bnames_[iB], &bvalues_[iB]);
  }
}

multidraw::TreeFiller::~TreeFiller()
{
  if (cloneSource_ != nullptr) {
    if (categorized_) {
      for (auto* obj : static_cast<TObjArray&>(tobj_))
        static_cast<TTree&>(*obj).ResetBranchAddresses();
    }
    else
      static_cast<TTree&>(tobj_).ResetBranchAddresses();
  }
}

void
multidraw::TreeFiller::addBranch(char const* _bname, char const* _expr, bool _rebind/* = false*/)
{
  if (bvalues_.size() == NBRANCHMAX)
    throw std::runtime_error("Cannot add any more branches");

  bvalues_.resize(bvalues_.size() + 1);

  auto addBranchForTree([_bname, _rebind, this](TTree& tree) {
      if (tree.GetBranch(_bname) != nullptr) {
        if (_rebind)
          tree.SetBranchAddress(_bname, &this->bvalues_.back());
        else
          throw std::runtime_error(TString::Format("Tree already has a branch named %s", _bname).Data());
      }
      else
        tree.Branch(_bname, &this->bvalues_.back(), TString::Format("%s/D", _bname));
  });

  if (categorized_) {
    for (auto* obj : static_cast<TObjArray&>(tobj_))
      addBranchForTree(static_cast<TTree&>(*obj));
  }
  else
    addBranchForTree(static_cast<TTree&>(tobj_));

  bnames_.emplace_back(_bname);
  sources_.emplace_back(_expr);
}

void
multidraw::TreeFiller::doFill_(unsigned _iD, int _icat/* = -1*/)
{
  if (printLevel_ > 3)
    std::cout << "            Fill(";

  for (unsigned iE(0); iE != compiledExprs_.size(); ++iE) {
    bvalues_[iE] = compiledExprs_[iE]->evaluate(_iD);

    if (printLevel_ > 3) {
      std::cout << bvalues_[iE];
      if (iE != compiledExprs_.size() - 1)
        std::cout << ", ";
    }
  }

  if (printLevel_ > 3)
    std::cout << "; " << entryWeight_ << ")" << std::endl;

  getTree(_icat).Fill();
}

multidraw::ExprFiller*
multidraw::TreeFiller::clone_()
{
  if (categorized_) {
    auto& myArray(static_cast<TObjArray&>(tobj_));

    auto* array(new TObjArray());
    array->SetOwner(true);

    for (auto* obj : myArray) {
      std::stringstream name;
      name << obj->GetName() << "_thread" << std::this_thread::get_id();

      auto& myTree(static_cast<TTree&>(*obj));

      // Thread-unsafe - in newer ROOT versions we can do new TTree(name.str().c_str(), myTree.GetTitle(), 99, myTree.GetDirectory());
      TDirectory::TContext(myTree.GetDirectory());
      array->Add(new TTree(name.str().c_str(), myTree.GetTitle()));
    }

    return new TreeFiller(*array, *this);
  }
  else {
    auto& myTree(static_cast<TTree&>(tobj_));

    std::stringstream name;
    name << myTree.GetName() << "_thread" << std::this_thread::get_id();

    // Thread-unsafe - in newer ROOT versions we can do new TTree(name.str().c_str(), myTree.GetTitle(), 99, myTree.GetDirectory());
    TDirectory::TContext(myTree.GetDirectory());
    auto* tree(new TTree(name.str().c_str(), myTree.GetTitle()));

    return new TreeFiller(*tree, *this);
  }
}

void
multidraw::TreeFiller::mergeBack_()
{
  if (categorized_) {
    auto& cloneSource(static_cast<TreeFiller&>(*cloneSource_));
    auto& myArray(static_cast<TObjArray&>(tobj_));

    for (int icat(0); icat < myArray.GetEntries(); ++icat) {
      TObjArray arr;
      arr.Add(&getTree(icat));
      cloneSource.getTree(icat).Merge(&arr);
    }
  }
  else {
    auto& sourceTree(static_cast<TTree&>(cloneSource_->getObj()));
    TObjArray arr;
    arr.Add(&tobj_);
    sourceTree.Merge(&arr);
  }
}
