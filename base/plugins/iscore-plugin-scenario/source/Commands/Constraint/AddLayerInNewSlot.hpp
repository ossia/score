#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <QString>

#include <tests/helpers/ForwardDeclaration.hpp>
class Process;
class RackModel;
class SlotModel;
class LayerModel;
class ConstraintModel;

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
                AddLayerInNewSlot(
                    Path<ConstraintModel>&& constraintPath,
                    Id<Process> process);

                virtual void undo() override;
                virtual void redo() override;

                Id<Process> processId() const
                {
                    return m_processId;
                }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

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
