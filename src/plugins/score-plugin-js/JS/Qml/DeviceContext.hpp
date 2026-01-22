#pragma once
#include <QObject>

#include <verdigris>

class QQmlEngine;
namespace ossia::net
{
class node_base;
}
namespace ossia::qt
{
class qml_engine_functions;
}

namespace JS
{
class qml_engine_functions;
class DeviceContext : public QObject
{
  W_OBJECT(DeviceContext)

public:
  explicit DeviceContext(QQmlEngine& engine, QObject* parent = nullptr);
  ~DeviceContext();

  bool init();
  ossia::net::node_base* find(const QString& addr);

  QVariant read(const QString& address);
  W_SLOT(read);

  void write(const QString& address, const QVariant& value);
  W_SLOT(write);

  void exec(const QString& code) W_SIGNAL(exec, code);
  void compute(const QString& code, const QString& cb) W_SIGNAL(compute, code, cb);
  void system(const QString& code) W_SIGNAL(system, code);

  /// Conversions ///
  QVariant asArray(QVariant v) const noexcept;
  W_SLOT(asArray)
  QVariant asColor(QVariant) const noexcept;
  W_SLOT(asColor)
  QVariant asVec2(QVariant) const noexcept;
  W_SLOT(asVec2)
  QVariant asVec3(QVariant) const noexcept;
  W_SLOT(asVec3)
  QVariant asVec4(QVariant) const noexcept;
  W_SLOT(asVec4)

  ossia::qt::qml_engine_functions* m_impl{};
  QQmlEngine& m_engine;
};
}
