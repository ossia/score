// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ClockManagerFactory.hpp"

#include <Execution/DocumentPlugin.hpp>
#include <Execution/IntervalComponent.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <core/document/DocumentView.hpp>
#include <score/document/DocumentInterface.hpp>

namespace Execution
{

ClockManager::~ClockManager() = default;
ClockManagerFactory::~ClockManagerFactory() = default;

ClockManager::ClockManager(const Context& ctx)
  : context{ctx}
  , scenario{context.doc.plugin<DocumentPlugin>().baseScenario()}
{
}

void ClockManager::play(const TimeVal& t)
{
  play_impl(t, scenario);
  if(auto v = score::IDocument::get<Scenario::ScenarioDocumentPresenter>(context.doc.document))
  {
    v->startTimeBar(scenario.baseInterval().interval());
  }
}

void ClockManager::pause()
{
  pause_impl(scenario);
}

void ClockManager::resume()
{
  resume_impl(scenario);
}

void ClockManager::stop()
{
  if (scenario.active())
    stop_impl(scenario);

  if(auto v = score::IDocument::get<Scenario::ScenarioDocumentPresenter>(context.doc.document))
  {
    v->stopTimeBar();
  }
}

bool ClockManager::paused() const
{
  return false;
}
}

