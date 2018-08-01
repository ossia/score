// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ClockManagerFactory.hpp"

#include <Execution/DocumentPlugin.hpp>
#include <Execution/IntervalComponent.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <core/document/DocumentView.hpp>

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
  if(auto v = context.doc.document.view())
  {
    auto view = static_cast<Scenario::ScenarioDocumentView*>(
        &v->viewDelegate());
    view->timeBar().setVisible(
        context.doc.app.settings<Scenario::Settings::Model>().getTimeBar());
    view->timeBar().playing = true;
    view->timeBar().setInterval(&scenario.baseInterval().interval());
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

  auto view = static_cast<Scenario::ScenarioDocumentView*>(
      &context.doc.document.view()->viewDelegate());
  view->timeBar().setVisible(false);
  view->timeBar().playing = false;
  view->timeBar().setInterval(nullptr);
}

bool ClockManager::paused() const
{
  return false;
}
}

