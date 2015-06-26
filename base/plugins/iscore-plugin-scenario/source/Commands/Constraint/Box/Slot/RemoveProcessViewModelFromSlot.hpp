#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

class ProcessViewModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The RemoveProcessViewFromSlot class
         *
         * Removes a process view from a slot.
         */
        class RemoveProcessViewModelFromSlot : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(RemoveProcessViewModelFromSlot, "ScenarioControl")

                RemoveProcessViewModelFromSlot(ObjectPath&& slotPath, id_type<ProcessViewModel> processViewId);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<ProcessViewModel> m_processViewId {};

                QByteArray m_serializedProcessViewData;
        };
    }
}
