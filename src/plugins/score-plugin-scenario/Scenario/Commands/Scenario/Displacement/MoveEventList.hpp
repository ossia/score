#pragma once
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>

namespace Scenario
{
namespace Command
{

class MoveEventList final
    : public score::InterfaceList<MoveEventFactoryInterface>
{
public:
  /**
   *@brief getMoveEventFactory
   *@return
   *the factory with the highest priority for the specified strategy
   */
  MoveEventFactoryInterface&
  get(const score::ApplicationContext& ctx,
      MoveEventFactoryInterface::Strategy s) const;
};
}
}
