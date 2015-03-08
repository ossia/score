#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <QVector>

namespace Scenario
{
    namespace Command
    {
        // TODO Use AggregateCommand instead
        class RemoveMultipleElements : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(RemoveMultipleElements, "ScenarioControl")
                RemoveMultipleElements(QVector<SerializableCommand*> elementsToDelete);
                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                QVector<QPair<
                QPair<QString, QString>, // Meta-data
                      QByteArray>>
                      m_serializedCommands;
                QVector<QPair<
                QPair<QString, QString>, // Meta-data
                      QByteArray>>
                      m_serializedCommandsDecreasing;
        };

    }
}
