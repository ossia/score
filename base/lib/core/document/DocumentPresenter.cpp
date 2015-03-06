#include "DocumentPresenter.hpp"

#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <interface/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <core/tools/utilsCPP11.hpp>
#include <core/document/DocumentView.hpp>
#include <core/document/DocumentModel.hpp>
#include <interface/panel/PanelModelInterface.hpp>


using namespace iscore;

DocumentPresenter::DocumentPresenter(DocumentDelegateFactoryInterface* fact,
                                     DocumentModel* m,
                                     DocumentView* v,
                                     QObject* parent) :
    NamedObject {"DocumentPresenter", parent},
            m_view{v},
            m_model{m},
            m_presenter{fact->makePresenter(this,
                                            m_model->modelDelegate(),
                                            m_view->viewDelegate())}
{
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
