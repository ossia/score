#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <core/document/DocumentPresenter.hpp>

#include <iscore/plugins/panel/PanelFactoryInterface.hpp>
#include <iscore/plugins/panel/PanelPresenterInterface.hpp>
#include <iscore/plugins/panel/PanelModelInterface.hpp>

#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateViewInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegatePresenterInterface.hpp>

#include <QDebug>
#include <QLayout>
#include <QCryptographicHash>
#include <QJsonDocument>

using namespace iscore;
Document::Document(DocumentDelegateFactoryInterface* factory, QWidget* parentview, QObject* parent) :
    NamedObject {"Document", parent},
    m_model {new DocumentModel{factory, this}},
    m_view {new DocumentView{factory, this, parentview}},
    m_presenter {new DocumentPresenter{factory,
                                       m_model,
                                       m_view,
                                       this}}
{
    init();
}

Document::Document(QVariant data,
                   DocumentDelegateFactoryInterface* factory,
                   QWidget* parentview,
                   QObject* parent):
    NamedObject {"Document", parent},
    m_model {new DocumentModel{data, factory, this}},
    m_view {new DocumentView{factory, this, parentview}},
    m_presenter {new DocumentPresenter{factory,
                                       m_model,
                                       m_view,
                                       this}}
{
    init();
}

Document::~Document()
{
    // We need a custom destructor because
    // for the sake of simplicity, we want the presenter
    // to be deleted before the model.
    // (Else we would have to fine-grain the deletion of the selection stack).

    delete m_presenter;
}

// TODO the Document should receive the list of the current panels.
// If a panel is closed or so, its model is deleted in all documents.
// If a panel is opened, its model should be created in all documents,
// and if a new document is created, it should be created with models for all panels.
// When the current document changes, the global Presenter should
// setup the correct links between the Panels ({presenter,view}) and the displayed document.
void Document::setupNewPanel(PanelPresenterInterface* pres, PanelFactoryInterface* factory)
{
    auto model = factory->makeModel(m_model);
    m_model->addPanel(model);
}

void Document::bindPanelPresenter(PanelPresenterInterface* pres)
{
    using namespace std;
    auto localmodel = std::find_if(begin(model()->panels()),
                                   end(model()->panels()),
                                   [&] (PanelModelInterface* model)
    {
        return model->objectName() == pres->modelObjectName();
    });

    pres->setModel(*localmodel);
}

QByteArray Document::saveDocumentModelAsByteArray()
{
    return m_model->modelDelegate()->toByteArray();
}

QJsonObject Document::saveDocumentModelAsJson()
{
    return m_model->modelDelegate()->toJson();
}


QJsonObject Document::savePanelAsJson(const QString& panel)
{
    return m_model->modelDelegate()->toJson();
}

QByteArray Document::savePanelAsByteArray(const QString& panel)
{
    using namespace std;
    auto panelmodel = find_if(begin(model()->panels()),
                              end(model()->panels()),
                              [&] (PanelModelInterface* model) { return model->objectName() == panel;});

    return (*panelmodel)->toByteArray();
}

QJsonObject Document::saveAsJson()
{
    QJsonObject complete;
    complete["Scenario"] = saveDocumentModelAsJson();
    for(auto panel : model()->panels())
    {
        complete[panel->objectName()] = panel->toJson();
    }

    return complete;
}

QByteArray Document::saveAsByteArray()
{
    using namespace std;
    QByteArray global;
    QDataStream writer(&global, QIODevice::WriteOnly);

    auto docByteArray = saveDocumentModelAsByteArray();

    QVector<QPair<QString, QByteArray>> panelModels;
    std::transform(begin(model()->panels()),
                   end(model()->panels()),
                   std::back_inserter(panelModels),
                   [] (PanelModelInterface* panel)
    { return QPair<QString, QByteArray>{
            panel->objectName(),
            panel->toByteArray()};
    });

    writer << docByteArray;
    writer << panelModels;

    auto hash = QCryptographicHash::hash(global, QCryptographicHash::Algorithm::Sha512);
    writer << hash;

/*
 * TODO Save this, too.
    QVector<QPair<QString, QByteArray>> documentPluginModels;
    std::transform(begin(model()->panels()),
                   end(model()->panels()),
                   std::back_inserter(panelModels),
                   [] (DocumentDelegatePluginModel* plugin) {  return QPair<QString, QByteArray>{plugin->objectName(), plugin->toByteArray()}; });
                  */

    return global;
}

void Document::init()
{
    connect(&m_selectionStack, &SelectionStack::currentSelectionChanged,
            [&] (const Selection& s)
            {
                m_model->setNewSelection(s);
                for(auto& panel : m_model->panels())
                {
                    panel->setNewSelection(s);
                }
            });
}

