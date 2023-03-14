#include "../interface/MultiDraw.h"
#include "../interface/FormulaLibrary.h"
#include "../interface/FunctionLibrary.h"

#include "TFile.h"
#include "TBranch.h"
#include "TGraph.h"
#include "TF1.h"
#include "TError.h"
#include "TLeafF.h"
#include "TLeafD.h"
#include "TLeafI.h"
#include "TLeafL.h"
#include "TEntryList.h"
#include "TTreeFormulaManager.h"
#include "TChainElement.h"

#include <stdexcept>
#include <cstring>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <numeric>
#include <memory>
#include <unordered_map>

multidraw::MultiDraw::MultiDraw(char const* _treeName/* = "events"*/) :
  treeName_{_treeName},
  filter_(new Cut(""))
{
}

multidraw::MultiDraw::MultiDraw(MultiDraw const& _orig) :
  treeName_{_orig.treeName_},
  inputPaths_{_orig.inputPaths_},
  entryList_{_orig.entryList_},
  goodRunBranch_{_orig.goodRunBranch_},
  goodRuns_{_orig.goodRuns_},
  weightBranchName_{_orig.weightBranchName_},
  evtNumBranchName_{_orig.evtNumBranchName_},
  inputMultiplexing_{_orig.inputMultiplexing_},
  prescale_{_orig.prescale_},
  filter_(new Cut("", _orig.filter_->getCutExpr())),
  aliases_{_orig.aliases_},
  globalWeight_{_orig.globalWeight_},
  treeWeights_{_orig.treeWeights_},
  branchReplacements_{_orig.branchReplacements_},
  printLevel_{_orig.printLevel_},
  doTimeProfile_{_orig.doTimeProfile_},
  doAbortOnReadError_{_orig.doAbortOnReadError_},
  totalEvents_{_orig.totalEvents_}
{
  for (auto const& ft : _orig.friendTrees_)
    addFriend(std::get<0>(ft), &std::get<1>(ft), std::get<2>(ft));

  for (auto const& cut : _orig.cuts_)
    addCut(cut.first, cut.second->getCutExpr());

  if (_orig.globalReweightSource_)
    globalReweightSource_ = std::make_unique<ReweightSource>(*_orig.globalReweightSource_);

  for (auto const& source : _orig.treeReweightSources_)
    setTreeReweight(source.first, source.second.second, *source.second.first);
}

multidraw::MultiDraw::~MultiDraw()
{
}

void
multidraw::MultiDraw::addFriend(char const* _treeName, TObjArray const* _paths, char const* _alias/* = ""*/)
{
  friendTrees_.emplace_back(_treeName, TObjArray(), _alias);
  auto& ft(friendTrees_.back());
  std::get<1>(ft).SetOwner(true);
  for (auto* path : *_paths)
    std::get<1>(ft).Add(path->Clone());
}

void
multidraw::MultiDraw::setGoodRunBranches(char const* bname1, char const* bname2/* = ""*/)
{
  goodRunBranch_[0] = bname1;
  goodRunBranch_[1] = bname2;
}

void
multidraw::MultiDraw::addGoodRun(unsigned v1, unsigned v2/* = -1*/)
{
  auto& run(goodRuns_[v1]);
  if (v2 != unsigned(-1))
    run.insert(v2);
}

void
multidraw::MultiDraw::setFilter(char const* _expr)
{
  filter_->setCutExpr(_expr);
}

void
multidraw::MultiDraw::addCut(char const* _name, char const* _expr)
{
  if (_name == nullptr || std::strlen(_name) == 0)
    throw std::invalid_argument("Cannot add a cut with no name");

  if (cuts_.count(_name) != 0) {
    std::stringstream ss;
    ss << "Cut named " << _name << " already exists";
    std::cout << ss.str() << std::endl;
    throw std::invalid_argument(ss.str());
  }

  cuts_.emplace(_name, new Cut(_name, _expr));
}

void
multidraw::MultiDraw::addCategory(char const* _cutName, char const* _expr)
{
  auto& cut(findCut_(_cutName));
  cut.addCategory(_expr);
}

void
multidraw::MultiDraw::setCategorization(char const* _cutName, char const* _expr)
{
  auto& cut(findCut_(_cutName));
  cut.setCategorization(_expr);
}

void
multidraw::MultiDraw::addAlias(char const* _name, char const* _expr)
{
  if (_name == nullptr || std::strlen(_name) == 0)
    throw std::invalid_argument("Cannot add an alias with no name");

  aliases_.emplace_back(_name, CompiledExprSource(_expr));
}

void
multidraw::MultiDraw::addAlias(char const* _name, TTreeFunction const& _func)
{
  if (_name == nullptr || std::strlen(_name) == 0)
    throw std::invalid_argument("Cannot add an alias with no name");

  aliases_.emplace_back(_name, CompiledExprSource(_func));
}

void
multidraw::MultiDraw::removeCut(char const* _name)
{
  auto cutItr(cuts_.find(_name));
  if (cutItr == cuts_.end()) {
    std::stringstream ss;
    ss << "Cut \"" << _name << "\" not defined";
    std::cerr << ss.str() << std::endl;
    throw std::runtime_error(ss.str());
  }

  cuts_.erase(cutItr);
}

void
multidraw::MultiDraw::setReweight(char const* _expr, TObject const* _source/* = nullptr*/)
{
  globalReweightSource_ = std::make_unique<ReweightSource>(_expr, _source);
}

void
multidraw::MultiDraw::setReweight(char const* _xexpr, char const* _yexpr, TObject const* _source/* = nullptr*/)
{
  globalReweightSource_ = std::make_unique<ReweightSource>(_xexpr, _yexpr, _source);
}

void
multidraw::MultiDraw::setReweight(ReweightSource const& _source)
{
  globalReweightSource_ = std::make_unique<ReweightSource>(_source);
}

void
multidraw::MultiDraw::setTreeReweight(int _treeNumber, bool _exclusive, char const* _expr, TObject const* _source/* = nullptr*/)
{
  auto& source(treeReweightSources_[_treeNumber]);
  source.first = std::make_unique<ReweightSource>(_expr, _source);
  source.second = _exclusive;
}

void
multidraw::MultiDraw::setTreeReweight(int _treeNumber, bool _exclusive, ReweightSource const& _reweight)
{
  auto& source(treeReweightSources_[_treeNumber]);
  source.first = std::make_unique<ReweightSource>(_reweight);
  source.second = _exclusive;
}

void
multidraw::MultiDraw::setPrescale(unsigned _p, char const* _evtNumBranch/* = ""*/)
{
  if (_p == 0)
    throw std::invalid_argument("Prescale of 0 not allowed");
  prescale_ = _p;
  evtNumBranchName_ = _evtNumBranch;
}

multidraw::Plot1DFiller&
multidraw::MultiDraw::addPlot(TH1* _hist, char const* _expr, char const* _cutName/* = ""*/, char const* _reweight/* = ""*/, Plot1DFiller::OverflowMode _overflowMode/* = kDefault*/)
{
  return addPlot_(_hist, CompiledExprSource(_expr), _cutName, _reweight, _overflowMode);
}

multidraw::Plot1DFiller&
multidraw::MultiDraw::addPlot(TH1* _hist, TTreeFunction const& _func, char const* _cutName/* = ""*/, char const* _reweight/* = ""*/, Plot1DFiller::OverflowMode _overflowMode/* = kDefault*/)
{
  return addPlot_(_hist, CompiledExprSource(_func), _cutName, _reweight, _overflowMode);
}

multidraw::Plot1DFiller&
multidraw::MultiDraw::addPlotList(TObjArray* _histlist, char const* _expr, char const* _cutName/* = ""*/, char const* _reweight/* = ""*/, Plot1DFiller::OverflowMode _overflowMode/* = kDefault*/)
{
  return addPlotList_(_histlist, CompiledExprSource(_expr), _cutName, _reweight, _overflowMode);
}

multidraw::Plot1DFiller&
multidraw::MultiDraw::addPlotList(TObjArray* _histlist, TTreeFunction const& _func, char const* _cutName/* = ""*/, char const* _reweight/* = ""*/, Plot1DFiller::OverflowMode _overflowMode/* = kDefault*/)
{
  return addPlotList_(_histlist, CompiledExprSource(_func), _cutName, _reweight, _overflowMode);
}

multidraw::Plot2DFiller&
multidraw::MultiDraw::addPlot2D(TH2* _hist, char const* _xexpr, char const* _yexpr, char const* _cutName/* = ""*/, char const* _reweight/* = ""*/)
{
  return addPlot2D_(_hist, CompiledExprSource(_xexpr), CompiledExprSource(_yexpr), _cutName, _reweight);
}

multidraw::Plot2DFiller&
multidraw::MultiDraw::addPlot2D(TH2* _hist, TTreeFunction const& _xfunc, TTreeFunction const& _yfunc, char const* _cutName/* = ""*/, char const* _reweight/* = ""*/)
{
  return addPlot2D_(_hist, CompiledExprSource(_xfunc), CompiledExprSource(_yfunc), _cutName, _reweight);
}

multidraw::Plot2DFiller&
multidraw::MultiDraw::addPlotList2D(TObjArray* _histlist, char const* _xexpr, char const* _yexpr, char const* _cutName/* = ""*/, char const* _reweight/* = ""*/)
{
  return addPlotList2D_(_histlist, CompiledExprSource(_xexpr), CompiledExprSource(_yexpr), _cutName, _reweight);
}

multidraw::Plot2DFiller&
multidraw::MultiDraw::addPlotList2D(TObjArray* _histlist, TTreeFunction const& _xfunc, TTreeFunction const& _yfunc, char const* _cutName/* = ""*/, char const* _reweight/* = ""*/)
{
  return addPlotList2D_(_histlist, CompiledExprSource(_xfunc), CompiledExprSource(_yfunc), _cutName, _reweight);
}

multidraw::TreeFiller&
multidraw::MultiDraw::addTree(TTree* _tree, char const* _cutName/* = ""*/, char const* _reweight/* = ""*/)
{
  if (printLevel_ > 1) {
    std::cout << "\nAdding tree " << _tree->GetName() << std::endl;
    if (_cutName != nullptr && std::strlen(_cutName) != 0)
      std::cout << " Cut: " << _cutName << std::endl;
    if (_reweight != nullptr && std::strlen(_reweight) != 0)
      std::cout << " Reweight: " << _reweight << std::endl;
  }

  auto& cut(findCut_(_cutName));

  auto* filler(new TreeFiller(*_tree, _reweight));

  cut.addFiller(std::unique_ptr<ExprFiller>(filler));

  return *filler;
}

multidraw::TreeFiller&
multidraw::MultiDraw::addTreeList(TObjArray* _treelist, char const* _cutName/* = ""*/, char const* _reweight/* = ""*/)
{
  if (printLevel_ > 1) {
    std::cout << "\nAdding tree list with ";
    if (_cutName != nullptr && std::strlen(_cutName) != 0)
      std::cout << " Cut: " << _cutName << std::endl;
    if (_reweight != nullptr && std::strlen(_reweight) != 0)
      std::cout << " Reweight: " << _reweight << std::endl;
  }

  auto& cut(findCut_(_cutName));

  int ncat(cut.getNCategories());
  if (ncat != -1 && ncat !=_treelist->GetEntries())
    throw std::runtime_error("Size of tree list does not match the number of categories");

  auto* filler(new TreeFiller(*_treelist, _reweight));

  cut.addFiller(std::unique_ptr<ExprFiller>(filler));

  return *filler;
}

void
multidraw::MultiDraw::resetReplaceBranch(char const* original)
{
  auto itr{std::find_if(branchReplacements_.begin(), branchReplacements_.end(), [original](auto& bnames)->bool { return bnames.first == original; })};
  if (itr == branchReplacements_.end())
    std::cerr << "Branch " << original << " was not replaced. Doing nothing." << std::endl;
  else
    branchReplacements_.erase(itr);
}

multidraw::Cut&
multidraw::MultiDraw::findCut_(char const* _cutName) const
{
  if (_cutName == nullptr || std::strlen(_cutName) == 0)
    return *filter_;

  auto cutItr(cuts_.find(_cutName));

  if (cutItr == cuts_.end()) {
    std::stringstream ss;
    ss << "Cut \"" << _cutName << "\" not defined";
    std::cerr << ss.str() << std::endl;
    throw std::runtime_error(ss.str());
  }

  return *cutItr->second;
}

multidraw::Plot1DFiller&
multidraw::MultiDraw::addPlot_(TH1* _hist, CompiledExprSource const& _source, char const* _cutName/* = ""*/, char const* _reweight/* = ""*/, Plot1DFiller::OverflowMode _overflowMode/* = kDefault*/)
{
  if (printLevel_ > 1) {
    std::cout << "\nAdding plot " << _hist->GetName() << " with ";
    if (_source.getFormula().Length() != 0)
      std::cout << "expression " << _source.getFormula() << std::endl;
    else
      std::cout << "function " << _source.getFunction()->getName() << std::endl;
    if (_cutName != nullptr && std::strlen(_cutName) != 0)
      std::cout << " Cut: " << _cutName << std::endl;
    if (_reweight != nullptr && std::strlen(_reweight) != 0)
      std::cout << " Reweight: " << _reweight << std::endl;
  }

  auto& cut(findCut_(_cutName));

  auto* filler(new Plot1DFiller(*_hist, _source, _reweight, _overflowMode));

  cut.addFiller(std::unique_ptr<ExprFiller>(filler));

  return *filler;
}

multidraw::Plot1DFiller&
multidraw::MultiDraw::addPlotList_(TObjArray* _histlist, CompiledExprSource const& _source, char const* _cutName/* = ""*/, char const* _reweight/* = ""*/, Plot1DFiller::OverflowMode _overflowMode/* = kDefault*/)
{
  if (printLevel_ > 1) {
    std::cout << "\nAdding plot list with ";
    if (_source.getFormula().Length() != 0)
      std::cout << "expression " << _source.getFormula() << std::endl;
    else
      std::cout << "function " << _source.getFunction()->getName() << std::endl;
    if (_cutName != nullptr && std::strlen(_cutName) != 0)
      std::cout << " Cut: " << _cutName << std::endl;
    if (_reweight != nullptr && std::strlen(_reweight) != 0)
      std::cout << " Reweight: " << _reweight << std::endl;
  }

  auto& cut(findCut_(_cutName));

  int ncat(cut.getNCategories());
  if (ncat != -1 && ncat !=_histlist->GetEntries())
    throw std::runtime_error("Size of histogram list does not match the number of categories");

  auto* filler(new Plot1DFiller(*_histlist, _source, _reweight, _overflowMode));

  cut.addFiller(std::unique_ptr<ExprFiller>(filler));

  return *filler;
}

multidraw::Plot2DFiller&
multidraw::MultiDraw::addPlot2D_(TH2* _hist, CompiledExprSource const& _xsource, CompiledExprSource const& _ysource, char const* _cutName/* = ""*/, char const* _reweight/* = ""*/)
{
  if (printLevel_ > 1) {
    std::cout << "\nAdding Plot " << _hist->GetName() << " with";
    if (_xsource.getFormula().Length() != 0)
      std::cout << " expression " << _ysource.getFormula() << ":" << _xsource.getFormula() << std::endl;
    else
      std::cout << " function " << _ysource.getFunction()->getName() << ":" << _xsource.getFunction()->getName() << std::endl;
    if (_cutName != nullptr && std::strlen(_cutName) != 0)
      std::cout << " Cut: " << _cutName << std::endl;
    if (_reweight != nullptr && std::strlen(_reweight) != 0)
      std::cout << " Reweight: " << _reweight << std::endl;
  }

  auto& cut(findCut_(_cutName));

  auto* filler(new Plot2DFiller(*_hist, _xsource, _ysource, _reweight));

  cut.addFiller(std::unique_ptr<ExprFiller>(filler));

  return *filler;
}

multidraw::Plot2DFiller&
multidraw::MultiDraw::addPlotList2D_(TObjArray* _histlist, CompiledExprSource const& _xsource, CompiledExprSource const& _ysource, char const* _cutName/* = ""*/, char const* _reweight/* = ""*/)
{
  if (printLevel_ > 1) {
    std::cout << "\nAdding plot list with";
    if (_xsource.getFormula().Length() != 0)
      std::cout << " expression " << _ysource.getFormula() << ":" << _xsource.getFormula() << std::endl;
    else
      std::cout << " function " << _ysource.getFunction()->getName() << ":" << _xsource.getFunction()->getName() << std::endl;
    if (_cutName != nullptr && std::strlen(_cutName) != 0)
      std::cout << " Cut: " << _cutName << std::endl;
    if (_reweight != nullptr && std::strlen(_reweight) != 0)
      std::cout << " Reweight: " << _reweight << std::endl;
  }

  auto& cut(findCut_(_cutName));

  int ncat(cut.getNCategories());
  if (ncat != -1 && ncat !=_histlist->GetEntries())
    throw std::runtime_error("Size of histogram list does not match the number of categories");

  auto* filler(new Plot2DFiller(*_histlist, _xsource, _ysource, _reweight));

  cut.addFiller(std::unique_ptr<ExprFiller>(filler));

  return *filler;
}

unsigned
multidraw::MultiDraw::numObjs() const
{
  unsigned n(filter_->getNFillers());
  for (auto& namecut : cuts_)
    n += namecut.second->getNFillers();
  return n;
}

void
multidraw::MultiDraw::execute(long _nEntries/* = -1*/, unsigned long _firstEntry/* = 0*/)
{
  totalEvents_ = 0;

  int abortLevel(gErrorAbortLevel);
  if (doAbortOnReadError_)
    gErrorAbortLevel = kError;

  TChain mainTree(treeName_);

  for (auto& path : inputPaths_)
    mainTree.Add(path);

  mainTree.SetEntryList(entryList_);

  std::vector<std::unique_ptr<TChain>> friendTrees{};

  // This is not needed for single-thread execution, but using this in single-thread makes the code simpler
  SynchTools synchTools;
  synchTools.mainThread = std::this_thread::get_id();

  if (inputMultiplexing_ <= 1) {
    // Single-thread execution

    for (auto& ft : friendTrees_) {
      friendTrees.emplace_back(new TChain(std::get<0>(ft)));
      auto& chain{friendTrees.back()};

      for (auto* path : std::get<1>(ft)) {
        if (path->InheritsFrom(TChainElement::Class()))
          chain->Add(path->GetTitle());
        else
          chain->Add(path->GetName());
      }

      mainTree.AddFriend(chain.get(), std::get<2>(ft));
    }

    totalEvents_ = executeOne_(_nEntries, _firstEntry, mainTree, synchTools);
  }
  else {
    // Multi-thread execution

    if (!friendTrees_.empty()) {
      std::stringstream ss;
      ss << "MultiDraw does not know how to split the input by file when there are friend trees." << std::endl;
      std::cerr << ss.str() << std::endl;
      std::runtime_error(ss.str());
    }

    // Number of input files
    unsigned nTrees(mainTree.GetNtrees());

    // Actual file names (can be different from inputPaths_ which can include wildcards)
    std::vector<TString> fileNames;
    auto& fileElements(*mainTree.GetListOfFiles());
    for (auto* elem : fileElements)
      fileNames.emplace_back(elem->GetTitle());

    std::vector<std::unique_ptr<TChain>> trees;
    std::vector<std::unique_ptr<std::thread>> threads;

    // threads will clone the histograms; need to disable adding to gDirectory
    bool currentTH1AddDirectory(TH1::AddDirectoryStatus());
    TH1::AddDirectory(false);

    // treeOffsets are used in executeOne_ to identify tree transitions and lock the thread
    // Except: newer versions of ROOT is more thread-safe and does not require this lock
    Long64_t* treeOffsets(nullptr);

    auto threadTask([this, &treeOffsets, &synchTools](long _nE, long _fE, TChain* _tree, unsigned _treeNumberOffset) {
#if ROOT_VERSION_CODE < ROOT_VERSION(6,12,0)
        this->executeOne_(_nE, _fE, *_tree, synchTools, _treeNumberOffset, treeOffsets);
#else
        this->executeOne_(_nE, _fE, *_tree, synchTools, _treeNumberOffset);
#endif
      });

    // arguments for the main thread executeOne
    long nEntriesMain(0);
    unsigned long firstEntryMain(0);
    bool byTreeMain(false);
#if ROOT_VERSION_CODE < ROOT_VERSION(6,12,0)
    Long64_t* treeOffsetsMain(nullptr);
#endif

    if (_nEntries == -1 && _firstEntry == 0 && nTrees > inputMultiplexing_) {
      // If there are more trees than threads and we are not limiting the number of entries to process,
      // we can split by file and avoid having to open all files up front with GetEntries.

      if (printLevel_ > 0) {
        std::cout << "Splitting task over " << nTrees;
        std::cout << " files in " << inputMultiplexing_ << " threads" << std::endl;
      }

      unsigned nFilesPerThread(nTrees / inputMultiplexing_);
      // main thread processes the residuals too
      unsigned nFilesMainThread(nTrees - nFilesPerThread * (inputMultiplexing_ - 1));

      for (unsigned iT(0); iT != inputMultiplexing_ - 1; ++iT) {
        auto* tree(new TChain(treeName_));

        unsigned treeNumberOffset(nFilesMainThread + iT * nFilesPerThread);
        TEntryList* threadElist{nullptr};

        if (entryList_ != nullptr) {
          threadElist = new TEntryList(tree);
          threadElist->SetDirectory(nullptr);
        }

        for (unsigned iS(treeNumberOffset); iS != treeNumberOffset + nFilesPerThread; ++iS) {
          auto& fileName(fileNames[iS]);
          tree->Add(fileName);
          if (threadElist != nullptr)
            threadElist->Add(entryList_->GetEntryList(treeName_, fileName));
        }

        if (threadElist != nullptr)
          tree->SetEntryList(threadElist);

        threads.emplace_back(new std::thread(threadTask, -1, 0, tree, treeNumberOffset));
        trees.emplace_back(tree);
      }

      nEntriesMain = nFilesMainThread;
      byTreeMain = true;
    }
    else {
      // If there are only a few files or if we are limiting the entries, we need to know how many
      // events are in each file

      if (printLevel_ > 0) {
        std::cout << "Fetching the total number of events from " << mainTree.GetNtrees();
        std::cout << " files to split the input " << inputMultiplexing_ << " ways" << std::endl;
      }

      // This step also fills the offsets array of the TChain
      unsigned long long nTotal(mainTree.GetEntries() - _firstEntry);

      if (entryList_ != nullptr)
        nTotal = entryList_->GetN() - _firstEntry;

      if (_nEntries >= 0 && _nEntries < (long long)(nTotal))
        nTotal = _nEntries;

      if (nTotal <= _firstEntry)
        return;

      long long nPerThread(nTotal / inputMultiplexing_);

      treeOffsets = mainTree.GetTreeOffset();

      long firstEntry(_firstEntry); // first entry in the full chain
      for (unsigned iT(0); iT != inputMultiplexing_ - 1; ++iT) {
        auto* tree(new TChain(treeName_));

        unsigned treeNumberOffset(0);
        long threadFirstEntry(0);
        TEntryList* threadElist{nullptr};

        if (entryList_ == nullptr) {
          while (firstEntry >= treeOffsets[treeNumberOffset + 1])
            ++treeNumberOffset;

          threadFirstEntry = firstEntry - treeOffsets[treeNumberOffset];
        }
        else {
          long long n(0);
          for (auto* obj : *entryList_->GetLists()) {
            auto* el(static_cast<TEntryList*>(obj));
            if (firstEntry < n + el->GetN())
              break;
            ++treeNumberOffset;
            n += el->GetN();
          }

          threadFirstEntry = firstEntry - n;

          threadElist = new TEntryList(tree);
          threadElist->SetDirectory(nullptr);
        }

        // Add file names from treeNumberOffset to max, but may only use a part (depends on how many events the thread will process)
        for (unsigned iS(treeNumberOffset); iS != fileNames.size(); ++iS) {
          auto& fileName(fileNames[iS]);
          tree->Add(fileName);
          if (threadElist != nullptr)
            threadElist->Add(entryList_->GetEntryList(treeName_, fileName));
        }

        if (threadElist != nullptr)
          tree->SetEntryList(threadElist);

        threads.push_back(std::make_unique<std::thread>(threadTask, nPerThread, threadFirstEntry, tree, treeNumberOffset));
        trees.emplace_back(tree);

        firstEntry += nPerThread;
      }
      
      nEntriesMain = nTotal - (firstEntry - _firstEntry);
      firstEntryMain = firstEntry;
#if ROOT_VERSION_CODE < ROOT_VERSION(6,12,0)
      treeOffsetsMain = treeOffsets;
#endif
    }

    // Started N-1 threads. Process the rest of events (staring from 0) in the main thread

#if ROOT_VERSION_CODE < ROOT_VERSION(6,12,0)
    executeOne_(nEntriesMain, firstEntryMain, mainTree, synchTools, 0, treeOffsetsMain, byTreeMain);
#else
    executeOne_(nEntriesMain, firstEntryMain, mainTree, synchTools, 0, byTreeMain);
#endif

    {
      std::unique_lock<std::mutex> lock(synchTools.mutex);
      synchTools.mainDone = true;
      synchTools.condition.notify_all();
    }

    // Let all threads join first before destroying the trees
    for (auto& thread : threads)
      thread->join();

    threads.clear();

    // Tree deletion should not be concurrent with THx deletion, which happens during the last part of executeOne_ (in Cut dtors)
    for (unsigned iT(0); iT != inputMultiplexing_ - 1; ++iT) {
      auto* threadElist(trees[iT]->GetEntryList());
      trees[iT]->SetEntryList(nullptr);
      delete threadElist;
    }

    TH1::AddDirectory(currentTH1AddDirectory);

    totalEvents_ = synchTools.totalEvents;
  }

  if (doAbortOnReadError_)
    gErrorAbortLevel = abortLevel;

  for (auto& ft : friendTrees)
    mainTree.RemoveFriend(ft.get());

  if (printLevel_ >= 0) {
    std::cout << "\r      " << totalEvents_ << " events" << std::endl;
    if (printLevel_ > 0) {
      auto printCut([this](Cut const& cut) {
          std::cout << "        Cut " << cut.getName() << ": passed total " << cut.getCount() << std::endl;
          if (this->printLevel_ > 1) {
            for (unsigned iF(0); iF != cut.getNFillers(); ++iF) {
              auto* filler(cut.getFiller(iF));
              std::cout << "          " << filler->getObj().GetName() << ": " << filler->getCount() << std::endl;
            }
          }
        });

      printCut(*filter_);

      for (auto& namecut : cuts_) {
        auto& cut(*namecut.second);
        if (namecut.first.Length() != 0 && cut.getNFillers() == 0) // skip non-default cut with no filler
          continue;

        printCut(cut);
      }
    }
  }
}

typedef std::chrono::steady_clock SteadyClock;

double
millisec(SteadyClock::duration const& interval)
{
  return std::chrono::duration_cast<std::chrono::nanoseconds>(interval).count() * 1.e-6;
}

// Global to be used by functions executed within expressions
namespace multidraw {
  thread_local TTree* currentTree{nullptr};
}

long
#if ROOT_VERSION_CODE < ROOT_VERSION(6,12,0)
multidraw::MultiDraw::executeOne_(long _nEntries, unsigned long _firstEntry, TChain& _tree, SynchTools& _synchTools, unsigned _treeNumberOffset/* = 0*/, Long64_t* _treeOffsets/* = nullptr*/, bool _byTree/* = false*/)
#else
multidraw::MultiDraw::executeOne_(long _nEntries, unsigned long _firstEntry, TChain& _tree, SynchTools& _synchTools, unsigned _treeNumberOffset/* = 0*/, bool _byTree/* = false*/)
#endif
{
  // treeNumberOffset: The offset of the given tree with respect to the original

  std::vector<SteadyClock::duration> cutTimers;
  SteadyClock::duration ioTimer(SteadyClock::duration::zero());
  SteadyClock::duration eventTimer(SteadyClock::duration::zero());
  SteadyClock::time_point start;

  bool isMainThread(std::this_thread::get_id() == _synchTools.mainThread);

  int printLevel(-1);
  bool doTimeProfile(false);

  if (isMainThread) {
    printLevel = printLevel_;
    doTimeProfile = doTimeProfile_;
  }

  if (_tree.GetNtrees() == 0) {
    // TTreeFormula compilation crashes if there is no tree in the chain
    if (printLevel >= 0)
      std::cout << "Input tree is empty." << std::endl;

    return 0;
  }

  currentTree = &_tree;

  // Create the repository of all TTreeFormulas
  FormulaLibrary library(_tree);
  // and of all TTreeFunctions
  FunctionLibrary flibrary(_tree);

  // If we have custom-defined aliases, must compile them before cuts and fillers refer to them
  struct AliasSpec {
    TBranch* nbranch{nullptr};
    TBranch* vbranch{nullptr};
    unsigned nD{0};
    std::vector<double> values{};
    std::unique_ptr<CompiledExpr> sourceExpr{};
  };

  std::unique_ptr<TTree> aliasesTree(nullptr);
  std::vector<AliasSpec> aliases;

  //int const maxTreeSize(100);

  if (!aliases_.empty()) {
    {
      std::lock_guard<std::mutex> lock(_synchTools.mutex);
      TDirectory::TContext(nullptr);
      aliasesTree = std::make_unique<TTree>("_aliases", "");
    }

    _tree.AddFriend(aliasesTree.get());

    aliases.reserve(aliases_.size());

    std::vector<TString> negativeMultiplicity;

    // Adding aliases in given order - aliases dependent on others must be declared in order
    for (auto& v : aliases_) {
      auto& name(v.first);
      auto& exprSource(v.second);

      if (_tree.GetBranch(name) != nullptr)
        throw std::runtime_error(("Branch with name " + name + " already exists in the input tree. Cannot define alias.").Data());

      aliases.resize(aliases.size() + 1);
      auto& varspec(aliases.back());

      varspec.sourceExpr = std::move(exprSource.compile(library, flibrary));

      int multiplicity(0);

      if (printLevel >= 1)
        std::cout << " Adding alias " << name;

      if (varspec.sourceExpr->getFormula() != nullptr) {
        if (printLevel >= 1)
          std::cout << " = " << exprSource.getFormula();

        auto* formula(varspec.sourceExpr->getFormula());
        auto* formulaManager(formula->GetManager());
        formulaManager->Sync();

        multiplicity = formulaManager->GetMultiplicity();
      }
      else {
        multiplicity = varspec.sourceExpr->getFunction()->getMultiplicity();

        if (printLevel >= 1)
          std::cout << " = [" << varspec.sourceExpr->getFunction()->getName();
      }

      if (printLevel >= 1)
        std::cout << " (multiplicity " << multiplicity << ")" << std::endl;

      if (multiplicity == 0) {
        // singlet branch
        varspec.values.resize(1);
        varspec.vbranch = aliasesTree->Branch(name, varspec.values.data(), name + "/D");
      }
      else {
        if (multiplicity < 0)
          negativeMultiplicity.push_back(name);

        // multiplicity > 0 or -1 -> number of values may change (case -1: either 0 or 1)
        // give some reasonable initial size
        varspec.values.resize(64);
        // array, or expression composed of dynamic array elements
        varspec.nbranch = aliasesTree->Branch("size__" + name, &varspec.nD, "size__" + name + "/i");
        varspec.vbranch = aliasesTree->Branch(name, varspec.values.data(), name + "[size__" + name + "]/D");
      }
    }

    if (!negativeMultiplicity.empty()) {
      TString names;
      for (unsigned iS(0); iS != negativeMultiplicity.size(); ++iS) {
        names += negativeMultiplicity[iS];
        if (iS != negativeMultiplicity.size() - 1)
          names += ", ";
      }

      if (printLevel >= 1) {
        std::cout << " Aliases " << names << " are singlets but are represented as arrays";
        std::cout << " within MultiDraw. Use index [0] whenever using the alias to ensure";
        std::cout << " we don't try to iterate over the values, especially in an expression";
        std::cout << " used for cuts." << std::endl;
      }
    }
  }

  // Set up the cuts and filler objects
  CutPtr filter;
  std::vector<CutPtr> cuts;

  if (isMainThread) {
    filter = std::move(filter_);
    filter->setPrintLevel(printLevel);
    filter->bindTree(library, flibrary);

    filter->initialize();

    for (auto& namecut : cuts_) {
      if (namecut.first.Length() != 0 && namecut.second->getNFillers() == 0)
        continue;

      cuts.emplace_back(std::move(namecut.second));
      cuts.back()->setPrintLevel(printLevel);
      cuts.back()->bindTree(library, flibrary);

      if (printLevel >= 1)
        std::cout << "Initializing cut \"" << namecut.first << "\"" << std::endl;

      cuts.back()->initialize();
    }
  }
  else {
    filter = filter_->threadClone(library, flibrary);

    for (auto& namecut : cuts_) {
      if (namecut.first.Length() != 0 && namecut.second->getNFillers() == 0)
        continue;

      cuts.emplace_back(namecut.second->threadClone(library, flibrary));

      cuts.back()->initialize();
    }
  }

  if (isMainThread && doTimeProfile)
    cutTimers.assign(1 + cuts.size(), SteadyClock::duration::zero());

  // Compile the reweight expressions
  ReweightPtr globalReweight{nullptr};
  std::unordered_map<unsigned, std::pair<ReweightPtr, bool>> treeReweights;

  if (globalReweightSource_)
    globalReweight = globalReweightSource_->compile(library, flibrary);

  for (auto& tr : treeReweightSources_)
    treeReweights.emplace(tr.first, std::make_pair(tr.second.first->compile(library, flibrary), tr.second.second));

  // Preparing for the event loop
  std::vector<double> eventWeights;

  long long iEntry(0);
  int treeNumber(-1);

  union FloatingPoint {
    Float_t f;
    Double_t d;
  } weight;

  std::function<Double_t()> getWeight([]()->Double_t { return 1.; });

  union Integer {
    Int_t i;
    UInt_t ui;
    Long64_t l;
    ULong64_t ul;
  } evtNum;

  // by default, use iEntry as the event number
  std::function<ULong64_t()> getEvtNum([&iEntry]()->ULong64_t { return iEntry; });

  TBranch* weightBranch(nullptr);
  TBranch* evtNumBranch(nullptr);

  double treeWeight(1.);
  Reweight* treeReweight{nullptr};
  bool exclusiveTreeReweight(false);

  // Applying good run list
  std::function<bool()> isGoodRunEvent;
  TTreeFormula* goodRunBranch[2]{};
  if (goodRunBranch_[0].Length() != 0) {
    goodRunBranch[0] = &library.getFormula(goodRunBranch_[0]);
    if (goodRunBranch_[1].Length() == 0) {
      isGoodRunEvent = [this, goodRunBranch]()->bool {
        goodRunBranch[0]->GetNdata();
        unsigned v1(goodRunBranch[0]->EvalInstance(0));
        return this->goodRuns_.count(v1) != 0;
      };
    }
    else { 
      goodRunBranch[1] = &library.getFormula(goodRunBranch_[1]);
      isGoodRunEvent = [this, goodRunBranch]()->bool {
        goodRunBranch[0]->GetNdata();
        unsigned v1(goodRunBranch[0]->EvalInstance(0));
        auto rItr(this->goodRuns_.find(v1));
        if (rItr == this->goodRuns_.end())
          return false;

        goodRunBranch[1]->GetNdata();
        unsigned v2(goodRunBranch[1]->EvalInstance(0));
        return rItr->second.count(v2) != 0;
      };
    }
  }

  // Replace branches in the expressions
  for (auto& repl : branchReplacements_) {
    library.replaceAll(repl.first, repl.second);
    flibrary.replaceAll(repl.first, repl.second);
  }

#if ROOT_VERSION_CODE < ROOT_VERSION(6,12,0)
  Long64_t* treeOffsets(nullptr);

  if (_treeOffsets != nullptr) {
    if (_byTree) {
      // Jobs split by tree; tree offsets have not been calculated in the main thread
      _tree.GetEntries();
      treeOffsets = _tree.GetTreeOffset();
    }
    else {
      // nextTreeBoundary = _treeOffsets[_treeNumberOffset + treeNumber + 1] - _treeOffsets[_treeNumberOffset]
      //                  = treeOffsets[treeNumber + 1] - treeOffsets[0]
      treeOffsets = &(_treeOffsets[_treeNumberOffset]);
    }
  }

  long nextTreeBoundary(0);
#endif

  bool filterHasAliases(filter->dependsOn(*aliasesTree.get()));

  long nEntries(_byTree ? -1 : _nEntries);

  long printEvery(100000);
  if (printLevel == 3)
    printEvery = 1000;
  else if (printLevel >= 4)
    printEvery = 1;

  if (printLevel >= 0)
    (std::cout << "      0 events").flush();

  while (iEntry != nEntries) {
    if (doTimeProfile)
      start = SteadyClock::now();

    // iEntryNumber != iEntry if tree has a TEntryList set
    long long iEntryNumber(_tree.GetEntryNumber(iEntry + _firstEntry));
    if (iEntryNumber < 0)
      break;

#if ROOT_VERSION_CODE < ROOT_VERSION(6,12,0)
    long long iLocalEntry(0);
    
    if (treeOffsets != nullptr && iEntryNumber >= nextTreeBoundary) {
      // we are crossing a tree boundary in a multi-thread environment
      std::lock_guard<std::mutex> lock(_synchTools.mutex);
      iLocalEntry = _tree.LoadTree(iEntryNumber);
    }
    else {
      iLocalEntry = _tree.LoadTree(iEntryNumber);
    }
#else
    // newer ROOT versions can handle concurrent file transitions
    long long iLocalEntry(_tree.LoadTree(iEntryNumber));
#endif

    if (iLocalEntry < 0)
      break;

    flibrary.setEntry(iEntryNumber);

    ++iEntry;

    // Print progress
    if (iEntry % printEvery == 0) {
      _synchTools.totalEvents += printEvery;

      if (printLevel >= 0) {
        (std::cout << "\r      " << _synchTools.totalEvents.load() << " events").flush();

        if (printLevel > 2)
          std::cout << std::endl;
      }
    }

    if (treeNumber != _tree.GetTreeNumber()) {
      if (_byTree && _nEntries >= 0 && _tree.GetTreeNumber() >= _nEntries) {
        // We are done
        break;
      }

      if (printLevel > 1)
        std::cout << "      Opened a new file: " << _tree.GetCurrentFile()->GetName() << std::endl;

      treeNumber = _tree.GetTreeNumber();

#if ROOT_VERSION_CODE < ROOT_VERSION(6,12,0)
      if (treeOffsets != nullptr)
        nextTreeBoundary = treeOffsets[treeNumber + 1] - treeOffsets[0];
#endif

      if (weightBranchName_.Length() != 0) {
        weightBranch = _tree.GetBranch(weightBranchName_);
        if (!weightBranch)
          throw std::runtime_error(("Could not find branch " + weightBranchName_).Data());

        auto* leaves(weightBranch->GetListOfLeaves());
        if (leaves->GetEntries() == 0) // shouldn't happen
          throw std::runtime_error(("Branch " + weightBranchName_ + " does not have any leaves").Data());

        weightBranch->SetAddress(&weight);

        auto* leaf(static_cast<TLeaf*>(leaves->At(0)));

        if (leaf->InheritsFrom(TLeafF::Class()))
          getWeight = [&weight]()->Double_t { return weight.f; };
        else if (leaf->InheritsFrom(TLeafD::Class()))
          getWeight = [&weight]()->Double_t { return weight.d; };
        else
          throw std::runtime_error(("I do not know how to read the leaf type of branch " + weightBranchName_).Data());
      }

      if (prescale_ > 1 && evtNumBranchName_.Length() != 0) {
        evtNumBranch = _tree.GetBranch(evtNumBranchName_);
        if (!evtNumBranch)
          throw std::runtime_error(("Could not find branch " + evtNumBranchName_).Data());

        auto* leaves(evtNumBranch->GetListOfLeaves());
        if (leaves->GetEntries() == 0) // shouldn't happen
          throw std::runtime_error(("Branch " + evtNumBranchName_ + " does not have any leaves").Data());

        evtNumBranch->SetAddress(&evtNum);

        auto* leaf(static_cast<TLeaf*>(leaves->At(0)));

        if (leaf->InheritsFrom(TLeafI::Class())) {
          if (leaf->IsUnsigned())
            getEvtNum = [&evtNum]()->ULong64_t { return evtNum.ui; };
          else
            getEvtNum = [&evtNum]()->ULong64_t { return evtNum.i; };
        }
        else if (leaf->InheritsFrom(TLeafL::Class())) {
          if (leaf->IsUnsigned())
            getEvtNum = [&evtNum]()->ULong64_t { return evtNum.ul; };
          else
            getEvtNum = [&evtNum]()->ULong64_t { return evtNum.l; };
        }
        else
          throw std::runtime_error(("I do not know how to read the leaf type of branch " + evtNumBranchName_).Data());
      }

      // Underlying tree changed; formulas must update their pointers
      library.updateFormulaLeaves();

      if (isGoodRunEvent && !isGoodRunEvent())
        continue;

      // Constant overall tree weights
      auto wItr(treeWeights_.find(treeNumber + _treeNumberOffset));
      if (wItr == treeWeights_.end())
        treeWeight = globalWeight_;
      else if (wItr->second.second) // exclusive tree-by-tree weight
        treeWeight = wItr->second.first;
      else
        treeWeight = globalWeight_ * wItr->second.first;

      auto rItr(treeReweights.find(treeNumber + _treeNumberOffset));
      if (rItr == treeReweights.end()) {
        treeReweight = globalReweight.get();
        exclusiveTreeReweight = true;
      }
      else {
        treeReweight = rItr->second.first.get();
        exclusiveTreeReweight = (!globalReweight || rItr->second.second);
      }
    }

    if (prescale_ > 1) {
      if (evtNumBranch != nullptr)
        evtNumBranch->GetEntry(iLocalEntry);

      if (printLevel > 3)
        std::cout << "        Event number " << getEvtNum() << std::endl;

      if (getEvtNum() % prescale_ != 0)
        continue;
    }

    // Reset formula cache
    library.resetCache();

    if (doTimeProfile) {
      ioTimer += SteadyClock::now() - start;
      start = SteadyClock::now();
    }

    if (!filterHasAliases) {
      // Optimization in the case when the global filter does not depend on aliases

      bool passFilter(filter->evaluate());

      if (doTimeProfile) {
        cutTimers.back() += SteadyClock::now() - start;
        start = SteadyClock::now();
      }

      if (!passFilter)
        continue;
    }

    if (aliasesTree) {
      // Need to set fReadEntry to the current number first for aliases dependent on other aliases to work
      // Need to set fEntries before fReadEntry (the latter has to be always smaller than the former)
      aliasesTree->SetEntries(aliasesTree->GetEntries() + 1);
      aliasesTree->LoadTree(aliasesTree->GetEntries() - 1);

      for (auto& v : aliases) {
        if (v.nbranch == nullptr) {
          v.sourceExpr->getNdata();
          v.values[0] = v.sourceExpr->evaluate(0);

          if (printLevel > 3)
            std::cout << "        Alias " << v.vbranch->GetName() << ": static value " << v.values[0] << std::endl;

          v.vbranch->Fill();
        }
        else {
          auto* currentData(v.values.data());
          v.nD = v.sourceExpr->getNdata();
          v.values.resize(v.nD);

          if (v.values.data() != currentData) {
            // vector was reallocated
            v.vbranch->SetAddress(v.values.data());
          }

          for (unsigned iD(0); iD != v.nD; ++iD)
            v.values[iD] = v.sourceExpr->evaluate(iD);

          if (printLevel > 3) {
            std::cout << "        Alias " << v.vbranch->GetName() << ": dynamic size " << v.nD;
            std::cout << " values [";
            for (unsigned iD(0); iD != v.nD; ++iD) {
              std::cout << v.values[iD];
              if (iD != v.nD - 1)
                std::cout << ", ";
            }
            std::cout << "]" << std::endl;
          }

          v.nbranch->Fill();
          v.vbranch->Fill();
        }
      }
      
      // tree KeepCircular is a protected method
      // it essentially does the following to keep the in-memory buffer finite
      // buffer size 10000 to have fewer cycles
      // NEVER MIND THIS SEGFAULTS FOR WHATEVER REASON - 22.11.2018
      // if (aliasesTree->GetEntries() > maxTreeSize) {
      //   int newSize(maxTreeSize - maxTreeSize / 10);
      //   for (auto& v : aliases) {
      //     v.vbranch->KeepCircular(newSize);
      //     if (v.nbranch != nullptr)
      //       v.nbranch->KeepCircular(newSize);
      //   }
      //   aliasesTree->SetEntries(newSize);
      // }
    }

    if (filterHasAliases) {
      bool passFilter(filter->evaluate());

      if (doTimeProfile) {
        cutTimers.back() += SteadyClock::now() - start;
        start = SteadyClock::now();
      }

      if (!passFilter)
        continue;
    }

    if (weightBranch != nullptr) {
      weightBranch->GetEntry(iLocalEntry);

      if (printLevel > 3)
        std::cout << "        Input weight " << getWeight() << std::endl;
    }

    if (doTimeProfile) {
      ioTimer += SteadyClock::now() - start;
      start = SteadyClock::now();
    }

    double commonWeight(getWeight() * treeWeight);

    if (treeReweight != nullptr) {
      unsigned nD(treeReweight->getNdata());
      if (!exclusiveTreeReweight)
        nD = std::max(nD, globalReweight->getNdata());
      
      if (nD == 0)
        continue; // skip event

      eventWeights.resize(nD);

      for (unsigned iD(0); iD != nD; ++iD) {
        eventWeights[iD] = treeReweight->evaluate(iD) * commonWeight;
        if (!exclusiveTreeReweight)
          eventWeights[iD] *= globalReweight->evaluate(iD);
      }
    }
    else {
      eventWeights.assign(1, commonWeight);
    }

    if (printLevel > 3) {
      std::cout << "         Global weights: ";
      for (double w : eventWeights)
        std::cout << w << " ";
      std::cout << std::endl;
    }

    if (doTimeProfile) {
      eventTimer += SteadyClock::now() - start;
      start = SteadyClock::now();
    }

    filter->fillExprs(eventWeights);

    if (doTimeProfile) {
      cutTimers.back() += SteadyClock::now() - start;
      start = SteadyClock::now();
    }

    for (unsigned iC(0); iC != cuts.size(); ++iC) {
      if (cuts[iC]->evaluate())
        cuts[iC]->fillExprs(eventWeights);

      if (doTimeProfile) {
        cutTimers[iC] += SteadyClock::now() - start;
        start = SteadyClock::now();
      }
    }
  }

  // Add the residual number of events
  _synchTools.totalEvents += (iEntry % printEvery);

  if (printLevel >= 0 && doTimeProfile) {
    double totalTime(millisec(ioTimer) + millisec(eventTimer));
    totalTime += millisec(std::accumulate(cutTimers.begin(), cutTimers.end(), SteadyClock::duration::zero()));
    std::cout << std::endl;
    std::cout << " Execution time: " << (totalTime / iEntry) << " ms/evt" << std::endl;

    std::cout << "        Time spent on tree input: " << (millisec(ioTimer) / iEntry) << " ms/evt" << std::endl;
    std::cout << "        Time spent on event reweighting: " << (millisec(eventTimer) / iEntry) << " ms/evt" << std::endl;

    if (printLevel > 0) {
      std::cout << "        cut " << filter->getName() << ": ";
      std::cout << (millisec(cutTimers.back()) / iEntry) << " ms/evt" << std::endl;
      for (unsigned iC(0); iC != cuts.size(); ++iC) {
        std::cout << "        cut " << cuts[iC]->getName() << ": ";
        std::cout << (millisec(cutTimers[iC]) / cuts[0]->getCount()) << " ms/evt" << std::endl;
      }
    }
  }

  if (isMainThread) {
    // unlink and return pointers

    filter->unlinkTree();
    filter_ = std::move(filter);

    for (auto& cut : cuts) {
      cut->unlinkTree();
      cuts_[cut->getName()] = std::move(cut);
    }
  }
  else {
    // merge & cleanup

    // Again we'll just lock the entire block
    std::unique_lock<std::mutex> lock(_synchTools.mutex);
    _synchTools.condition.wait(lock, [&_synchTools]() { return _synchTools.mainDone; });

    // Clone fillers will merge themselves to the main object in the destructor of the cuts
    filter.reset();
    cuts.clear();
  }

  if (aliasesTree)
    _tree.RemoveFriend(aliasesTree.get());

  return iEntry;
}
