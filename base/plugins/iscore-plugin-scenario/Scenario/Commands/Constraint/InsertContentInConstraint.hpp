#pragma once
#include <Process/ExpandMode.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <QJsonObject>
#include <QMap>

#include <iscore/tools/SettableIdentifier.hpp>

class ConstraintModel;
class DataStreamInput;
class DataStreamOutput;
namespace Process { class ProcessModel; }
class RackModel;

namespace Scenario
{
    namespace Command
    {
        class InsertContentInConstraint final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), InsertContentInConstraint, "Insert content in a constraint")
            public:
                InsertContentInConstraint(
                    QJsonObject&& sourceConstraint,
                    Path<ConstraintModel>&&  targetConstraint,
                    ExpandMode mode);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                QJsonObject m_source;
                Path<ConstraintModel> m_target;
                ExpandMode m_mode{ExpandMode::GrowShrink};

                QMap<Id<RackModel>, Id<RackModel>> m_rackIds;
                QMap<Id<Process::ProcessModel>, Id<Process::ProcessModel>> m_processIds;
        };
    }
}
