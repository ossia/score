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
  BaseScenarioElement& scenario;

  void play(const TimeVal& t);
  void pause();
  void resume();
  void stop();
  virtual bool paused() const;

protected:
  virtual void play_impl(const TimeVal& t, BaseScenarioElement&) = 0;
  virtual void pause_impl(BaseScenarioElement&) = 0;
  virtual void resume_impl(BaseScenarioElement&) = 0;
  virtual void stop_impl(BaseScenarioElement&) = 0;
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

class SCORE_PLUGIN_ENGINE_EXPORT ClockFactoryList final : public score::InterfaceList<ClockFactory>
{
public:
  using object_type = Clock;
};
}

Q_DECLARE_METATYPE(Execution::ClockFactory::ConcreteKey)
W_REGISTER_ARGTYPE(Execution::ClockFactory::ConcreteKey)
