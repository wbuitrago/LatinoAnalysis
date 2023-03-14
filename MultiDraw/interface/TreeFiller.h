#ifndef multidraw_TreeFiller_h
#define multidraw_TreeFiller_h

#include "ExprFiller.h"

#include "TTree.h"

namespace multidraw {

  //! A wrapper class for TTree
  /*!
   * The class is to be used within MultiDraw, and is instantiated by addTree().
   * Arguments:
   *  tree      The actual tree object (the user is responsible to create it)
   *  library   FormulaLibrary to draw formula objects from
   *  reweight  If provided, evalutaed and used as weight for filling the histogram
   */
  class TreeFiller : public ExprFiller {
  public:
    TreeFiller(TTree& tree, char const* reweight = "");
    TreeFiller(TObjArray& treelist, char const* reweight = "");
    TreeFiller(TreeFiller const&);
    ~TreeFiller();

    void addBranch(char const* bname, char const* expr, bool rebind = false);

    TTree const& getTree(int icat = -1) const { return static_cast<TTree const&>(getObj(icat)); }
    TTree& getTree(int icat = -1) { return static_cast<TTree&>(getObj(icat)); }

    unsigned getNdim() const override { return bvalues_.size(); }

    static unsigned const NBRANCHMAX = 128;

  private:
    TreeFiller(TTree& tree, TreeFiller const&);
    TreeFiller(TObjArray& treelist, TreeFiller const&);

    void doFill_(unsigned, int icat = -1) override;
    ExprFiller* clone_() override;
    void mergeBack_() override;

    std::vector<double> bvalues_{};
    std::vector<TString> bnames_{};
  };

}

#endif
