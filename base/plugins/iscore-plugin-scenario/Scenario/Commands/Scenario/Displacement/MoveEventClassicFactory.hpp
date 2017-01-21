#pragma once

#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>

#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/model/Identifier.hpp>

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
  ISCORE_CONCRETE("644a6f8d-de63-4951-b28b-33b5e2c71ac8")

  std::unique_ptr<SerializableMoveEvent> make(
      Path<Scenario::ProcessModel>&& scenarioPath,
      Id<EventModel>
          eventId,
      TimeVal newDate,
      ExpandMode mode) override;

  std::unique_ptr<SerializableMoveEvent> make() override;

  int priority(
      const iscore::ApplicationContext& ctx,
      MoveEventFactoryInterface::Strategy s) const override
  {
    if (s == MoveEventFactoryInterface::CREATION)
      return 1;

    return 0; // default choice
  }
};
}
}
