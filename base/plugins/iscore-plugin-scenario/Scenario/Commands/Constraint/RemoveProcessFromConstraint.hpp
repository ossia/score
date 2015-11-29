#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <qbytearray.h>
#include <qpair.h>
#include <qvector.h>

#include "iscore/tools/SettableIdentifier.hpp"

class ConstraintModel;
class DataStreamInput;
class DataStreamOutput;
class LayerModel;
class Process;

namespace Scenario
{
    namespace Command
    {
        class RemoveProcessFromConstraint final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), RemoveProcessFromConstraint, "Remove a process")
            public:
                RemoveProcessFromConstraint(
                    Path<ConstraintModel>&& constraintPath,
                    const Id<Process>& processId);
                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<ConstraintModel> m_path;
                Id<Process> m_processId;

                QByteArray m_serializedProcessData;
                QVector<QPair<Path<LayerModel>, QByteArray>> m_serializedViewModels;
        };
    }
}
