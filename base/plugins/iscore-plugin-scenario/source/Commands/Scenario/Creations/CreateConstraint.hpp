#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include "Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp"

class EventModel;
class ConstraintViewModel;
class ConstraintModel;
class LayerModel;
class StateModel;

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
                ISCORE_COMMAND_DECL("CreateConstraint","CreateConstraint")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(CreateConstraint, "ScenarioControl")
                CreateConstraint(
                    ObjectPath&& scenarioPath,
                    const id_type<StateModel>& startState,
                    const id_type<StateModel>& endState);
                CreateConstraint& operator= (CreateConstraint &&) = default;

                const ObjectPath& scenarioPath() const
                { return m_path; }

                virtual void undo() override;
                virtual void redo() override;

                const id_type<ConstraintModel>& createdConstraint() const
                { return m_createdConstraintId; }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                QString m_createdName;

                id_type<ConstraintModel> m_createdConstraintId {};

                id_type<StateModel> m_startStateId {};
                id_type<StateModel> m_endStateId {};

                ConstraintViewModelIdMap m_createdConstraintViewModelIDs;
                id_type<ConstraintViewModel> m_createdConstraintFullViewId {};
        };
    }
}
