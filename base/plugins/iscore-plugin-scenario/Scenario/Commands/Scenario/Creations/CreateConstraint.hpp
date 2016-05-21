#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <QString>

#include <iscore_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class StateModel;
class ConstraintModel;
class ConstraintViewModel;
class ScenarioModel;

namespace Command
{
/**
        * @brief The CreateEventAfterEventCommand class
        *
        * This Command creates a constraint and another event in a scenario,
        * starting from an event selected by the user.
        */
class ISCORE_PLUGIN_SCENARIO_EXPORT CreateConstraint final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateConstraint,"Create a constraint")
        public:
            CreateConstraint(
                Path<Scenario::ScenarioModel>&& scenarioPath,
                Id<StateModel> startState,
                Id<StateModel> endState);
        CreateConstraint& operator= (CreateConstraint &&) = default;

        const Path<Scenario::ScenarioModel>& scenarioPath() const
        { return m_path; }

        void undo() const override;
        void redo() const override;

        const Id<ConstraintModel>& createdConstraint() const
        { return m_createdConstraintId; }

        const Id<StateModel>& startState() const
        { return m_startStateId; }

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
