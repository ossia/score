/*
#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
class LayerModel;
class ProcessModel;
namespace Scenario
{
    namespace Command
    {
        ///
        // @brief The CopyLayerModel class
        //
        // Copy a process view from a Slot to another.
        // Note : this must be in the same constraint.
        // Note : there cannot be two Process View Models of the same Process in the same Slot.
        // It is up to the user of this Command to prevent this.
        //
        // For instance, a message could be displayed saying that the LM cannot be copied
        // as long as there is another lm for the same process in the other slot (same for the merging of slots).
        ///
        class CopyLayerModel : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(CopyLayerModel, "ScenarioControl")
                CopyLayerModel(ObjectPath&& lmToCopy,
                                     ObjectPath&& targetSlot);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_lmPath;
                ObjectPath m_targetSlotPath;

                id_type<LayerModel> m_newLayerModelId;
        };
    }
}
*/
