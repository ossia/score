#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
class ProcessViewModelInterface;
class ProcessSharedModelInterface;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The AddProcessViewToDeck class
         *
         * Adds a process view to a deck.
         */
        class AddProcessViewModelToDeck : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(AddProcessViewModelToDeck, "ScenarioControl")
                AddProcessViewModelToDeck(ObjectPath&& deck, ObjectPath&& process);

                virtual void undo() override;
                virtual void redo() override;
                virtual int id() const override;
                virtual bool mergeWith(const QUndoCommand* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_deckPath;
                ObjectPath m_processPath;

                id_type<ProcessViewModelInterface> m_createdProcessViewId {};
        };
    }
}
