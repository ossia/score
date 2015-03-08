#pragma once
#include <public_interface/command/SerializableCommand.hpp>
#include <public_interface/tools/ObjectPath.hpp>

#include <QString>

namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The ResizeDeckVerticallyCommand class
         *
         * Changes a deck's vertical size
         */
        class ResizeDeckVertically : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(ResizeDeckVertically, "ScenarioControl")
                ResizeDeckVertically(ObjectPath&& deckPath,
                                     int newSize);

                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;

                int m_originalSize {};
                int m_newSize {};
        };
    }
}
