#pragma once

#include <Execution/Clock/ClockFactory.hpp>
#include <Execution/Clock/DataflowClock.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <ossia/audio/audio_engine.hpp>
#include <ossia/editor/scenario/time_value.hpp>

#include <QMainWindow>
#include <QPointer>
#include <QToolBar>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

#include <verdigris>

namespace Execution
{
namespace ManualClock
{

//! Runs the execution tick on the audio thread, for exactly the samples the
//! UI thread requested.
struct StepGate
{
  ossia::audio_engine::fun_type tick;
  int64_t bufferSize{512};
  double sampleRate{44100.};

  std::vector<std::vector<float>> inputs;
  std::vector<std::vector<float>> outputs;
  std::vector<float*> inputPointers;
  std::vector<float*> outputPointers;

  std::atomic<int64_t> requested{};
  std::atomic<int64_t> done{};

  StepGate(
      ossia::audio_engine::fun_type t, int64_t bs, double rate, int ins, int outs)
      : tick{std::move(t)}
      , bufferSize{std::max<int64_t>(bs, 1)}
      , sampleRate{rate > 0. ? rate : 44100.}
      , inputs(std::max(ins, 0), std::vector<float>(bufferSize))
      , outputs(std::max(outs, 0), std::vector<float>(bufferSize))
  {
    for(auto& c : inputs)
      inputPointers.push_back(c.data());
    for(auto& c : outputs)
      outputPointers.push_back(c.data());
  }

  int64_t samplesAt(double seconds) const noexcept
  {
    return int64_t(std::floor(seconds * sampleRate + 1e-6));
  }

  void run(const ossia::audio_tick_state& t)
  {
    for(int chan = 0; chan < t.n_out; chan++)
      std::fill_n(t.outputs[chan], t.frames, 0.f);

    using clk = std::chrono::steady_clock;
    constexpr auto linger = std::chrono::milliseconds(5);
    auto deadline = clk::now() + linger;
    for(;;)
    {
      const int64_t target = requested.load(std::memory_order_acquire);
      int64_t cur = done.load(std::memory_order_relaxed);
      if(cur < target)
      {
        while(cur < target)
        {
          const int64_t n = std::min(target - cur, bufferSize);
          ossia::audio_tick_state st{
              inputPointers.data(),
              outputPointers.data(),
              int32_t(inputPointers.size()),
              int32_t(outputPointers.size()),
              uint64_t(n),
              double(cur) / sampleRate};
          tick(st);
          cur += n;
        }
        done.store(cur, std::memory_order_release);
        deadline = clk::now() + linger;
        continue;
      }
      if(clk::now() >= deadline)
        break;
      std::this_thread::yield();
    }
  }

  bool waitFor(int64_t target, const ossia::audio_engine& engine)
  {
    int64_t prev = requested.load(std::memory_order_relaxed);
    if(target > prev)
      requested.store(target, std::memory_order_release);

    using clk = std::chrono::steady_clock;
    const auto t0 = clk::now();
    int spins = 0;
    while(done.load(std::memory_order_acquire) < target)
    {
      if(!engine.running())
        return false;
      if(clk::now() - t0 > std::chrono::seconds(60))
        return false;
      if(spins++ < 1000)
        std::this_thread::yield();
      else
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    return true;
  }
};

class TimeWidget : public QToolBar
{
  W_OBJECT(TimeWidget)
public:
  TimeWidget(QWidget* parent = nullptr)
      : QToolBar{parent}
  {
    for(int ms : {1, 10, 100, 1000})
    {
      auto act = addAction(QStringLiteral("+%1 ms").arg(ms));
      connect(act, &QAction::triggered, this, [this, ms] { advance(ms); });
    }
  }

  void advance(int milliseconds) W_SIGNAL(advance, milliseconds);
};

class Clock final : public Dataflow::Clock
{
public:
  Clock(const Execution::Context& ctx)
      : Dataflow::Clock{ctx, true}
  {
  }

  ~Clock() override { delete m_widg.data(); }

private:
  void play_impl(const TimeVal& t) override
  {
    Dataflow::Clock::play_impl(t);

    if(auto win = context.doc.app.mainWindow)
    {
      m_widg = new TimeWidget;
      win->addToolBar(Qt::ToolBarArea::BottomToolBarArea, m_widg);
      QObject::connect(m_widg, &TimeWidget::advance, m_widg, [this](int ms) {
        stepTo(steppedSeconds() + ms / 1000.);
      });
      m_widg->show();
    }
  }

  void stop_impl() override
  {
    delete m_widg.data();
    Dataflow::Clock::stop_impl();
  }

  QPointer<TimeWidget> m_widg;
};

class ClockFactory final : public Execution::ClockFactory
{
  SCORE_CONCRETE("5e8d0f1b-752f-4e29-8c8c-ecd65bd69806")

  QString prettyName() const override { return QObject::tr("Manual"); }

  std::unique_ptr<Execution::Clock> make(const Execution::Context& ctx) override
  {
    return std::make_unique<Clock>(ctx);
  }

  Execution::time_function
  makeTimeFunction(const score::DocumentContext& ctx) const override
  {
    return Dataflow::ClockFactory{}.makeTimeFunction(ctx);
  }

  Execution::reverse_time_function
  makeReverseTimeFunction(const score::DocumentContext& ctx) const override
  {
    return Dataflow::ClockFactory{}.makeReverseTimeFunction(ctx);
  }
};
}
}
