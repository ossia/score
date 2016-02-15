#pragma once
#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <QVector>

#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

namespace Scenario
{
namespace Command
{

class MoveEventList final :
        public iscore::ConcreteFactoryList<MoveEventFactoryInterface>
{
    public:
        /**
     * @brief getMoveEventFactory
     * @return
     * the factory with the highest priority for the specified strategy
     */
        MoveEventFactoryInterface& get(const iscore::ApplicationContext& ctx,
                                       MoveEventFactoryInterface::Strategy s) const;
};

}
}
