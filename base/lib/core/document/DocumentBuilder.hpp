#pragma once
class QByteArray;
class QVariant;
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_lib_base_export.h>
namespace iscore
{
class Document;
class DocumentBackupManager;
class DocumentDelegateFactory;
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
class ISCORE_LIB_BASE_EXPORT DocumentBuilder
{
    public:
        explicit DocumentBuilder(
            QObject* parentPresenter,
            QWidget* parentView);

        Document* newDocument(
                const iscore::ApplicationContext& ctx,
                const Id<DocumentModel>& id,
                iscore::DocumentDelegateFactory* doctype);
        Document* loadDocument(
                const iscore::ApplicationContext& ctx,
                const QVariant &data,
                iscore::DocumentDelegateFactory* doctype);
        Document* restoreDocument(
                const iscore::ApplicationContext& ctx,
                const QByteArray &docData,
                const QByteArray &cmdData,
                iscore::DocumentDelegateFactory* doctype);

    private:
        void setBackupManager(Document* doc);
        template<
                typename InitFun, // for setup of m_backupManager
                typename BackupFun // the model data to save
        >
        Document* loadDocument_impl(
                const iscore::ApplicationContext& ctx,
                const QVariant &data,
                iscore::DocumentDelegateFactory* doctype,
                InitFun&& initfun,
                BackupFun&& backupfun);

        QObject* m_parentPresenter{};
        QWidget* m_parentView{};

        DocumentBackupManager* m_backupManager{};
};

}
