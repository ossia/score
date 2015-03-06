#include "DocumentPresenter.hpp"

#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <core/tools/utilsCPP11.hpp>
#include <core/document/DocumentView.hpp>
#include <core/document/DocumentModel.hpp>
#include <interface/panel/PanelModelInterface.hpp>


using namespace iscore;

DocumentPresenter::DocumentPresenter(DocumentModel* m, DocumentView* v, QObject* parent) :
    NamedObject {"DocumentPresenter", parent},
            m_commandStack {std::make_unique<CommandStack> (this) },
            m_view{v},
            m_model{m}
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

void DocumentPresenter::setPresenterDelegate(DocumentDelegatePresenterInterface* pres)
{
    // TODO put in ctor instead
    if(m_presenter)
    {
        m_presenter->deleteLater();
    }

    m_presenter = pres;

    // Commands
    // TODO put the QueuedConnection in the OngoingCommandManager.

    // Selection
    // TODO same for SelectionDispatcher
}

//// Locking / unlocking ////
void DocumentPresenter::lock_impl()
{
    QByteArray arr;
    Serializer<DataStream> ser {&arr};
    ser.readFrom(m_lockedObject);
    emit lock(arr);
}

void DocumentPresenter::unlock_impl()
{
    QByteArray arr;
    Serializer<DataStream> ser {&arr};
    ser.readFrom(m_lockedObject);
    emit unlock(arr);
    m_lockedObject = ObjectPath();
}
