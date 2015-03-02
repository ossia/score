#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <core/document/DocumentPresenter.hpp>

#include <interface/panel/PanelFactoryInterface.hpp>
#include <interface/panel/PanelPresenterInterface.hpp>
#include <interface/panel/PanelModelInterface.hpp>

#include <interface/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <interface/documentdelegate/DocumentDelegateViewInterface.hpp>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>

#include <QDebug>
#include <QLayout>

using namespace iscore;
Document::Document(DocumentDelegateFactoryInterface* type, QWidget* parentview, QObject* parent) :
    NamedObject {"Document", parent},
    m_model {new DocumentModel{this}},
    m_view {new DocumentView{parentview}},
    m_presenter {new DocumentPresenter{m_model, m_view, this}}
{
    // TODO Do this in the initialization list.
    // Model setup
    m_model->setModelDelegate(type->makeModel(m_model));

    // View setup
    auto view = type->makeView(m_view);
    m_view->setViewDelegate(view);

    // Presenter setup
    auto pres = type->makePresenter(m_presenter, m_model->modelDelegate(), view);
    m_presenter->setPresenterDelegate(pres);
    emit newDocument_start();
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

// TODO Load should go in the global presenter.
void Document::load(QByteArray data)
{
    /*
    // Model setup
    m_model->setModelDelegate(m_currentDocumentType->makeModel(m_model, data));

    // TODO call newDocument_start if loaded from this computer, not if serialized from network.
    setupDocument();
    */
}

QByteArray Document::save()
{
    return m_model->modelDelegate()->save();
}

