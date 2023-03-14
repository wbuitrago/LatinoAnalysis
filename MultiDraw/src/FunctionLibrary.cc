#include "../interface/FunctionLibrary.h"

multidraw::FunctionLibrary::~FunctionLibrary()
{
  for (auto& f : destructorCallbacks_)
    f();
}

void
multidraw::FunctionLibrary::setEntry(long long _iEntry)
{
  reader_->SetEntry(_iEntry);
  for (auto& fct : functions_)
    fct.second->beginEvent(_iEntry);
}

multidraw::TTreeFunction&
multidraw::FunctionLibrary::getFunction(TTreeFunction const& _source)
{
  auto fItr(functions_.find(&_source));
  if (fItr == functions_.end())
    fItr = functions_.emplace(&_source, _source.linkedCopy(*this)).first;

  return *fItr->second.get();
}

void
multidraw::FunctionLibrary::replaceAll(char const* _from, char const* _to)
{
  auto fItr(branchReaders_.find(_from));
  if (fItr == branchReaders_.end())
    return;

  fItr->second->replace(*reader_, _to);
}
