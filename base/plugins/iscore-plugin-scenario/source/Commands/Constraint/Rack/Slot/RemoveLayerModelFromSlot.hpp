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
                ISCORE_COMMAND_DECL("RemoveLayerModelFromSlot", "RemoveLayerModelFromSlot")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(RemoveLayerModelFromSlot, "ScenarioControl")

                RemoveLayerModelFromSlot(
                    ModelPath<SlotModel>&& slotPath,
                    const id_type<LayerModel>& layerId);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<SlotModel> m_path;
                id_type<LayerModel> m_layerId {};

                QByteArray m_serializedLayerData;
        };
    }
}
