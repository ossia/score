#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <QString>

#include <tests/helpers/ForwardDeclaration.hpp>
class Process;
class RackModel;
class SlotModel;
class LayerModel;

namespace Scenario
{
    namespace Command
    {
        /**
        * @brief The AddLayerInNewSlot class
        */
        class AddLayerInNewSlot : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("AddLayerInNewSlot", "AddLayerInNewSlot")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddLayerInNewSlot, "ScenarioControl")
                AddLayerInNewSlot(ObjectPath&& constraintPath,
                                        id_type<Process> process);

                virtual void undo() override;
                virtual void redo() override;

                id_type<Process> processId() const
                {
                    return m_processId;
                }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;

                bool m_existingRack {};

                id_type<Process> m_processId {};
                id_type<RackModel> m_createdRackId {};
                id_type<SlotModel> m_createdSlotId {};
                id_type<LayerModel> m_createdLayerId {};
                id_type<Process> m_sharedProcessModelId {};

                QByteArray m_processData;
        };
    }
}
