#ifndef multidraw_TTreeFormulaCached_h
#define multidraw_TTreeFormulaCached_h

#include "TTreeFormula.h"

#include <vector>
#include <utility>
#include <memory>

//! Cached version of TTreeFormula.
/*!
 * Only the expression values are cached. GetNdata() must be called before calls to EvalInstance.
 * Note: override keyword in this class definition is commented out to avoid getting compiler
 * warnings (-Winconsistent-missing-override).
 */
class TTreeFormulaCached : public TTreeFormula {
public:
  struct Cache {
    std::vector<std::pair<Bool_t, Double_t>> fValues{};
  };

  typedef std::shared_ptr<Cache> CachePtr;

  TTreeFormulaCached(char const* name, char const* formula, TTree* tree, CachePtr const&);
  TTreeFormulaCached(char const* name, char const* formula, TTree* tree);
  TTreeFormulaCached(TTreeFormulaCached const&);
  ~TTreeFormulaCached() {}

  TTreeFormulaCached& operator=(TTreeFormulaCached const&);

  Int_t GetNdata()/* override*/;
  Double_t EvalInstance(Int_t, char const* [] = nullptr)/* override*/;

  CachePtr const& GetCache() const { return fCache; }

  TObjArray const* GetListOfLeaves() const { return &fLeaves; }

  bool ReplaceLeaf(TString const& from, TString const& to);

private:
  void ConvertSubformulas();

  CachePtr fCache{};

  ClassDef(TTreeFormulaCached, 1)
};

/*
  TFormula has no foolproof mechanism to signal a failure of expression compilation.
  For normal expressions like TTreeFormula f("formula", "bogus", tree), we get f.GetTree() == 0.
  However if a TTreePlayer function (Sum$, Max$, etc.) is used, the top-level expression
  is considered valid even if the enclosed expression is not, and GetTree() returns the tree
  address.
  The only way we catch all compilation failures is to use the error message using ROOT
  error handling mechanism.
  Functions NewTTreeFormula and NewTTreeFormulaCached are written for this purpose. They are
  just `new TTreeFormula(Cached)` calls, wrapped in error handling routines.
*/

//! Create a TTreeFormula with error handling
TTreeFormula* NewTTreeFormula(char const* name, char const* expr, TTree*, bool silent = false);

//! Create a TTreeFormulaCached with error handling
TTreeFormulaCached* NewTTreeFormulaCached(char const* name, char const* expr, TTree*, TTreeFormulaCached::CachePtr const&, bool silent = false);

#endif
