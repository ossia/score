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

class MoveEventList final : public iscore::FactoryListInterface
{
    public:
        static const iscore::FactoryBaseKey& staticFactoryKey() {
            return MoveEventFactoryInterface::staticFactoryKey();
        }

        iscore::FactoryBaseKey name() const final override {
            return MoveEventFactoryInterface::staticFactoryKey();
        }

        /**
     * @brief registerMoveEventFactory
     * register a moveEvent with a unique priority (higher the better),
     * WARNING, if the same priority is already there, it will be overriden
     * @param factoryInterface
     */
        void insert(std::unique_ptr<iscore::FactoryInterfaceBase> e) final override
        {
            if(auto pf = dynamic_unique_ptr_cast<MoveEventFactoryInterface>(std::move(e)))
                m_list.push_back(std::move(pf));
        }

        const auto& list() const
        { return m_list; }

        /**
     * @brief getMoveEventFactory
     * @return
     * the factory with the highest priority for the specified strategy
     */
        MoveEventFactoryInterface* get(MoveEventFactoryInterface::Strategy strategy) const;

    private:
        std::vector<std::unique_ptr<MoveEventFactoryInterface>> m_list;
};

}
}
