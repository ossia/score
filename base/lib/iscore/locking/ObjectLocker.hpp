#pragma once
#include <QObject>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>

namespace iscore
{
/**
 * @brief The ObjectLocker class
 *
 * In network operation, will graphically lock an object on the other
 * computers.
 *
 * For instance, if user A starts moving a constraint in a scenario, the
 * other users won't be able to change the scenario.
 */
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

        std::vector<ObjectPath> m_lockedObjects;
};

/**
 * @brief The LockHelper class
 *
 * Class to be used in command dispatchers, so that
 * they automatically block the element no matter the commands sent through.
 */
class LockHelper
{
    public:
        LockHelper(QObject& model, ObjectLocker& locker);

        LockHelper(ObjectPath&& path, ObjectLocker& locker);

        ~LockHelper();

        void lock();

        void unlock();

    private:
        ObjectPath m_path;
        QByteArray m_serializedPath;
        ObjectLocker& m_locker;
        bool m_locked{false};

};

}
