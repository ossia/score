#pragma once
#include <Process/TimeValue.hpp>

#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <ossia/editor/scenario/time_value.hpp>

#include <score_plugin_engine_export.h>
#include <smallfun.hpp>
namespace score
{
struct DocumentContext;
class Document;
}
namespace Execution
{
struct Context;
class BaseScenarioElement;

/**
 * @brief Sets-up and manages the main execution clock
 *
 * This class allows to create control mechanisms
 * for the score execution clock.
 *
 * The default implementation, DefaultClock,
 * uses sleep() and threading to do the execution.
 *
 * Other implementations could be used to synchronize
 * to external clocks, such as the sound card clock,
 * or the host clock if score is used as a plug-in.
 *
 * The derived constructors should set-up
 * the clock drive mode, speed, etc.
 *
 */

using time_function = smallfun::function<ossia::time_value(const TimeVal&)>;
using reverse_time_function = smallfun::function<TimeVal(const ossia::time_value&)>;
class SCORE_PLUGIN_ENGINE_EXPORT Clock
{
public:
  Clock(const Execution::Context& ctx);
  virtual ~Clock();

  const Context& context;
  std::shared_ptr<BaseScenarioElement> scenario;

  void play(const TimeVal& t);
  void pause();
  void resume();
  void stop();
  virtual bool paused() const;

  //! Moves the execution under step control, or back to its own clock.
  virtual bool setStepping(bool stepping);
  virtual bool stepping() const noexcept;

  //! Runs the execution synchronously until @p seconds have been executed
  //! since the clock started stepping.
  virtual bool stepTo(double seconds);

  //! The clock of the execution currently playing @p doc, if any.
  static Clock* running(const score::Document& doc) noexcept;

  //! Incremented each time a clock starts playing.
  static int64_t playCount() noexcept;

  //! The next clock that plays @p doc starts stepping, until it stops.
  static void requestStepping(const score::Document& doc, bool stepping);
  static bool steppingRequested(const score::Document& doc) noexcept;

protected:
  virtual void play_impl(const TimeVal& t) = 0;
  virtual void pause_impl() = 0;
  virtual void resume_impl() = 0;
  virtual void stop_impl() = 0;
};

class SCORE_PLUGIN_ENGINE_EXPORT ClockFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(ClockFactory, "fb2b3624-ee6f-4e9a-901a-a096bb5fec0a")
public:
  virtual ~ClockFactory();

  virtual QString prettyName() const = 0;
  virtual std::unique_ptr<Clock> make(const Execution::Context& ctx) = 0;

  virtual time_function makeTimeFunction(const score::DocumentContext& ctx) const = 0;
  virtual reverse_time_function
  makeReverseTimeFunction(const score::DocumentContext& ctx) const = 0;
};

class SCORE_PLUGIN_ENGINE_EXPORT ClockFactoryList final
    : public score::InterfaceList<ClockFactory>
{
public:
  using object_type = Clock;
};
}

Q_DECLARE_METATYPE(Execution::ClockFactory::ConcreteKey)
W_REGISTER_ARGTYPE(Execution::ClockFactory::ConcreteKey)
