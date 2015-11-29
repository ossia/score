#pragma once
#include <qbytearray.h>
#include <qobject.h>
#include <qtemporaryfile.h>

namespace iscore
{
class CommandBackupFile;
class Document;

/**
 * @brief The DocumentBackupManager class
 *
 * Manages on-the-fly backup of documents to be able to restore gracefully in
 * case of a crash.
 *
 * There are two parts : one that replicates the on-disk part of a document
 * when it was loaded, and one that saves all the command that have been applied.
 *
 * TODO : update the document part each time the user saves the document. This
 * way there will be much fewer operations to apply if there is a crash.
 * However we have to be very careful in case of the user saving, and then going before
 * his "saved" point in the undo / redo list.
 * Maybe we should keep the previous document saves somewhere and reload from the
 * correct document save according to our position in the command stack.
 */
class DocumentBackupManager final : public QObject
{
    public:
        explicit DocumentBackupManager(Document& doc);

        ~DocumentBackupManager();

        void saveModelData(const QByteArray&);

        void updateBackupData();

    private:
        QTemporaryFile& crashDataFile();
        CommandBackupFile& crashCommandFile();

        QTemporaryFile m_modelFile;
        CommandBackupFile* m_commandFile{};
};

}
