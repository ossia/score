#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include "Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp"
#include <tuple>

class EventModel;
class AbstractConstraintViewModel;
class ConstraintModel;
class LayerModel;
class DisplayedStateModel;

#include <tests/helpers/ForwardDeclaration.hpp>
namespace Scenario
{
    namespace Command
    {
        /**
        * @brief The CreateEventAfterEventCommand class
        *
        * This Command creates a constraint and another event in a scenario,
        * starting from an event selected by the user.
        */
        class CreateConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(CreateConstraint, "ScenarioControl")
                CreateConstraint(ObjectPath&& scenarioPath, id_type<EventModel> startEvent, id_type<EventModel> endEvent);
                CreateConstraint& operator= (CreateConstraint &&) = default;

                virtual void undo() override;
                virtual void redo() override;

                id_type<ConstraintModel> createdConstraint() const
                {
                    return m_createdConstraintId;
                }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;

                id_type<ConstraintModel> m_createdConstraintId {};
                id_type<EventModel> m_startEventId {};
                id_type<EventModel> m_endEventId {};

                id_type<DisplayedStateModel> m_endStateId {};

                ConstraintViewModelIdMap m_createdConstraintViewModelIDs;
                id_type<AbstractConstraintViewModel> m_createdConstraintFullViewId {};
        };
    }
}
