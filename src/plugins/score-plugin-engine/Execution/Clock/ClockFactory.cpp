// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ClockFactory.hpp"

#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <Execution/DocumentPlugin.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/widgets/MessageBox.hpp>

#include <core/document/DocumentView.hpp>

#include <QApplication>
namespace Execution
{

Clock::~Clock() = default;
ClockFactory::~ClockFactory() = default;

Clock::Clock(const Context& ctx)
    : context{ctx}
    , scenario{context.doc.plugin<DocumentPlugin>().baseScenario()}
{
  SCORE_ASSERT(scenario);
}

void Clock::play(const TimeVal& t)
{
  SCORE_ASSERT(scenario);
  try
  {
    play_impl(t);
    if(auto v = score::IDocument::get<Scenario::ScenarioDocumentPresenter>(
           context.doc.document))
    {
      v->startTimeBar();
    }
  }
  catch(const std::runtime_error& e)
  {
    score::warning(qApp->activeWindow(), QObject::tr("Error !"), e.what());
  }
}

void Clock::pause()
{
  pause_impl();
}

void Clock::resume()
{
  resume_impl();
}

void Clock::stop()
{
  if(scenario->active())
    stop_impl();

  if(auto v
     = score::IDocument::get<Scenario::ScenarioDocumentPresenter>(context.doc.document))
  {
    v->stopTimeBar();
  }
}

bool Clock::paused() const
{
  return false;
}
}
