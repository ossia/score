#pragma once
#include <Process/ProcessList.hpp>

#include <score/document/DocumentContext.hpp>
class FocusDispatcher;
namespace Process
{
class DataflowManager;
class LayerPresenter;

struct Context : public score::DocumentContext
{
  Context(const score::DocumentContext& doc, DataflowManager& dfm, FocusDispatcher& d)
      : score::DocumentContext{doc}
      , dataflow{dfm}
      , focusDispatcher{d}
      , processList{doc.app.interfaces<Process::LayerFactoryList>()}
  {
  }

  DataflowManager& dataflow;
  FocusDispatcher& focusDispatcher;
  const Process::LayerFactoryList& processList;
};

struct LayerContext
{
  const Context& context;
  Process::LayerPresenter& presenter;
};
}
