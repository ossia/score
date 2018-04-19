#pragma once
#include <QByteArray>
#include <QObject>
#include <score/model/path/ObjectPath.hpp>
#include <vector>
#include <wobjectdefs.h>
namespace score
{
/**
 * @brief The ObjectLocker class
 *
 * In network operation, will graphically lock an object on the other
 * computers.
 *
 * For instance, if user A starts moving a interval in a scenario, the
 * other users won't be able to change the scenario.
 */
class SCORE_LIB_BASE_EXPORT ObjectLocker : public QObject
{
  W_OBJECT(ObjectLocker)
public:
  explicit ObjectLocker(QObject* parent);

  // To the network
  void lock(QByteArray b) W_SIGNAL(lock, b) void unlock(QByteArray b)
      W_SIGNAL(unlock, b)

      // From the network
      void on_lock(QByteArray b);
  W_INVOKABLE(on_lock)
  void on_unlock(QByteArray);
  W_INVOKABLE(on_unlock)

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
