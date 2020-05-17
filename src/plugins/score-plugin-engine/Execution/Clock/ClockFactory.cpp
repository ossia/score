// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ClockFactory.hpp"

#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/widgets/MessageBox.hpp>

#include <core/document/DocumentView.hpp>

#include <QApplication>

#include <Execution/DocumentPlugin.hpp>
namespace Execution
{

Clock::~Clock() = default;
ClockFactory::~ClockFactory() = default;

Clock::Clock(const Context& ctx)
    : context{ctx}, scenario{context.doc.plugin<DocumentPlugin>().baseScenario()}
{
}

void Clock::play(const TimeVal& t)
{
  try
  {
    play_impl(t, scenario);
    if (auto v = score::IDocument::get<Scenario::ScenarioDocumentPresenter>(context.doc.document))
    {
      v->startTimeBar(scenario.baseInterval().interval());
    }
  }
  catch (const std::runtime_error& e)
  {
    score::warning(qApp->activeWindow(), QObject::tr("Error !"), e.what());
  }
}

void Clock::pause()
{
  pause_impl(scenario);
}

void Clock::resume()
{
  resume_impl(scenario);
}

void Clock::stop()
{
  if (scenario.active())
    stop_impl(scenario);

  if (auto v = score::IDocument::get<Scenario::ScenarioDocumentPresenter>(context.doc.document))
  {
    v->stopTimeBar();
  }
}

bool Clock::paused() const
{
  return false;
}
}
