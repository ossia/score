#pragma once
#include <iscore/document/DocumentContext.hpp>
#include <Process/ProcessList.hpp>
class FocusDispatcher;
namespace Process
{
class LayerPresenter;

struct ProcessPresenterContext : public iscore::DocumentContext
{
  ProcessPresenterContext(
      const iscore::DocumentContext& doc, FocusDispatcher& d)
      : iscore::DocumentContext{doc}, focusDispatcher{d},
        processList{doc.app.interfaces<Process::LayerFactoryList>()}
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
