#pragma once
#include <QTemporaryFile>
#include <QStack>
namespace iscore
{
    class CommandStack;

    /**
     * @brief The CommandBackupFile class
     *
     * Synchronizes the commands of a document to an on-disk file,
     * by maintaining serialized stacks of commands at each new command.
     *
     * This way, if there is a crash, the document can be restored from the
     * last successful command and only the latest user action is lost.
     */
    class CommandBackupFile : public QObject
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
            QTemporaryFile m_file;

            int m_previousIndex{};
            QStack<QPair <QPair <std::string, std::string>, QByteArray> > m_savedUndo, m_savedRedo;
    };
}
