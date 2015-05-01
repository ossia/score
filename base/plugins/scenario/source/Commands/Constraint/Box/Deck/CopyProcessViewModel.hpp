#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
class ProcessViewModelInterface;
class ProcessSharedModelInterface;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The CopyProcessViewModel class
         *
         * Copy a process view from a Deck to another.
         * Note : this must be in the same constraint.
         * Note : there cannot be two Process View Models of the same Process in the same Deck.
         * It is up to the user of this Command to prevent this.
         *
         * For instance, a message could be displayed saying that the PVM cannot be copied
         * as long as there is another pvm for the same process in the other deck (same for the merging of decks).
         */
        class CopyProcessViewModel : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(CopyProcessViewModel, "ScenarioControl")
                CopyProcessViewModel(ObjectPath&& pvmToCopy,
                                     ObjectPath&& targetDeck);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_pvmPath;
                ObjectPath m_targetDeckPath;

                id_type<ProcessViewModelInterface> m_newProcessViewModelId;
        };
    }
}
