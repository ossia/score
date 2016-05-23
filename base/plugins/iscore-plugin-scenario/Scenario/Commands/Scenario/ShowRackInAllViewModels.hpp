#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <QMap>

#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class RackModel;
class ConstraintModel;
class ConstraintViewModel;

namespace Command
{

class ShowRackInAllViewModels final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), ShowRackInAllViewModels, "Show a rack everywhere")
    public:
        ShowRackInAllViewModels(
                Path<ConstraintModel>&& constraint_path,
                Id<RackModel> rackId);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<ConstraintModel> m_constraintPath;
        Id<RackModel> m_rackId {};

        QMap<Id<ConstraintViewModel>, Id<RackModel>> m_previousRacks;

};

}

}
