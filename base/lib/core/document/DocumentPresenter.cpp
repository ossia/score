#include "DocumentPresenter.hpp"

#include <plugin_interface/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <plugin_interface/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <plugin_interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>
#include <core/tools/utilsCPP11.hpp>
#include <core/document/DocumentView.hpp>
#include <core/document/DocumentModel.hpp>
#include <plugin_interface/panel/PanelModelInterface.hpp>


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

void DocumentPresenter::on_lock(QByteArray arr)
{
    ObjectPath objectToLock;

    Deserializer<DataStream> s {&arr};
    s.writeTo(objectToLock);

    auto obj = objectToLock.find<QObject>();
    // TODO it would be better to have a "lockable" concept / mixin to cast to.
    QMetaObject::invokeMethod(obj, "lock");
}

void DocumentPresenter::on_unlock(QByteArray arr)
{
    ObjectPath objectToUnlock;

    Deserializer<DataStream> s {&arr};
    s.writeTo(objectToUnlock);

    auto obj = objectToUnlock.find<QObject>();
    QMetaObject::invokeMethod(obj, "unlock");
}


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
