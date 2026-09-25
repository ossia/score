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

#include <algorithm>
#include <vector>
namespace Execution
{
namespace
{
Clock* g_running{};
int64_t g_playCount{};
std::vector<const score::Document*> g_steppingRequests;
}

Clock::~Clock()
{
  if(g_running == this)
    g_running = nullptr;
}

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
  g_running = this;
  g_playCount++;
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
  requestStepping(context.doc.document, false);
  if(scenario->active())
    stop_impl();
  if(g_running == this)
    g_running = nullptr;

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

bool Clock::setStepping(bool)
{
  return false;
}

bool Clock::stepping() const noexcept
{
  return false;
}

bool Clock::stepTo(double)
{
  return false;
}

Clock* Clock::running(const score::Document& doc) noexcept
{
  if(g_running && &g_running->context.doc.document == &doc)
    return g_running;
  return nullptr;
}

int64_t Clock::playCount() noexcept
{
  return g_playCount;
}

void Clock::requestStepping(const score::Document& doc, bool stepping)
{
  auto it = std::find(g_steppingRequests.begin(), g_steppingRequests.end(), &doc);
  if(stepping && it == g_steppingRequests.end())
    g_steppingRequests.push_back(&doc);
  else if(!stepping && it != g_steppingRequests.end())
    g_steppingRequests.erase(it);
}

bool Clock::steppingRequested(const score::Document& doc) noexcept
{
  return std::find(g_steppingRequests.begin(), g_steppingRequests.end(), &doc)
         != g_steppingRequests.end();
}
}
