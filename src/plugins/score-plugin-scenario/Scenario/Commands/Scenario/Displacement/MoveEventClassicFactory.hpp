#pragma once

#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>

#include <score/model/Identifier.hpp>

template <typename Object>
class Path;

namespace Scenario
{
class ProcessModel;
class EventModel;

namespace Command
{
class SerializableMoveEvent;

class MoveEventClassicFactory final : public MoveEventFactoryInterface
{
  SCORE_CONCRETE("644a6f8d-de63-4951-b28b-33b5e2c71ac8")

  std::unique_ptr<SerializableMoveEvent> make(
      const Scenario::ProcessModel&,
      Id<EventModel> eventId,
      TimeVal newDate,
      ExpandMode mode,
      LockMode lck) override;

  std::unique_ptr<SerializableMoveEvent> make(LockMode) override;

  int priority(const score::ApplicationContext& ctx, MoveEventFactoryInterface::Strategy s)
      const override
  {
    if (s == MoveEventFactoryInterface::CREATION)
      return 1;

    return 0; // default choice
  }
};
}
}
