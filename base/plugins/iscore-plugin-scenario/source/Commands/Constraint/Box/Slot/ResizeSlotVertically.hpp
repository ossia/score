#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <QString>

namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The ResizeSlotVerticallyCommand class
         *
         * Changes a slot's vertical size
         */
        class ResizeSlotVertically : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(ResizeSlotVertically, "ScenarioControl")
                ResizeSlotVertically(ObjectPath&& slotPath,
                                     double newSize);

                virtual void undo() override;
                virtual void redo() override;

                void update(const ObjectPath&, double size)
                {
                    m_newSize = size;
                }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;

                double m_originalSize {};
                double m_newSize {};
        };
    }
}
