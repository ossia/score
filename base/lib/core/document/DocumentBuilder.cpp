#include "DocumentBuilder.hpp"
#include <core/presenter/Presenter.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentBackupManager.hpp>
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include <core/view/View.hpp>

#include <QByteArray>
#include <QMessageBox>
using namespace iscore;

DocumentBuilder::DocumentBuilder(iscore::Presenter& pres):
    m_presenter{pres}
{

}

Document* DocumentBuilder::newDocument(
        const Id<DocumentModel>& id,
        DocumentDelegateFactoryInterface* doctype)
{
    auto doc = new Document{id, doctype, m_presenter.view(), &m_presenter};

    m_backupManager = new DocumentBackupManager{*doc};
    for(auto& control: m_presenter.applicationComponents().controls())
    {
        control->on_newDocument(doc);
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
        const QVariant &docData,
        iscore::DocumentDelegateFactoryInterface* doctype,
        InitFun&& initfun,
        BackupFun&& backupfun)
{

    Document* doc = nullptr;
    try
    {
        doc = new Document{docData, doctype, m_presenter.view(), &m_presenter};
        initfun(doc);
        m_backupManager =  new DocumentBackupManager{*doc};

        for(auto& control: m_presenter.applicationComponents().controls())
        {
            control->on_loadedDocument(doc);
        }

        m_backupManager->saveModelData(backupfun(doc));
        setBackupManager(doc);

        return doc;
    }
    catch(std::runtime_error& e)
    {
        QMessageBox::warning(m_presenter.view(), QObject::tr("Error"), e.what());
        delete doc;
        return nullptr;
    }
}


Document* DocumentBuilder::loadDocument(
        const QVariant& docData,
        DocumentDelegateFactoryInterface* doctype)
{
    return loadDocument_impl(
                docData,
                doctype,
                [] (iscore::Document*) { },
                [] (iscore::Document* doc) { return doc->saveAsByteArray(); }
    );
}

Document* DocumentBuilder::restoreDocument(
        const QByteArray& docData,
        const QByteArray& cmdData,
        DocumentDelegateFactoryInterface* doctype)
{
    return loadDocument_impl(
                docData,
                doctype,
                [&] (iscore::Document* doc) {
        // We restore the pre-crash command stack.
        Deserializer<DataStream> writer(cmdData);
        writer.writeTo(doc->commandStack());
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
