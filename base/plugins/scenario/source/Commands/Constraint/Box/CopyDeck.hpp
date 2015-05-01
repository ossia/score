#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>

class DeckModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The CopyProcessViewModel class
         *
         * Copy a deck, in any Box of its parent constraint.
         * The process view models are recursively copied.
         * The Deck is put at the end.
         */
        class CopyDeck : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(CopyDeck, "ScenarioControl")
                CopyDeck(ObjectPath&& deckToCopy,
                         ObjectPath&& targetBoxPath);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_deckPath;
                ObjectPath m_targetBoxPath;

                id_type<DeckModel> m_newDeckId;
        };
    }
}
