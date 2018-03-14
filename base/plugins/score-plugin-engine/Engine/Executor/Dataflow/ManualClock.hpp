#pragma once

#include <Engine/Executor/ClockManager/ClockManagerFactory.hpp>
#include <Engine/Executor/ClockManager/DefaultClockManager.hpp>
#include <Engine/Executor/IntervalComponent.hpp>

#include <Engine/Executor/DocumentPlugin.hpp>

#include <QPushButton>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QToolBar>
namespace Engine
{
namespace ManualClock
{

class TimeWidget : public QToolBar
{
    Q_OBJECT
  public:
    TimeWidget(QWidget* parent = nullptr)
    {
      {
        auto pb = addAction("+1");
        connect(pb, &QAction::triggered,
                this, [=] { advance(1); });
      }

      {
        auto pb = addAction("+5");
        connect(pb, &QAction::triggered,
                this, [=] { advance(5); });
      }

      {
        auto pb = addAction("+100");
        connect(pb, &QAction::triggered,
                this, [=] { advance(100); });
      }
    }

    Q_SIGNAL void advance(int);
};

class Clock final
    : public QObject
    , public Engine::Execution::ClockManager
    , public Nano::Observer
{
    public:
        Clock(const Engine::Execution::Context& ctx):
          ClockManager{ctx}
        , m_default{ctx}
        , m_plug{ctx.plugin}
        {

        }

        ~Clock() override
        {

        }

    private:
        TimeWidget* m_widg{};
        // Clock interface
        void play_impl(
                const TimeVal& t,
                Engine::Execution::BaseScenarioElement& bs) override
        {
          m_paused = false;

          m_widg= new TimeWidget;
          if(context.doc.app.mainWindow)
          {
            context.doc.app.mainWindow->addToolBar(Qt::ToolBarArea::BottomToolBarArea, m_widg);
          }
          QObject::connect(m_widg, &TimeWidget::advance,
                  this, [=] (int val) {
            m_cur->baseInterval().OSSIAInterval()->tick(ossia::time_value{val});
          });
          m_widg->show();

          m_cur = &bs;
          m_default.play(t);

          resume_impl(bs);
        }
        void pause_impl(Engine::Execution::BaseScenarioElement&) override
        {

        }
        void resume_impl(Engine::Execution::BaseScenarioElement&) override
        {

        }
        void stop_impl(Engine::Execution::BaseScenarioElement&) override
        {
          delete m_widg;
          m_plug.finished();
        }
        bool paused() const override
        {
          return m_paused;
        }

        Engine::Execution::DefaultClockManager m_default;
        Engine::Execution::DocumentPlugin& m_plug;
        Engine::Execution::BaseScenarioElement* m_cur{};
        bool m_paused{};
};

class ClockFactory final : public Engine::Execution::ClockManagerFactory
{
        SCORE_CONCRETE("5e8d0f1b-752f-4e29-8c8c-ecd65bd69806")

        QString prettyName() const override
        {
          return QObject::tr("Manual");
        }

        std::unique_ptr<Engine::Execution::ClockManager> make(
            const Engine::Execution::Context& ctx) override
        {
          return std::make_unique<Clock>(ctx);
        }


        Engine::Execution::time_function
        makeTimeFunction(const score::DocumentContext& ctx) const override
        {
          constexpr double rate = 44100;
          return [=] (const TimeVal& v) -> ossia::time_value {
            return v.isInfinite()
                ? ossia::Infinite
                : ossia::time_value(std::llround(rate * v.msec() / 1000.));
          };
        }

        Engine::Execution::reverse_time_function
        makeReverseTimeFunction(const score::DocumentContext& ctx) const override
        {
          constexpr double rate = 44100;
          return [=] (const ossia::time_value& v) -> TimeVal {
            return v.infinite()
                ? TimeVal{PositiveInfinity{}}
                : TimeVal::fromMsecs(1000. * v.impl / rate);
          };
        }
};

}
}
