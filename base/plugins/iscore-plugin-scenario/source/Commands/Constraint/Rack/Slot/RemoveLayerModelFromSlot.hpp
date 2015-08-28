#pragma once
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
        class RemoveLayerModelFromSlot : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL_OBSOLETE("RemoveLayerModelFromSlot", "RemoveLayerModelFromSlot")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR_OBSOLETE(RemoveLayerModelFromSlot, "ScenarioControl")

                RemoveLayerModelFromSlot(
                    Path<SlotModel>&& slotPath,
                    const Id<LayerModel>& layerId);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<SlotModel> m_path;
                Id<LayerModel> m_layerId {};

                QByteArray m_serializedLayerData;
        };
    }
}
