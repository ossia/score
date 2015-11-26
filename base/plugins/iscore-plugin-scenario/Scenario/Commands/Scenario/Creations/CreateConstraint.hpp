#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp>

class EventModel;
class ConstraintViewModel;
class ConstraintModel;
class LayerModel;
class StateModel;
namespace Scenario { class ScenarioModel; }

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
        class CreateConstraint final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateConstraint,"Create a constraint")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                CreateConstraint(
                    Path<Scenario::ScenarioModel>&& scenarioPath,
                    const Id<StateModel>& startState,
                    const Id<StateModel>& endState);
                CreateConstraint& operator= (CreateConstraint &&) = default;

                const Path<Scenario::ScenarioModel>& scenarioPath() const
                { return m_path; }

                void undo() const override;
                void redo() const override;

                const Id<ConstraintModel>& createdConstraint() const
                { return m_createdConstraintId; }

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<Scenario::ScenarioModel> m_path;
                QString m_createdName;

                Id<ConstraintModel> m_createdConstraintId {};

                Id<StateModel> m_startStateId {};
                Id<StateModel> m_endStateId {};

                ConstraintViewModelIdMap m_createdConstraintViewModelIDs;
                Id<ConstraintViewModel> m_createdConstraintFullViewId {};
        };
    }
}
