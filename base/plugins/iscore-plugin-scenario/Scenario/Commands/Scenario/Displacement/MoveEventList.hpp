#pragma once
#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <qvector.h>

#include "iscore/plugins/customfactory/FactoryInterface.hpp"


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
        void insert(iscore::FactoryInterfaceBase* e) final override
        {
            if(auto pf = dynamic_cast<MoveEventFactoryInterface*>(e))
                m_list.push_back(pf);
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
        QVector<MoveEventFactoryInterface*> m_list;
};
