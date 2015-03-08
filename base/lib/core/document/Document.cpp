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

Document::Document(const QByteArray& data,
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

QByteArray Document::save()
{
    return m_model->modelDelegate()->save();
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

