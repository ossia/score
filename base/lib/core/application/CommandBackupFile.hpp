#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/command/CommandData.hpp>
#include <QByteArray>
#include <QObject>
#include <QPair>
#include <QStack>
#include <QString>
#include <QTemporaryFile>

namespace iscore
{
    class CommandStack;
    struct CommandStackBackup
    {
            CommandStackBackup(const iscore::CommandStack& stack);

            QStack<CommandData> savedUndo;
            QStack<CommandData> savedRedo;
    };

    /**
     * @brief The CommandBackupFile class
     *
     * Synchronizes the commands of a document to an on-disk file,
     * by maintaining serialized stacks of commands at each new command.
     *
     * This way, if there is a crash, the document can be restored from the
     * last successful command and only the latest user action is lost.
     */
    class CommandBackupFile final : public QObject
    {
        public:
            CommandBackupFile(const iscore::CommandStack& stack, QObject* parent);
            QString fileName() const;

        private:
            void on_push();
            void on_undo();
            void on_redo();
            void on_indexChanged();

            // Writes the current buffers to disk.
            void commit();

            const iscore::CommandStack& m_stack;
            CommandStackBackup m_backup;

            QTemporaryFile m_file;

            int m_previousIndex{};
    };
}
