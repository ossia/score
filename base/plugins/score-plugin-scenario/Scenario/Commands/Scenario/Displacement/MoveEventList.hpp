#pragma once
#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>

#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/customfactory/FactoryInterface.hpp>

#include <QVector>

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
