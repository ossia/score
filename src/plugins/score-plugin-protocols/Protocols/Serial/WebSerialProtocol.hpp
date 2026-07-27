#pragma once
#include <ossia/detail/config.hpp>

#if defined(OSSIA_PROTOCOL_SERIAL) && defined(__EMSCRIPTEN__)
#include <ossia-qt/serial/serial_protocol.hpp>

#include <QByteArray>
#include <QJSValue>
#include <QObject>
#include <QQmlComponent>

#include <string>

class QQmlEngine;
class QTimer;

namespace Protocols
{
/**
 * @brief Serial protocol backed by the browser's Web Serial API.
 *
 * Everything runs on the browser's main thread: navigator.serial hands out
 * SerialPort objects that cannot be transferred to a worker, and the QML engine
 * driving the device tree lives there too. push() hops back to it when the
 * execution engine writes from the audio thread.
 */
class WebSerialProtocol final
    : public QObject
    , public ossia::net::protocol_base
{
public:
  WebSerialProtocol(const QByteArray& code, std::string portId, int baudRate);
  ~WebSerialProtocol() override;

  static ossia::net::serial_parameter_data read_data(const QJSValue& js) { return js; }

  bool pull(ossia::net::parameter_base&) override;
  bool push(const ossia::net::parameter_base&, const ossia::value& v) override;
  bool push_raw(const ossia::net::full_parameter_data&) override;
  bool observe(ossia::net::parameter_base&, bool) override;
  bool update(ossia::net::node_base&) override;

  void set_device(ossia::net::device_base& dev) override;
  void stop() override;

private:
  void startEngine();
  void onComponentStatus(QQmlComponent::Status status);
  void makeDefaultTree();
  void openPort();
  void poll();
  void onFrame(const QByteArray& frame);
  void doWrite(const ossia::net::parameter_base&, const ossia::value&);
  void writeBytes(const QByteArray&);

  QByteArray m_code;
  std::string m_portId;
  int m_baudRate{};

  QQmlEngine* m_engine{};
  QQmlComponent* m_component{};
  QTimer* m_timer{};

  ossia::net::device_base* m_device{};
  QObject* m_object{};
  QJSValue m_jsObj{};
  QJSValue m_onTextMessage{};
  QJSValue m_onBinaryMessage{};
  QJSValue m_onRead{};

  QByteArray m_delimiter{"\r\n"};
  QByteArray m_pending;
  bool m_lineFraming{true};

  int m_handle{};
  bool m_opened{};
  bool m_reportedError{};
};

using WebSerialDevice
    = ossia::net::wrapped_device<ossia::net::serial_node, WebSerialProtocol>;
}
#endif
