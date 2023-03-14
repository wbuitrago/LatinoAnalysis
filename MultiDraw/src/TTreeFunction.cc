#include "../interface/TTreeFunction.h"

multidraw::TTreeFunctionPtr
multidraw::TTreeFunction::linkedCopy(FunctionLibrary& _library) const
{
  auto* copy{clone()};
  copy->bindTree_(_library);
  copy->linked_ = true;

  return TTreeFunctionPtr(copy);
}
