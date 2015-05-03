#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
class ProcessViewModelInterface;
class ProcessModel;
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

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_deckPath;
                ObjectPath m_processPath;

                QByteArray m_processData;

                id_type<ProcessViewModelInterface> m_createdProcessViewId {};
        };
    }
}
