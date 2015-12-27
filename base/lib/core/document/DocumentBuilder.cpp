#include <core/document/Document.hpp>
#include <core/document/DocumentBackupManager.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <QByteArray>
#include <QMessageBox>
#include <QObject>

#include <QString>
#include <QVariant>
#include <stdexcept>

#include "DocumentBuilder.hpp"
#include <iscore/application/ApplicationComponents.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <core/command/CommandStackSerialization.hpp>

#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/RandomNameProvider.hpp>
using namespace iscore;

DocumentBuilder::DocumentBuilder(
        QObject* parentPresenter,
        QWidget* parentView):
    m_parentPresenter{parentPresenter},
    m_parentView{parentView}
{

}

ISCORE_LIB_BASE_EXPORT
Document* DocumentBuilder::newDocument(
        const iscore::ApplicationContext& ctx,
        const Id<DocumentModel>& id,
        DocumentDelegateFactoryInterface* doctype)
{
    QString docName = "Untitled." + RandomNameProvider::generateRandomName();
    auto doc = new Document{docName, id, doctype, m_parentView, m_parentPresenter};

    m_backupManager = new DocumentBackupManager{*doc};
    for(auto& appPlug: ctx.components.applicationPlugins())
    {
        appPlug->on_newDocument(doc);
    }

    // First save
    m_backupManager->saveModelData(doc->saveAsByteArray());
    setBackupManager(doc);

    return doc;
}

template<
        typename InitFun, // for setup of m_backupManager
        typename BackupFun // the model data to save
>
Document* DocumentBuilder::loadDocument_impl(
        const iscore::ApplicationContext& ctx,
        const QVariant &docData,
        iscore::DocumentDelegateFactoryInterface* doctype,
        InitFun&& initfun,
        BackupFun&& backupfun)
{

    Document* doc = nullptr;
    try
    {
        doc = new Document{docData, doctype, m_parentView, m_parentPresenter};
        initfun(doc);
        m_backupManager =  new DocumentBackupManager{*doc};

        for(auto& appPlug: ctx.components.applicationPlugins())
        {
            appPlug->on_loadedDocument(doc);
        }

        m_backupManager->saveModelData(backupfun(doc));
        setBackupManager(doc);

        return doc;
    }
    catch(std::runtime_error& e)
    {
        QMessageBox::warning(m_parentView, QObject::tr("Error"), e.what());
        delete doc;
        return nullptr;
    }
}


Document* DocumentBuilder::loadDocument(
        const iscore::ApplicationContext& ctx,
        const QVariant& docData,
        DocumentDelegateFactoryInterface* doctype)
{
    return loadDocument_impl(
                ctx,
                docData,
                doctype,
                [] (iscore::Document*) { },
                [] (iscore::Document* doc) { return doc->saveAsByteArray(); }
    );
}

Document* DocumentBuilder::restoreDocument(
        const iscore::ApplicationContext& ctx,
        const QByteArray& docData,
        const QByteArray& cmdData,
        DocumentDelegateFactoryInterface* doctype)
{
    return loadDocument_impl(
                ctx,
                docData,
                doctype,
                [&] (iscore::Document* doc) {
        // We restore the pre-crash command stack.
        Deserializer<DataStream> writer(cmdData);
        loadCommandStack(
                    ctx.components,
                    writer,
                    doc->commandStack(),
                    [] (auto cmd) { cmd->redo(); }
        );
    },
                [&] (iscore::Document*) { return docData; }
);
}

void DocumentBuilder::setBackupManager(Document* doc)
{
    m_backupManager->updateBackupData();
    doc->setBackupMgr(m_backupManager);
    m_backupManager = nullptr;
}
