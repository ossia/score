#pragma once

#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>

#include <score/model/Identifier.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/StringFactoryKey.hpp>

#include <score_plugin_scenario_export.h>

#include <memory>

namespace score
{
struct ApplicationContext;
}
template <typename Object>
class Path;
namespace Scenario
{
class ProcessModel;
class EventModel;
class ProcessModel;
namespace Command
{
class SerializableMoveEvent;

class SCORE_PLUGIN_SCENARIO_EXPORT MoveEventFactoryInterface : public score::InterfaceBase
{
  SCORE_INTERFACE(MoveEventFactoryInterface, "69dc1f79-5cb9-4a36-b382-8c099f7abf57")

public:
  enum Strategy
  {
    CREATION,
    MOVE
  };
  virtual std::unique_ptr<SerializableMoveEvent> make(
      const Scenario::ProcessModel&,
      Id<EventModel> eventId,
      TimeVal newDate,
      ExpandMode mode,
      LockMode lm)
      = 0;

  virtual std::unique_ptr<SerializableMoveEvent> make(LockMode) = 0;
  virtual ~MoveEventFactoryInterface();

  /**
   * @brief priority
   * the highest priority will be default move behavior for the indicated
   * strategy
   * Basically, we want to know how well the policy is suited for the desired
   * strategy.
   * @param strategy
   * the strategy for which we need a displacement policy;
   * @return
   */
  virtual int priority(const score::ApplicationContext& ctx, Strategy strategy) const = 0;
};
}
}
