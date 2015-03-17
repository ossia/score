#pragma once
#include <QObject>
#include <iscore/tools/ObjectPath.hpp>
namespace iscore
{

    class ObjectLocker : public QObject
    {
            Q_OBJECT
        public:
            ObjectLocker(QObject* parent);

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

}
