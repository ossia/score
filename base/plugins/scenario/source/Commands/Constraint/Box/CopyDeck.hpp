#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

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
                CopyDeck();
                CopyDeck(ObjectPath&& deckToCopy,
                         ObjectPath&& targetBoxPath);

                virtual void undo() override;
                virtual void redo() override;
                virtual int id() const override;
                virtual bool mergeWith(const QUndoCommand* other) override;

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
