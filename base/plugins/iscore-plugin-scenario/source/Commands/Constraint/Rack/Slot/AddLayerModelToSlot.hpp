#pragma once
#include <Commands/ScenarioCommandFactory.hpp>
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
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), "AddLayerModelToSlot", "AddLayerModelToSlot")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddLayerModelToSlot)
                AddLayerModelToSlot(
                    Path<SlotModel>&& slot,
                    Path<Process>&& process);

                void undo() const override;
                void redo() const override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<SlotModel> m_slotPath;
                Path<Process> m_processPath;

                QByteArray m_processData;

                Id<LayerModel> m_createdLayerId {};
        };
    }
}
