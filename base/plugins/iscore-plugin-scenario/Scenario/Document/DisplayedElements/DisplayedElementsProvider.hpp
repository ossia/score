#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>


namespace Scenario
{
class ConstraintModel;

class DisplayedElementsProvider : public iscore::FactoryInterfaceBase
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                DisplayedElementsContainer,
                "4bfcf0ee-6c47-405a-a15d-9da73436e273")
    public:
        virtual ~DisplayedElementsProvider();
        virtual bool matches(const ConstraintModel& cst) const = 0;
        virtual DisplayedElementsContainer make(const ConstraintModel& cst) const = 0;
};
}
