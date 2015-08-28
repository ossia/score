#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include "Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp"

class EventModel;
class ConstraintViewModel;
class ConstraintModel;
class LayerModel;
class StateModel;
class ScenarioModel;

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
                ISCORE_COMMAND_DECL_OBSOLETE("CreateConstraint","CreateConstraint")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR_OBSOLETE(CreateConstraint, "ScenarioControl")
                CreateConstraint(
                    Path<ScenarioModel>&& scenarioPath,
                    const Id<StateModel>& startState,
                    const Id<StateModel>& endState);
                CreateConstraint& operator= (CreateConstraint &&) = default;

                const Path<ScenarioModel>& scenarioPath() const
                { return m_path; }

                virtual void undo() override;
                virtual void redo() override;

                const Id<ConstraintModel>& createdConstraint() const
                { return m_createdConstraintId; }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<ScenarioModel> m_path;
                QString m_createdName;

                Id<ConstraintModel> m_createdConstraintId {};

                Id<StateModel> m_startStateId {};
                Id<StateModel> m_endStateId {};

                ConstraintViewModelIdMap m_createdConstraintViewModelIDs;
                Id<ConstraintViewModel> m_createdConstraintFullViewId {};
        };
    }
}
