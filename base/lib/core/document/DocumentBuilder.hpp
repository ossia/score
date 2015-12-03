#pragma once
class QByteArray;
class QVariant;
#include <iscore/tools/SettableIdentifier.hpp>

namespace iscore
{
class Document;
class DocumentBackupManager;
class DocumentDelegateFactoryInterface;
class DocumentModel;
struct ApplicationContext;

/**
 * @brief The DocumentBuilder class
 *
 * Facility to construct a document according to different cases :
 * - Creating a blank, new document.
 * - Loading a document.
 * - Restoring a document after a crash.
 *
 */
class DocumentBuilder
{
    public:
        explicit DocumentBuilder(
            QObject* parentPresenter,
            QWidget* parentView);

        Document* newDocument(
                const iscore::ApplicationContext& ctx,
                const Id<DocumentModel>& id,
                iscore::DocumentDelegateFactoryInterface* doctype);
        Document* loadDocument(
                const iscore::ApplicationContext& ctx,
                const QVariant &data,
                iscore::DocumentDelegateFactoryInterface* doctype);
        Document* restoreDocument(
                const iscore::ApplicationContext& ctx,
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
                const iscore::ApplicationContext& ctx,
                const QVariant &data,
                iscore::DocumentDelegateFactoryInterface* doctype,
                InitFun&& initfun,
                BackupFun&& backupfun);

        QObject* m_parentPresenter{};
        QWidget* m_parentView{};

        DocumentBackupManager* m_backupManager{};
};

}
