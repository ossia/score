#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <QString>

#include <tests/helpers/ForwardDeclaration.hpp>
class ProcessModel;
class BoxModel;
class DeckModel;
class ProcessViewModel;

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
                AddProcessViewInNewDeck(ObjectPath&& constraintPath,
                                        id_type<ProcessModel> process);

                virtual void undo() override;
                virtual void redo() override;

                id_type<ProcessModel> processId() const
                {
                    return m_processId;
                }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;

                bool m_existingBox {};

                id_type<ProcessModel> m_processId {};
                id_type<BoxModel> m_createdBoxId {};
                id_type<DeckModel> m_createdDeckId {};
                id_type<ProcessViewModel> m_createdProcessViewId {};
                id_type<ProcessModel> m_sharedProcessModelId {};

                QByteArray m_processData;
        };
    }
}
