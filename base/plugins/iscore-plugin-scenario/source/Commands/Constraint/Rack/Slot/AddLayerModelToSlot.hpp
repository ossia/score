#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

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
        class AddLayerModelToSlot : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("AddLayerModelToSlot", "AddLayerModelToSlot")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddLayerModelToSlot, "ScenarioControl")
                AddLayerModelToSlot(
                    ModelPath<SlotModel>&& slot,
                    ModelPath<Process>&& process);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<SlotModel> m_slotPath;
                ModelPath<Process> m_processPath;

                QByteArray m_processData;

                id_type<LayerModel> m_createdLayerId {};
        };
    }
}
