#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

class ProcessViewModelInterface;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The RemoveProcessViewFromDeck class
         *
         * Removes a process view from a deck.
         */
        class RemoveProcessViewModelFromDeck : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(RemoveProcessViewModelFromDeck, "ScenarioControl")

                RemoveProcessViewModelFromDeck(ObjectPath&& pvmPath);
                RemoveProcessViewModelFromDeck(ObjectPath&& deckPath, id_type<ProcessViewModelInterface> processViewId);

                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<ProcessViewModelInterface> m_processViewId {};

                QByteArray m_serializedProcessViewData;
        };
    }
}
