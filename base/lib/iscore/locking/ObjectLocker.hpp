#pragma once
#include <QObject>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/document/DocumentInterface.hpp>
namespace iscore
{
    class ObjectLocker : public QObject
    {
            Q_OBJECT
        public:
            explicit ObjectLocker(QObject* parent);

        signals:
            // To the network
            void lock(QByteArray);
            void unlock(QByteArray);

        public slots:
            // From the network
            void on_lock(QByteArray);
            void on_unlock(QByteArray);

        private:
            // In the commands
            void lock_impl();
            void unlock_impl();

            QList<ObjectPath> m_lockedObjects;
    };

    class LockHelper
    {
        public:
            LockHelper(QObject& model, ObjectLocker& locker):
                m_path{IDocument::safe_path(model)},
                m_locker{locker}
            {
                Serializer<DataStream> ser {&m_serializedPath};
                ser.readFrom(m_path);
            }

            LockHelper(ObjectPath&& path, ObjectLocker& locker):
                m_path{std::move(path)},
                m_locker{locker}
            {
                Serializer<DataStream> ser {&m_serializedPath};
                ser.readFrom(m_path);
            }

            ~LockHelper()
            {
                if(m_locked)
                    unlock();
            }

            void lock()
            {
                emit m_locker.lock(m_serializedPath);
                m_locked = true;
            }

            void unlock()
            {
                emit m_locker.unlock(m_serializedPath);
                m_locked = false;
            }

        private:
              ObjectPath m_path;
              QByteArray m_serializedPath;
              ObjectLocker& m_locker;
              bool m_locked{false};

    };

}
