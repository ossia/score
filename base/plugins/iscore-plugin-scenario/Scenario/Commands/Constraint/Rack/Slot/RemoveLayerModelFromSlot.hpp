#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class LayerModel;
class SlotModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The RemoveLayerFromSlot class
         *
         * Removes a process view from a slot.
         */
        class RemoveLayerModelFromSlot final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), RemoveLayerModelFromSlot, "RemoveLayerModelFromSlot")
            public:

                RemoveLayerModelFromSlot(
                    Path<SlotModel>&& slotPath,
                    const Id<LayerModel>& layerId);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(QDataStream&) const override;
                void deserializeImpl(QDataStream&) override;

            private:
                Path<SlotModel> m_path;
                Id<LayerModel> m_layerId {};

                QByteArray m_serializedLayerData;
        };
    }
}
