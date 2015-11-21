#include "DocumentManager.hpp"
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>

#include <core/document/DocumentBackups.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>
#include <core/document/DocumentModel.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QFileDialog>
#include <QSaveFile>
#include <QMessageBox>
#include <QSettings>
namespace iscore
{
DocumentManager::DocumentManager(Presenter& p):
    m_presenter{p},
    m_builder{p}
{
    connect(m_presenter.view(), &View::activeDocumentChanged,
            this,   [&] (Document* doc) {
        prepareNewDocument();
        setCurrentDocument(doc);
    });


    connect(m_presenter.view(), &View::closeRequested,
            this,   &DocumentManager::closeDocument);

    m_recentFiles = new QRecentFilesMenu{tr("Recent files"), nullptr};

    QSettings settings("OSSIA", "i-score");
    m_recentFiles->restoreState(settings.value("RecentFiles").toByteArray());

    connect(m_recentFiles, &QRecentFilesMenu::recentFileTriggered,
            this, [&] (const QString& f) { loadFile(f); });

}

DocumentManager::~DocumentManager()
{
    QSettings settings("OSSIA", "i-score");
    settings.setValue("RecentFiles", m_recentFiles->saveState());

    // The documents have to be deleted before the application context plug-ins.
    // This is because the Local device has to be deleted last in OSSIAApplicationPlugin.
    for(auto document : m_documents)
    {
        delete document;
    }

    m_documents.clear();
    m_currentDocument = nullptr;
}

Document* DocumentManager::setupDocument(Document* doc)
{
    if(doc)
    {
        for(auto& panel : m_presenter.applicationComponents().panelPresenters())
        {
            doc->setupNewPanel(panel.second);
        }

        m_documents.push_back(doc);
        m_presenter.view()->addDocumentView(&doc->view());
        setCurrentDocument(doc);
        connect(&doc->model(), &DocumentModel::fileNameChanged,
                this, [=] (const QString& s)
        {
            m_presenter.view()->on_fileNameChanged(&doc->view(), s);
        });
    }
    else
    {
        setCurrentDocument(m_documents.empty() ? nullptr : m_documents.front());
    }

    return doc;
}

Document *DocumentManager::currentDocument() const
{
    return m_currentDocument;
}

void DocumentManager::setCurrentDocument(Document* doc)
{
    auto old = m_currentDocument;
    m_currentDocument = doc;

    for(auto& pair : m_presenter.applicationComponents().panelPresenters())
    {
        if(doc)
            m_currentDocument->bindPanelPresenter(pair.first);
        else
            pair.first->setModel(nullptr);
    }

    for(auto& ctrl : m_presenter.applicationComponents().applicationPlugins())
    {
        emit ctrl->documentChanged(old, m_currentDocument);
    }

    emit currentDocumentChanged(doc);
}

bool DocumentManager::closeDocument(Document* doc)
{
    // Warn the user if he might loose data
    if(!doc->commandStack().isAtSavedIndex())
    {
        QMessageBox msgBox;
        msgBox.setText(tr("The document has been modified."));
        msgBox.setInformativeText(tr("Do you want to save your changes?"));
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();
        switch (ret)
        {
            case QMessageBox::Save:
                if(saveDocument(doc))
                    break;
                else
                    return false;
            case QMessageBox::Discard:
                // Do nothing
                break;
            case QMessageBox::Cancel:
                return false;
                break;
            default:
                break;
        }
    }

    // Close operation
    m_presenter.view()->closeDocument(&doc->view());
    remove_one(m_documents, doc);
    setCurrentDocument(m_documents.size() > 0 ? m_documents.back() : nullptr);

    delete doc;
    return true;
}

bool DocumentManager::saveDocument(Document * doc)
{
    auto savename = doc->docFileName();

    if(savename == tr("Untitled"))
    {
        saveDocumentAs(doc);
    }
    else if (savename.size() != 0)
    {
        QSaveFile f{savename};
        f.open(QIODevice::WriteOnly);
        if(savename.indexOf(".scorebin") != -1)
            f.write(doc->saveAsByteArray());
        else
        {
            QJsonDocument json_doc;
            json_doc.setObject(doc->saveAsJson());

            f.write(json_doc.toJson());
        }
        f.commit();
    }

    return true;
}

bool DocumentManager::saveDocumentAs(Document * doc)
{
    QFileDialog d{m_presenter.view(), tr("Save Document As")};
    QString binFilter{tr("Binary (*.scorebin)")};
    QString jsonFilter{tr("JSON (*.scorejson)")};
    QStringList filters;
    filters << jsonFilter
            << binFilter;

    d.setNameFilters(filters);
    d.setConfirmOverwrite(true);
    d.setFileMode(QFileDialog::AnyFile);
    d.setAcceptMode(QFileDialog::AcceptSave);

    if(d.exec())
    {
        QString savename = d.selectedFiles().first();
        auto suf = d.selectedNameFilter();

        if(!savename.isEmpty())
        {
            if(suf == binFilter)
            {
                if(!savename.contains(".scorebin"))
                    savename += ".scorebin";
            }
            else
            {
                if(!savename.contains(".scorejson"))
                    savename += ".scorejson";
            }

            QSaveFile f{savename};
            f.open(QIODevice::WriteOnly);
            doc->setDocFileName(savename);
            if(savename.indexOf(".scorebin") != -1)
                f.write(doc->saveAsByteArray());
            else
            {
                QJsonDocument json_doc;
                json_doc.setObject(doc->saveAsJson());

                f.write(json_doc.toJson());
            }
            f.commit();
        }
        return true;
    }
    return false;
}

Document* DocumentManager::loadFile()
{
    QString loadname = QFileDialog::getOpenFileName(m_presenter.view(), tr("Open"), QString(), "*.scorebin *.scorejson");
    return loadFile(loadname);
}

Document* DocumentManager::loadFile(const QString& fileName)
{
    Document* doc{};
    if(!fileName.isEmpty()
    && (fileName.indexOf(".scorebin") != -1
     || fileName.indexOf(".scorejson") != -1 ))
    {
        QFile f {fileName};
        if(f.open(QIODevice::ReadOnly))
        {
            if (fileName.indexOf(".scorebin") != -1)
            {
                doc = loadDocument(f.readAll(), m_presenter.applicationComponents().availableDocuments().front());
            }
            else if (fileName.indexOf(".scorejson") != -1)
            {
                auto json = QJsonDocument::fromJson(f.readAll());
                doc = loadDocument(json.object(), m_presenter.applicationComponents().availableDocuments().front());
            }

            m_currentDocument->setDocFileName(fileName);
            m_recentFiles->addRecentFile(fileName);
        }
    }

    return doc;
}

void DocumentManager::prepareNewDocument()
{
    for(GUIApplicationContextPlugin* appPlugin : m_presenter.applicationComponents().applicationPlugins())
    {
        appPlugin->prepareNewDocument();
    }
}

bool DocumentManager::closeAllDocuments()
{
    while(!m_documents.empty())
    {
        bool b = closeDocument(m_documents.back());
        if(!b)
            return false;
    }

    return true;
}



void DocumentManager::restoreDocuments()
{
    for(const auto& backup : DocumentBackups::restorableDocuments())
    {
        restoreDocument(backup.first, backup.second, m_presenter.applicationComponents().availableDocuments().front());
    }
}

}
