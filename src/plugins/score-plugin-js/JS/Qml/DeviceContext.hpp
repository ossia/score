#pragma once
#include <QObject>

#include <verdigris>

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
  explicit DeviceContext(QObject* parent = nullptr);
  ~DeviceContext();

  bool init();
  ossia::net::node_base* find(const QString& addr);

  QVariant read(const QString& address);
  W_SLOT(read);

  void write(const QString& address, const QVariant& value);
  W_SLOT(write);

  ossia::qt::qml_engine_functions* m_impl{};
};
}
