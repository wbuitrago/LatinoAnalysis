#ifndef multidraw_MultiDraw_h
#define multidraw_MultiDraw_h

#include "Plot1DFiller.h"
#include "Plot2DFiller.h"
#include "TreeFiller.h"
#include "Cut.h"
#include "Reweight.h"

#include "TChain.h"
#include "TH1.h"
#include "TH2.h"
#include "TTree.h"
#include "TString.h"

#include <vector>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <tuple>
#include <array>

namespace multidraw {

  //! A handy class to fill multiple histograms in one pass over a TChain using string expressions.
  /*!
   * Usage
   *  MultiDraw drawer;
   *  drawer.addInputPath("source.root");
   *  drawer.addCut("baseline", "electrons.loose && TMath::Abs(electrons.eta_) < 1.5");
   *  drawer.addCut("fullsel", "electrons.tight");
   *  TH1D* h1 = new TH1D("h1", "histogram 1", 100, 0., 100.);
   *  TH1D* h2 = new TH1D("h2", "histogram 2", 100, 0., 100.);
   *  drawer.addPlot(h1, "electrons.pt_", "", "baseline");
   *  drawer.addPlot(h2, "electrons.pt_", "", "fullsel");
   *  drawer.fillPlots();
   *
   * In the above example, histogram h1 is filled with the pt of the loose barrel electrons, and
   * h2 with the subset of such electrons that also pass the tight selection.
   * It is also possible to set cuts and reweights for individual plots. Event-wide weights can
   * be set by three methods setWeightBranch, setConstantWeight, and setGlobalReweight.
   */
  class MultiDraw {
  public:
    //! Constructor.
    /*!
     * \param treeName  Name of the input tree.
     */
    MultiDraw(char const* treeName = "events");

    //! Copy constructor.
    /*!
     * All cut and weight expressions are copied, but no histograms / trees are.
     */
    MultiDraw(MultiDraw const&);
    ~MultiDraw();

    //! Cloning for PyROOT
    MultiDraw* clone() { return new MultiDraw(*this); }

    //! Set the tree name.
    void setTreeName(char const* name) { treeName_ = name; }

    //! Add an input file.
    void addInputPath(char const* path) { inputPaths_.emplace_back(path); }

    //! Add a friend tree (not tested)
    void addFriend(char const* treeName, TObjArray const* paths, char const* alias = "");

    //! Apply an entry list.
    void applyEntryList(TEntryList* elist) { entryList_ = elist; }

    //! Set the name of branches to be used for good run filtering.
    void setGoodRunBranches(char const* bname1, char const* bname2 = "");

    //! Add good runs.
    void addGoodRun(unsigned v1, unsigned v2 = -1);

    //! Set the name and the C variable type of the weight branch. Pass an empty string to unset.
    void setWeightBranch(char const* bname) { weightBranchName_ = bname; }

    //! Set a global filtering cut. Events not passing this expression are not considered at all.
    void setFilter(char const* expr);

    //! Add a new cut.
    void addCut(char const* name, char const* expr);

    //! Add a new category.
    void addCategory(char const* cutName, char const* expr);

    //! Set a categorization expression that evaluates to an integer.
    void setCategorization(char const* cutName, char const* expr);

    //! Add a new alias, computed as a function of other branches
    void addAlias(char const* name, char const* expr);

    //! Add a new alias, computed as a function of other branches
    void addAlias(char const* name, TTreeFunction const& func);

    //! Backward compatibility
    void addVariable(char const* name, char const* expr) { addAlias(name, expr); }

    //! Remove a cut.
    void removeCut(char const* name);

    //! Apply a constant overall weight (e.g. luminosity times cross section) to all events
    void setConstantWeight(double w) { globalWeight_ = w; }

    //! Apply an overall weight to specific trees
    /*
     * When exclusive = true, the global weight is not applied to the events
     * from the given tree number. If exclusive = false, the tree-by-tree weight is applied on top
     * of the global one.
     */
    void setTreeWeight(int treeNumber, bool exclusive, double w) { treeWeights_[treeNumber] = std::make_pair(w, exclusive); }

    //! Set an overall reweight
    /*!
     * Reweight factor can be set in two ways. If the second argument is nullptr,
     * the value of expr in every event is used as the weight. If instead a TH1,
     * TGraph, or TF1 is passed as the second argument, the value of expr is used
     * to look up the y value of the source object, which is used as the weight.
     */
    void setReweight(char const* expr, TObject const* source = nullptr);

    //! Set an overall reweight
    /*!
     * Reweight factor can be set in two ways. If the second argument is nullptr,
     * the value of expr in every event is used as the weight. If instead a TH2
     * or TF2 is passed as the second argument, the value of xexpr and yexpr is used
     * to look up the z value of the source object, which is used as the weight.
     */
    void setReweight(char const* xexpr, char const* yexpr, TObject const* source = nullptr);

    //! Set an overall reweight
    /*!
     * Set directly with a ReweightSource object.
     */
    void setReweight(ReweightSource const&);

    //! Set an overall reweight to specific trees
    /*!
     * When exclusive = true, the global reweight is not applied to the events
     * from the given tree number. If exclusive = false, the tree-by-tree weight is applied on top
     * of the global one.
     */
    void setTreeReweight(int treeNumber, bool exclusive, const char* expr, TObject const* source = nullptr);

    //! Set an overall reweight to specific trees
    /*!
     * When exclusive = true, the global reweight is not applied to the events
     * from the given tree number. If exclusive = false, the tree-by-tree weight is applied on top
     * of the global one.
     */
    void setTreeReweight(int treeNumber, bool exclusive, ReweightSource const&);

    //! Set a prescale factor
    /*!
     * When prescale_ > 1, only events that satisfy eventNumber % prescale_ == 0 are read.
     * If evtNumBranch is set, branch of the given name is read as the eventNumber. The
     * tree entry number is used otherwise.
     */
    void setPrescale(unsigned p, char const* evtNumBranch = "");

    //! Add a 1D histogram to fill.
    Plot1DFiller& addPlot(TH1* hist, char const* expr, char const* cutName = "", char const* reweight = "", Plot1DFiller::OverflowMode mode = Plot1DFiller::kDefault);

    Plot1DFiller& addPlot(TH1* hist, TTreeFunction const& func, char const* cutName = "", char const* reweight = "", Plot1DFiller::OverflowMode mode = Plot1DFiller::kDefault);

    //! Add a 2D histogram to fill.
    Plot2DFiller& addPlot2D(TH2* hist, char const* xexpr, char const* yexpr, char const* cutName = "", char const* reweight = "");

    Plot2DFiller& addPlot2D(TH2* hist, TTreeFunction const& xfunc, TTreeFunction const& yfunc, char const* cutName = "", char const* reweight = "");

    //! Add 1D histograms to fill by a category group.
    Plot1DFiller& addPlotList(TObjArray* histlist, char const* expr, char const* cutName, char const* reweight = "", Plot1DFiller::OverflowMode mode = Plot1DFiller::kDefault);

    Plot1DFiller& addPlotList(TObjArray* histlist, TTreeFunction const& func, char const* cutName, char const* reweight = "", Plot1DFiller::OverflowMode mode = Plot1DFiller::kDefault);

    //! Add 2D histograms to fill by a category group.
    Plot2DFiller& addPlotList2D(TObjArray* histlist, char const* xexpr, char const* yexpr, char const* cutName, char const* reweight = "");

    Plot2DFiller& addPlotList2D(TObjArray* histlist, TTreeFunction const& xexpr, TTreeFunction const& yexpr, char const* cutName, char const* reweight = "");
    
    //! Add a tree to fill.
    TreeFiller& addTree(TTree* tree, char const* cutName = "", char const* reweight = "");

    //! Add trees to fill by a category group.
    TreeFiller& addTreeList(TObjArray* treelist, char const* cutName = "", char const* reweight = "");

    //! Replace a branch appearing in all compiled expressions with another.
    void replaceBranch(char const* from, char const* to) { branchReplacements_.emplace_back(from, to); }

    //! Reset the branch replacement
    void resetReplaceBranch(char const* original);

    //! Run and fill the plots and trees.
    void execute(long nEntries = -1, unsigned long firstEntry = 0);

    //! Set input tree multiplexing.
    /*
     * If multiplex > 1, execute() will launch multiple processes to process parts of the input. This
     * can speed up the overall processing unless I/O is saturated.
     */
    void setInputMultiplexing(unsigned mux) { inputMultiplexing_ = mux; }

    //! Set the print level.
    /*
     * Level -1: silent
     * Level  0: print number of events every 10000
     * Level  1: additionally print the summary of execution
     * Level  2: display info about each added plot/tree and each new file
     * Level  3: print every 100 events
     * Level  4: debug print out
     */
    void setPrintLevel(int l) { printLevel_ = l; }

    //! Set time profiling switch.
    /*
     * If true, execute() call records execution times of various parts of the function and prints the
     * summary according to the print level.
     */
    void setDoTimeProfile(bool d) { doTimeProfile_ = d; }

    //! Abort if there is a read error.
    /*
     * By default, TChain skips files that cannot be opened or data blocks that cannot be read. When this
     * flag is true, MultiDraw throws an exception upon file open error.
     */
    void setAbortOnReadError(bool a) { doAbortOnReadError_ = a; }

    long getTotalEvents() const { return totalEvents_; }

    unsigned numObjs() const;

  private:
    //! Handle addPlot and addTree with the same interface (requires a callback to generate the right object)
    Cut& findCut_(char const* cutName) const;

    Plot1DFiller& addPlot_(TH1* hist, CompiledExprSource const& source, char const* cutName, char const* reweight, Plot1DFiller::OverflowMode mode);
    Plot2DFiller& addPlot2D_(TH2* hist, CompiledExprSource const& xsource, CompiledExprSource const& ysource, char const* cutName, char const* reweight);
    Plot1DFiller& addPlotList_(TObjArray* histlist, CompiledExprSource const& source, char const* cutName, char const* reweight, Plot1DFiller::OverflowMode mode);
    Plot2DFiller& addPlotList2D_(TObjArray* histlist, CompiledExprSource const& xsource, CompiledExprSource const& ysource, char const* cutName, char const* reweight);
    
    struct SynchTools {
      std::thread::id mainThread;
      std::mutex mutex;
      std::condition_variable condition;
      bool mainDone{false};
      std::atomic_ullong totalEvents{0};
    };

    //! Core of the execute function
    /*
      treeNumberOffset: The offset of the given tree with respect to the original
      byTree: If true, nEntries refers to file numbers in the given tree, not events
     */
#if ROOT_VERSION_CODE < ROOT_VERSION(6,12,0)
    long executeOne_(long nEntries, unsigned long firstEntry, TChain&, SynchTools&, unsigned treeNumberOffset = 0, Long64_t* treeOffsets = nullptr, bool byTree = false);
#else
    long executeOne_(long nEntries, unsigned long firstEntry, TChain&, SynchTools&, unsigned treeNumberOffset = 0, bool byTree = false);
#endif

    TString treeName_{"events"};
    std::vector<TString> inputPaths_{};

    std::vector<std::tuple<TString, TObjArray, TString>> friendTrees_{};

    TEntryList* entryList_{nullptr};

    std::array<TString, 2> goodRunBranch_{};
    std::map<unsigned, std::set<unsigned>> goodRuns_{};

    TString weightBranchName_{"weight"};
    TString evtNumBranchName_{""};

    unsigned inputMultiplexing_{1};
    unsigned prescale_{1};

    CutPtr filter_{};
    std::map<TString, CutPtr> cuts_{};
    std::vector<std::pair<TString, CompiledExprSource>> aliases_{};

    double globalWeight_{1.};
    ReweightSourcePtr globalReweightSource_{};
    std::map<unsigned, std::pair<double, bool>> treeWeights_{};
    std::map<unsigned, std::pair<ReweightSourcePtr, bool>> treeReweightSources_{};

    std::list<std::pair<TString, TString>> branchReplacements_{};

    int printLevel_{0};
    bool doTimeProfile_{false};
    bool doAbortOnReadError_{false};

    long long totalEvents_{0};
  };

}

#endif
