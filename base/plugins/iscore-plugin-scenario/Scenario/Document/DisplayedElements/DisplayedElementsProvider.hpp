#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

class DisplayedElementsProvider : public iscore::FactoryInterfaceBase
{
        ISCORE_FACTORY_DECL("DisplayedElementsProvider")
    public:
        virtual bool matches(const ConstraintModel& cst) const = 0;
        virtual DisplayedElementsContainer make(const ConstraintModel& cst) const = 0;
};
