#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <QString>

#include <tests/helpers/ForwardDeclaration.hpp>
class ProcessSharedModelInterface;
class BoxModel;
class DeckModel;
class ProcessViewModelInterface;

namespace Scenario
{
    namespace Command
    {
        /**
        * @brief The AddProcessViewInNewDeck class
        */
        class AddProcessViewInNewDeck : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(AddProcessViewInNewDeck, "ScenarioControl")
                AddProcessViewInNewDeck(ObjectPath&& constraintPath, QString process);

                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

                id_type<ProcessSharedModelInterface> processId() const
                {
                    return m_processId;
                }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                ObjectPath m_processPath;
                QString m_processName;

                bool m_existingBox {};

                id_type<ProcessSharedModelInterface> m_processId {};
                id_type<BoxModel> m_createdBoxId {};
                id_type<DeckModel> m_createdDeckId {};
                id_type<ProcessViewModelInterface> m_createdProcessViewId {};
                id_type<ProcessSharedModelInterface> m_sharedProcessModelId {};
        };
    }
}
