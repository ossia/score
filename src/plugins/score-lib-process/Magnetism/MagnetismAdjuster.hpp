#pragma once
#include <Magnetism/MagneticInfo.hpp>
#include <Process/TimeValue.hpp>

#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <QPointer>

#include <score_lib_process_export.h>

#include <utility>
#include <vector>

namespace Process
{
class ProcessModel;

class SCORE_LIB_PROCESS_EXPORT MagnetismAdjuster final
    : public QObject
    , public score::InterfaceListBase
{
public:
  MagnetismAdjuster() noexcept;
  ~MagnetismAdjuster() noexcept;

  MagneticInfo getPosition(const QObject* obj, TimeVal original) noexcept;

  // Shortcut for some classes : the API to implement must look like
  // Position magneticPosition(Position
  template <typename T>
  void registerHandler(T& context) noexcept
  {
    registerHandler(&context, [&context](const QObject* obj, TimeVal t) {
      return context.magneticPosition(obj, t);
    });
  }
  void registerHandler(QObject* context, MagnetismHandler h) noexcept;
  void unregisterHandler(QObject* context) noexcept;

  static score::InterfaceKey static_interfaceKey() noexcept;
  score::InterfaceKey interfaceKey() const noexcept override;

private:
  void insert(std::unique_ptr<score::InterfaceBase>) override;
  void optimize() noexcept override;

  std::vector<std::pair<QPointer<QObject>, MagnetismHandler>> m_handlers;
};

}
