#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

class LayerModel;
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

                RemoveLayerModelFromSlot(ObjectPath&& slotPath, id_type<LayerModel> layerId);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<LayerModel> m_layerId {};

                QByteArray m_serializedLayerData;
        };
    }
}
