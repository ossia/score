#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <qbytearray.h>
#include <qobject.h>
#include <qpair.h>
#include <qstack.h>
#include <qstring.h>
#include <qtemporaryfile.h>

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
            QTemporaryFile m_file;

            int m_previousIndex{};
            QStack<QPair <QPair <CommandParentFactoryKey, CommandFactoryKey>, QByteArray> > m_savedUndo, m_savedRedo;
    };
}
