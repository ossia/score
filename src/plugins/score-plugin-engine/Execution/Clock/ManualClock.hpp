#pragma once

#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <ossia/editor/scenario/time_value.hpp>

#include <QToolBar>

#include <Execution/Clock/ClockFactory.hpp>
#include <Execution/Clock/DefaultClock.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <verdigris>
#include <QMainWindow>

namespace Execution
{
namespace ManualClock
{

class TimeWidget : public QToolBar
{
  W_OBJECT(TimeWidget)
public:
  TimeWidget(QWidget* parent = nullptr)
  {
    {
      auto pb = addAction("+1");
      connect(pb, &QAction::triggered, this, [=] { advance(1); });
    }

    {
      auto pb = addAction("+5");
      connect(pb, &QAction::triggered, this, [=] { advance(5); });
    }

    {
      auto pb = addAction("+100");
      connect(pb, &QAction::triggered, this, [=] { advance(100); });
    }
  }

  void advance(int arg_1) W_SIGNAL(advance, arg_1);
};

class Clock final : public QObject,
                    public Execution::Clock,
                    public Nano::Observer
{
public:
  Clock(const Execution::Context& ctx) : Execution::Clock{ctx}, m_default{ctx}
  {
  }

  ~Clock() override {}

private:
  TimeWidget* m_widg{};
  // Clock interface
  void play_impl(const TimeVal& t, Execution::BaseScenarioElement& bs) override
  {
    m_paused = false;

    m_widg = new TimeWidget;
    if (context.doc.app.mainWindow)
    {
      context.doc.app.mainWindow->addToolBar(
          Qt::ToolBarArea::BottomToolBarArea, m_widg);
    }
    QObject::connect(m_widg, &TimeWidget::advance, this, [=](int val) {
      using namespace ossia;
      ossia::time_interval& itv = *scenario.baseInterval().OSSIAInterval();
      ossia::time_value time{val};
      ossia::token_request tok{};
      itv.tick_offset(time, 0_tv, tok);
    });
    m_widg->show();

    m_default.play(t);

    resume_impl(bs);
  }
  void pause_impl(Execution::BaseScenarioElement&) override {}
  void resume_impl(Execution::BaseScenarioElement&) override {}
  void stop_impl(Execution::BaseScenarioElement&) override
  {
    delete m_widg;
    context.doc.plugin<DocumentPlugin>().finished();
  }
  bool paused() const override { return m_paused; }

  Execution::DefaultClock m_default;
  bool m_paused{};
};

/*
class ClockFactory final : public Execution::ClockFactory
{
  SCORE_CONCRETE("5e8d0f1b-752f-4e29-8c8c-ecd65bd69806")

  QString prettyName() const override { return QObject::tr("Manual"); }

  std::unique_ptr<Execution::Clock>
  make(const Execution::Context& ctx) override
  {
    return std::make_unique<Clock>(ctx);
  }

  Execution::time_function
  makeTimeFunction(const score::DocumentContext& ctx) const override
  {
    constexpr double rate = 44100;
    return [=](const TimeVal& v) -> ossia::time_value {
      return v.isInfinite()
                 ? ossia::Infinite
                 : ossia::time_value{std::llround(rate * v.msec() / 1000.)};
    };
  }

  Execution::reverse_time_function
  makeReverseTimeFunction(const score::DocumentContext& ctx) const override
  {
    constexpr double rate = 44100;
    return [=](const ossia::time_value& v) -> TimeVal {
      return v.infinite() ? TimeVal::infinite()
                          : TimeVal::fromMsecs(1000. * v.impl / rate);
    };
  }
};
*/
}
}
