#pragma once
#include <QTemporaryFile>
#include <core/application/CommandBackupFile.hpp>
namespace iscore
{
class CommandBackupFile;
class Document;
class DocumentBackupManager : public QObject
{
    public:
        DocumentBackupManager(Document& doc);

        ~DocumentBackupManager();

        void saveModelData(const QByteArray&);

        void updateBackupData();

    private:
        QTemporaryFile& crashDataFile();
        CommandBackupFile& crashCommandFile();

        Document& m_doc;
        QTemporaryFile m_modelFile;
        CommandBackupFile* m_commandFile{};
};

}
