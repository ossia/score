#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class ConstraintViewModel;
class RackModel;
namespace Command
{
/**
         * @brief The ShowRackInViewModel class
         *
         * For a given constraint view model,
         * select the rack that is to be shown, and show it.
         */
class ISCORE_PLUGIN_SCENARIO_EXPORT ShowRackInViewModel final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), ShowRackInViewModel, "Show a rack")
        public:
            ShowRackInViewModel(
                Path<ConstraintViewModel>&& constraint_path,
                Id<RackModel> rackId);
        ShowRackInViewModel(
                const ConstraintViewModel& vm,
                Id<RackModel> rackId);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<ConstraintViewModel> m_constraintViewPath;
        Id<RackModel> m_rackId {};
        Id<RackModel> m_previousRackId {};

};
}
}
