// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ClockManagerFactory.hpp"
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <core/document/DocumentView.hpp>
#include <Engine/Executor/BaseScenarioComponent.hpp>
#include <Engine/Executor/IntervalComponent.hpp>
namespace Engine
{
namespace Execution
{

ClockManager::~ClockManager() = default;
ClockManagerFactory::~ClockManagerFactory() = default;

void ClockManager::play(const TimeVal& t)
{
  auto& bs = context.scenario;
  play_impl(t, bs);
  auto view = static_cast<Scenario::ScenarioDocumentView*>(&context.doc.document.view()->viewDelegate());
  view->timeBar().setVisible(context.doc.app.settings<Scenario::Settings::Model>().getTimeBar());
  view->timeBar().playing = true;
  view->timeBar().setInterval(&bs.baseInterval().interval());
}

void ClockManager::pause()
{
  auto& bs = context.scenario;
  pause_impl(bs);
}

void ClockManager::resume()
{
  auto& bs = context.scenario;
  resume_impl(bs);
}

void ClockManager::stop()
{
  auto& bs = context.scenario;
  if(bs.active())
    stop_impl(bs);

  auto view = static_cast<Scenario::ScenarioDocumentView*>(&context.doc.document.view()->viewDelegate());
  view->timeBar().setVisible(false);
  view->timeBar().playing = false;
  view->timeBar().setInterval(nullptr);
}

bool ClockManager::paused() const
{
  return false;
}
}
}
