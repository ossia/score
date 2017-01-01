#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore_plugin_engine_export.h>

#include <Process/TimeValue.hpp>

namespace Engine
{
namespace Execution
{
struct Context;
class BaseScenarioElement;

/**
 * @brief Sets-up and manages the main execution clock
 *
 * This class allows to create control mechanisms
 * for the i-score execution clock.
 *
 * The default implementation, DefaultClockManager,
 * uses sleep() and threading to do the execution.
 *
 * Other implementations could be used to synchronize
 * to external clocks, such as the sound card clock,
 * or the host clock if i-score is used as a plug-in.
 *
 * The derived constructors should set-up
 * the clock drive mode, speed, etc.
 *
 */
class ISCORE_PLUGIN_ENGINE_EXPORT ClockManager
{
public:
  ClockManager(const Engine::Execution::Context& ctx) : context{ctx}
  {
  }
  virtual ~ClockManager();

  const Context& context;

  void play(const TimeValue& t);
  void pause();
  void resume();
  void stop();

protected:
  virtual void play_impl(const TimeValue& t, BaseScenarioElement&) = 0;
  virtual void pause_impl(BaseScenarioElement&) = 0;
  virtual void resume_impl(BaseScenarioElement&) = 0;
  virtual void stop_impl(BaseScenarioElement&) = 0;
};

class ISCORE_PLUGIN_ENGINE_EXPORT ClockManagerFactory
    : public iscore::Interface<ClockManagerFactory>
{
  ISCORE_INTERFACE("fb2b3624-ee6f-4e9a-901a-a096bb5fec0a")
public:
  virtual ~ClockManagerFactory();

  virtual QString prettyName() const = 0;
  virtual std::unique_ptr<ClockManager>
  make(const Engine::Execution::Context& ctx) = 0;
};

class ISCORE_PLUGIN_ENGINE_EXPORT ClockManagerFactoryList final
    : public iscore::InterfaceList<ClockManagerFactory>
{
public:
  using object_type = ClockManager;
};
}
}

Q_DECLARE_METATYPE(Engine::Execution::ClockManagerFactory::ConcreteKey)
