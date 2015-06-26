#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
class LayerModel;
class ProcessModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The AddLayerToSlot class
         *
         * Adds a process view to a slot.
         */
        class AddLayerModelToSlot : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddLayerModelToSlot, "ScenarioControl")
                AddLayerModelToSlot(ObjectPath&& slot, ObjectPath&& process);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_slotPath;
                ObjectPath m_processPath;

                QByteArray m_processData;

                id_type<LayerModel> m_createdLayerId {};
        };
    }
}
