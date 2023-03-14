#ifndef multidraw_TTreeFunction_h
#define multidraw_TTreeFunction_h

#include <memory>

namespace multidraw {

  class FunctionLibrary;

  class TTreeFunction {
  public:
    TTreeFunction() {}
    virtual ~TTreeFunction() {}

    virtual char const* getName() const = 0;
    virtual TTreeFunction* clone() const = 0;
    
    std::unique_ptr<TTreeFunction> linkedCopy(FunctionLibrary&) const;

    bool isLinked() const { return linked_; }

    virtual void beginEvent(long long) {}
    virtual int getMultiplicity() { return 0; }
    virtual unsigned getNdata() = 0;
    virtual double evaluate(unsigned) = 0;

  protected:
    virtual void bindTree_(FunctionLibrary&) = 0;

  private:
    bool linked_{false};
  };

  typedef std::unique_ptr<TTreeFunction> TTreeFunctionPtr;

}

#endif
