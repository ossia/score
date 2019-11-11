#pragma once
#include <Process/TimeValue.hpp>

#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score_lib_process_export.h>

#include <QPointer>

#include <utility>
#include <vector>

namespace Process
{

class SCORE_LIB_PROCESS_EXPORT MagnetismAdjuster final : public score::InterfaceListBase
{
public:
  MagnetismAdjuster() noexcept;
  ~MagnetismAdjuster() noexcept;

  using MagnetismHandler = std::function<TimeVal(TimeVal)>;

  TimeVal getPosition(TimeVal original) noexcept;

  // Shortcut for some classes : the API to implement must look like
  // Position magneticPosition(Position
  template<typename T>
  void registerHandler(T& context) noexcept
  {
    registerHandler(&context, [&context] (TimeVal t) {
      return context.magneticPosition(t);
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
