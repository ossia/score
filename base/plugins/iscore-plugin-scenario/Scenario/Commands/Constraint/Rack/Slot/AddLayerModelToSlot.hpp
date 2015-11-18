#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <Process/ProcessFactoryKey.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
class LayerModel;
class Process;
class SlotModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The AddLayerToSlot class
         *
         * Adds a process view to a slot.
         */
        class AddLayerModelToSlot final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddLayerModelToSlot, "AddLayerModelToSlot")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                // Use this constructor when the process already exists
                AddLayerModelToSlot(
                    Path<SlotModel>&& slot,
                    Path<Process>&& process);

                // Use this constructor when the process isn't created yet
                AddLayerModelToSlot(
                    Path<SlotModel>&& slot,
                    Path<Process>&& process,
                    const QByteArray& processConstructionData);

                AddLayerModelToSlot(
                        Path<SlotModel>&& slot,
                        const Id<LayerModel>& layerid,
                        Path<Process>&& process,
                        const QByteArray& processConstructionData);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(QDataStream&) const override;
                void deserializeImpl(QDataStream&) override;

            private:
                Path<SlotModel> m_slotPath;
                Path<Process> m_processPath;

                QByteArray m_processData;

                Id<LayerModel> m_createdLayerId {};
        };
    }
}
