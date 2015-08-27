#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <QString>
class SlotModel;
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
                ISCORE_COMMAND_DECL("ResizeSlotVertically", "ResizeSlotVertically")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(ResizeSlotVertically, "ScenarioControl")
                ResizeSlotVertically(
                    ModelPath<SlotModel>&& slotPath,
                    double newSize);

                virtual void undo() override;
                virtual void redo() override;

                void update(const ModelPath<SlotModel>&, double size)
                {
                    m_newSize = size;
                }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<SlotModel> m_path;

                double m_originalSize {};
                double m_newSize {};
        };
    }
}
