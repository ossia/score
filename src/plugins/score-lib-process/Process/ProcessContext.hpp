#pragma once
#include <Process/ProcessList.hpp>

#include <score/document/DocumentContext.hpp>
class FocusDispatcher;
namespace Process
{
class LayerPresenter;

struct ProcessPresenterContext : public score::DocumentContext
{
  ProcessPresenterContext(
      const score::DocumentContext& doc,
      FocusDispatcher& d)
      : score::DocumentContext{doc}
      , focusDispatcher{d}
      , processList{doc.app.interfaces<Process::LayerFactoryList>()}
  {
  }

  FocusDispatcher& focusDispatcher;
  const Process::LayerFactoryList& processList;
};

struct LayerContext
{
  const ProcessPresenterContext& context;
  Process::LayerPresenter& presenter;
};
}
