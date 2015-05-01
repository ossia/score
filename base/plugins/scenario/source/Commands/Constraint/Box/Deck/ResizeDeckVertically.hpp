#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

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
