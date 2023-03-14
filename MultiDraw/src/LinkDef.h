#include "LatinoAnalysis/MultiDraw/interface/CompiledExpr.h"
#include "LatinoAnalysis/MultiDraw/interface/Cut.h"
#include "LatinoAnalysis/MultiDraw/interface/ExprFiller.h"
#include "LatinoAnalysis/MultiDraw/interface/FormulaLibrary.h"
#include "LatinoAnalysis/MultiDraw/interface/FunctionLibrary.h"
#include "LatinoAnalysis/MultiDraw/interface/MultiDraw.h"
#include "LatinoAnalysis/MultiDraw/interface/Plot1DFiller.h"
#include "LatinoAnalysis/MultiDraw/interface/Plot2DFiller.h"
#include "LatinoAnalysis/MultiDraw/interface/Reweight.h"
#include "LatinoAnalysis/MultiDraw/interface/TTreeFormulaCached.h"
#include "LatinoAnalysis/MultiDraw/interface/TTreeFunction.h"
#include "LatinoAnalysis/MultiDraw/interface/TreeFiller.h"

#ifdef __CLING__
#pragma link off all globals;
#pragma link off all classes;
#pragma link off all functions;
#pragma link C++ nestedclass;
#pragma link C++ nestedtypedef;

#pragma link C++ namespace multidraw;
#pragma link C++ class multidraw::CompiledExprSource-;
#pragma link C++ class multidraw::CompiledExpr-;
#pragma link C++ class multidraw::Cut-;
#pragma link C++ class multidraw::ExprFiller-;
#pragma link C++ class multidraw::FormulaLibrary-;
#pragma link C++ class multidraw::TTreeReaderObjectWrapper-;
#pragma link C++ class multidraw::TTreeReaderArrayWrapper-;
#pragma link C++ class multidraw::TTreeReaderValueWrapper-;
#pragma link C++ class multidraw::FunctionLibrary-;
#pragma link C++ class multidraw::MultiDraw-;
#pragma link C++ class multidraw::Plot1DFiller-;
#pragma link C++ class multidraw::Plot2DFiller-;
#pragma link C++ class multidraw::Reweight-;
#pragma link C++ class multidraw::FactorizedReweight-;
#pragma link C++ class multidraw::ReweightSource-;
#pragma link C++ class TTreeFormulaCached+;
#pragma link C++ class multidraw::TTreeFunction-;
#pragma link C++ class multidraw::TreeFiller-;
#endif
