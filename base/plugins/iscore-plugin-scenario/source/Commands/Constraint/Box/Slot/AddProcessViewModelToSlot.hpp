#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
class ProcessViewModel;
class ProcessModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The AddProcessViewToSlot class
         *
         * Adds a process view to a slot.
         */
        class AddProcessViewModelToSlot : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddProcessViewModelToSlot, "ScenarioControl")
                AddProcessViewModelToSlot(ObjectPath&& slot, ObjectPath&& process);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_slotPath;
                ObjectPath m_processPath;

                QByteArray m_processData;

                id_type<ProcessViewModel> m_createdProcessViewId {};
        };
    }
}
