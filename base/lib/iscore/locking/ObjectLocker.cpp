#include "ObjectLocker.hpp"
using namespace iscore;
//// Locking / unlocking ////

ObjectLocker::ObjectLocker(QObject* parent)
{

}

void ObjectLocker::on_lock(QByteArray arr)
{
    /*
    ObjectPath objectToLock;

    Deserializer<DataStream> s {&arr};
    s.writeTo(objectToLock);

    auto obj = objectToLock.find<QObject>();
    // TODO it would be better to have a "lockable" concept / mixin to cast to.
    // Or maybe only the ProcessSharedModel could be locked ? It would be much simpler...
    QMetaObject::invokeMethod(obj, "lock");
    */
}

void ObjectLocker::on_unlock(QByteArray arr)
{
    /*
    ObjectPath objectToUnlock;

    Deserializer<DataStream> s {&arr};
    s.writeTo(objectToUnlock);

    auto obj = objectToUnlock.find<QObject>();
    QMetaObject::invokeMethod(obj, "unlock");
    */
}


void ObjectLocker::lock_impl()
{
    /*
    QByteArray arr;
    Serializer<DataStream> ser {&arr};
    ser.readFrom(m_lockedObject);
    emit lock(arr);
    */
}

void ObjectLocker::unlock_impl()
{
    /*
    QByteArray arr;
    Serializer<DataStream> ser {&arr};
    ser.readFrom(m_lockedObject);
    emit unlock(arr);
    m_lockedObject = ObjectPath();
    */
}
