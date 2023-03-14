#ifndef multidraw_TTreeReaderLibrary_h
#define multidraw_TTreeReaderLibrary_h

#include "TTreeFunction.h"

#include "TTreeReader.h"
#include "TTreeReaderArray.h"
#include "TTreeReaderValue.h"
#include "TString.h"

#include <unordered_map>
#include <memory>
#include <functional>

namespace multidraw {

  // Wrappers to facilitate replaceAll() of FunctionLibrary
  // Background: Functions objects will already have obtained direct pointers to the
  // TTreeReaderArray/Value objects. I didn't think far enough ahead when designing
  // FunctionLibrary - another layer of indirection (e.g. handing a pointer of pointer)
  // would have been better to realize a replaceAll feature. To keep the same memory
  // address and still be able to replace the object, I use a wrapper + non-allocating
  // operator new / delete.

  class TTreeReaderObjectWrapper {
  public:
    TTreeReaderObjectWrapper() {}
    virtual ~TTreeReaderObjectWrapper() {}
    ROOT::Internal::TTreeReaderValueBase* get() const { return obj_; }
    virtual void replace(TTreeReader& tr, char const* branchName = nullptr) = 0;

  protected:
    ROOT::Internal::TTreeReaderValueBase* obj_{nullptr};
  };

  typedef std::unique_ptr<TTreeReaderObjectWrapper> TTreeReaderObjectPtr;

  template<typename T>
  class TTreeReaderArrayWrapper : public TTreeReaderObjectWrapper {
  public:
    TTreeReaderArrayWrapper(TTreeReader& tr, char const* branchName);
    ~TTreeReaderArrayWrapper();
    void replace(TTreeReader& tr, char const* branchName = nullptr) override;
  };

  template<typename T>
  class TTreeReaderValueWrapper : public TTreeReaderObjectWrapper {
  public:
    TTreeReaderValueWrapper(TTreeReader& tr, char const* branchName);
    ~TTreeReaderValueWrapper();
    void replace(TTreeReader& tr, char const* branchName = nullptr) override;
  };

  class FunctionLibrary {
  public:
    FunctionLibrary(TTree& tree) : reader_(new TTreeReader(&tree)) {}
    ~FunctionLibrary();

    //! Set the TTreeReader entry number and call beginEvent() of all linked functions
    void setEntry(long long iEntry);

    TTreeFunction& getFunction(TTreeFunction const&);

    template<typename T> TTreeReaderArray<T>& getArray(char const*);
    template<typename T> TTreeReaderValue<T>& getValue(char const*);

    // convenience
    template<class T> void bindBranch(TTreeReaderArray<T>*&, char const*);
    template<class T> void bindBranch(TTreeReaderValue<T>*&, char const*);

    // swap branch pointers
    void replaceAll(char const* from, char const* to);

    void addDestructorCallback(std::function<void(void)> const& f) { destructorCallbacks_.push_back(f); }

  private:
    std::unique_ptr<TTreeReader> reader_{};
    std::unordered_map<std::string, TTreeReaderObjectPtr> branchReaders_{};
    std::unordered_map<TTreeFunction const*, std::unique_ptr<TTreeFunction>> functions_{};

    std::vector<std::function<void(void)>> destructorCallbacks_;
  };
}

#include <stdexcept>
#include <sstream>

template<typename T>
multidraw::TTreeReaderArrayWrapper<T>::TTreeReaderArrayWrapper(TTreeReader& _tr, char const* _branchName) :
  TTreeReaderObjectWrapper()
{
  void* mem{::operator new(sizeof(TTreeReaderArray<T>))};
  obj_ = new(mem) TTreeReaderArray<T>(_tr, _branchName);
}

template<typename T>
multidraw::TTreeReaderArrayWrapper<T>::~TTreeReaderArrayWrapper()
{
  static_cast<TTreeReaderArray<T>*>(obj_)->~TTreeReaderArray<T>();
  ::operator delete(obj_);
}

template<typename T>
void
multidraw::TTreeReaderArrayWrapper<T>::replace(TTreeReader& _tr, char const* _branchName/* = nullptr*/)
{
  static_cast<TTreeReaderArray<T>*>(obj_)->~TTreeReaderArray<T>();
  new(obj_) TTreeReaderArray<T>(_tr, _branchName);
}

template<typename T>
multidraw::TTreeReaderValueWrapper<T>::TTreeReaderValueWrapper(TTreeReader& _tr, char const* _branchName) :
  TTreeReaderObjectWrapper()
{
  void* mem{::operator new(sizeof(TTreeReaderValue<T>))};
  obj_ = new(mem) TTreeReaderValue<T>(_tr, _branchName);
}

template<typename T>
multidraw::TTreeReaderValueWrapper<T>::~TTreeReaderValueWrapper()
{
  static_cast<TTreeReaderValue<T>*>(obj_)->~TTreeReaderValue<T>();
  ::operator delete(obj_);
}

template<typename T>
void
multidraw::TTreeReaderValueWrapper<T>::replace(TTreeReader& _tr, char const* _branchName/* = nullptr*/)
{
  static_cast<TTreeReaderValue<T>*>(obj_)->~TTreeReaderValue<T>();
  new(obj_) TTreeReaderValue<T>(_tr, _branchName);
}

template<typename T>
TTreeReaderArray<T>&
multidraw::FunctionLibrary::getArray(char const* _bname)
{
  auto rItr(branchReaders_.find(_bname));
  if (rItr == branchReaders_.end()) {
    if (reader_->GetTree()->GetBranch(_bname) == nullptr) {
      std::stringstream ss;
      ss << "Branch " << _bname << " does not exist" << std::endl;
      throw std::runtime_error(ss.str());
    }
    auto* arr(new TTreeReaderArrayWrapper<T>(*reader_, _bname));
    // TODO want to check if T is the correct type
    rItr = branchReaders_.emplace(_bname, arr).first;
  }
  else if (dynamic_cast<TTreeReaderArray<T>*>(rItr->second->get()) == nullptr) {
    std::stringstream ss;
    ss << "Branch " << _bname << " is not an array" << std::endl;
    throw std::runtime_error(ss.str());
  }

  return *static_cast<TTreeReaderArray<T>*>(rItr->second->get());
}

template<typename T>
TTreeReaderValue<T>&
multidraw::FunctionLibrary::getValue(char const* _bname)
{
  auto rItr(branchReaders_.find(_bname));
  if (rItr == branchReaders_.end()) {
    if (reader_->GetTree()->GetBranch(_bname) == nullptr) {
      std::stringstream ss;
      ss << "Branch " << _bname << " does not exist" << std::endl;
      throw std::runtime_error(ss.str());
    }
    auto* val(new TTreeReaderValueWrapper<T>(*reader_, _bname));
    rItr = branchReaders_.emplace(_bname, val).first; 
  }
  else if (dynamic_cast<TTreeReaderValue<T>*>(rItr->second->get()) == nullptr) {
    std::stringstream ss;
    ss << "Branch " << _bname << " is not a value" << std::endl;
    throw std::runtime_error(ss.str());
  }

  return *static_cast<TTreeReaderValue<T>*>(rItr->second->get());
}

template<class T>
void
multidraw::FunctionLibrary::bindBranch(TTreeReaderValue<T>*& _reader, char const* _bname)
{
  _reader = &getValue<T>(_bname);
}

template<class T>
void
multidraw::FunctionLibrary::bindBranch(TTreeReaderArray<T>*& _reader, char const* _bname)
{
  _reader = &getArray<T>(_bname);
}

typedef TTreeReaderArray<Float_t> FloatArrayReader;
typedef TTreeReaderValue<Float_t> FloatValueReader;
typedef TTreeReaderArray<Int_t> IntArrayReader;
typedef TTreeReaderValue<Int_t> IntValueReader;
typedef TTreeReaderArray<Bool_t> BoolArrayReader;
typedef TTreeReaderValue<Bool_t> BoolValueReader;
typedef TTreeReaderArray<UChar_t> UCharArrayReader;
typedef TTreeReaderValue<UChar_t> UCharValueReader;
typedef TTreeReaderArray<UInt_t> UIntArrayReader;
typedef TTreeReaderValue<UInt_t> UIntValueReader;
typedef TTreeReaderValue<ULong64_t> ULong64ValueReader;
typedef std::unique_ptr<FloatArrayReader> FloatArrayReaderPtr;
typedef std::unique_ptr<FloatValueReader> FloatValueReaderPtr;
typedef std::unique_ptr<IntArrayReader> IntArrayReaderPtr;
typedef std::unique_ptr<IntValueReader> IntValueReaderPtr;
typedef std::unique_ptr<BoolArrayReader> BoolArrayReaderPtr;
typedef std::unique_ptr<BoolValueReader> BoolValueReaderPtr;
typedef std::unique_ptr<UCharArrayReader> UCharArrayReaderPtr;
typedef std::unique_ptr<UCharValueReader> UCharValueReaderPtr;
typedef std::unique_ptr<UIntArrayReader> UIntArrayReaderPtr;
typedef std::unique_ptr<UIntValueReader> UIntValueReaderPtr;
typedef std::unique_ptr<ULong64ValueReader> ULong64ValueReaderPtr;

#endif
