#pragma once
#include <QTemporaryFile>
#include <QStack>
namespace iscore
{
    class CommandStack;

    // Will synchronize the Commands on the document with an on-disk file.
    class CommandBackupFile : public QObject
    {
        public:
            CommandBackupFile(const iscore::CommandStack& stack, QObject* parent);
            QString fileName() const;

        private:
            void commit();

            const iscore::CommandStack& m_stack;
            QTemporaryFile m_file;

            int m_previousIndex{};
            QStack<QPair <QPair <QString,QString>, QByteArray> > m_savedUndo, m_savedRedo;
    };
}
