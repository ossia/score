// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ObjectLocker.hpp"

#include <algorithm>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/ObjectPath.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(score::ObjectLocker)
namespace score
{

//// Locking / unlocking ////

ObjectLocker::ObjectLocker(QObject* parent)
{
}

void ObjectLocker::on_lock(QByteArray arr)
{
  /*
  ObjectPath objectToLock;

  DataStream::Deserializer s {&arr};
  s.writeTo(objectToLock);

  auto obj = objectToLock.find<QObject>();
  // TODO it would be better to have a "lockable" concept / mixin to cast to.
  // Or maybe only the ProcessSharedModel could be locked ? It would be much
  simpler...
  QMetaObject::invokeMethod(obj, "lock");
  */
}

void ObjectLocker::on_unlock(QByteArray arr)
{
  /*
  ObjectPath objectToUnlock;

  DataStream::Deserializer s {&arr};
  s.writeTo(objectToUnlock);

  auto obj = objectToUnlock.find<QObject>();
  QMetaObject::invokeMethod(obj, "unlock");
  */
}

void ObjectLocker::lock_impl()
{
  /*
  QByteArray arr;
  DataStream::Serializer ser {&arr};
  ser.readFrom(m_lockedObject);
  lock(arr);
  */
}

void ObjectLocker::unlock_impl()
{
  /*
  QByteArray arr;
  DataStream::Serializer ser {&arr};
  ser.readFrom(m_lockedObject);
  unlock(arr);
  m_lockedObject = ObjectPath();
  */
}

LockHelper::LockHelper(QObject& model, ObjectLocker& locker)
    : m_path{IDocument::unsafe_path(model)}, m_locker{locker}
{
  DataStream::Serializer ser{&m_serializedPath};
  ser.readFrom(m_path);
}

LockHelper::LockHelper(ObjectPath&& path, ObjectLocker& locker)
    : m_path{std::move(path)}, m_locker{locker}
{
  DataStream::Serializer ser{&m_serializedPath};
  ser.readFrom(m_path);
}

LockHelper::~LockHelper()
{
  if (m_locked)
    unlock();
}

void LockHelper::lock()
{
  m_locker.lock(m_serializedPath);
  m_locked = true;
}

void LockHelper::unlock()
{
  m_locker.unlock(m_serializedPath);
  m_locked = false;
}
}
