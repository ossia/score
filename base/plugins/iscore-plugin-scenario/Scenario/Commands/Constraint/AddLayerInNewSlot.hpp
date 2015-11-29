#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <qbytearray.h>

#include "iscore/tools/SettableIdentifier.hpp"

class ConstraintModel;
class DataStreamInput;
class DataStreamOutput;
class LayerModel;
class Process;
class RackModel;
class SlotModel;

namespace Scenario
{
    namespace Command
    {
        /**
        * @brief The AddLayerInNewSlot class
        */
        class AddLayerInNewSlot final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddLayerInNewSlot, "Add a new layer")
            public:
                AddLayerInNewSlot(
                    Path<ConstraintModel>&& constraintPath,
                    const Id<Process>& process);

                void undo() const override;
                void redo() const override;

                Id<Process> processId() const
                {
                    return m_processId;
                }

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<ConstraintModel> m_path;

                bool m_existingRack {};

                Id<Process> m_processId {};
                Id<RackModel> m_createdRackId {};
                Id<SlotModel> m_createdSlotId {};
                Id<LayerModel> m_createdLayerId {};
                Id<Process> m_sharedProcessModelId {};

                QByteArray m_processData;
        };
    }
}
