#pragma once

class QVariant;
class QByteArray;
namespace iscore
{
class Presenter;
class Document;
class DocumentDelegateFactoryInterface;
class DocumentBackupManager;
class DocumentBuilder
{
    public:
        DocumentBuilder(Presenter& pres);

        Document* newDocument(
                iscore::DocumentDelegateFactoryInterface* doctype);
        Document* loadDocument(
                const QVariant &data,
                iscore::DocumentDelegateFactoryInterface* doctype);
        Document* restoreDocument(
                const QByteArray &docData,
                const QByteArray &cmdData,
                iscore::DocumentDelegateFactoryInterface* doctype);

    private:
        void setBackupManager(Document* doc);
        template<
                typename InitFun, // for setup of m_backupManager
                typename BackupFun // the model data to save
        >
        Document* loadDocument_impl(
                const QVariant &data,
                iscore::DocumentDelegateFactoryInterface* doctype,
                InitFun&& initfun,
                BackupFun&& backupfun);

        Presenter& m_presenter;
        DocumentBackupManager* m_backupManager{};
};

}
