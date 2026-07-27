#include <Protocols/Serial/WebSerialProtocol.hpp>

#if defined(OSSIA_PROTOCOL_SERIAL) && defined(__EMSCRIPTEN__)
#include <Protocols/Serial/WebSerial.hpp>

#include <ossia/network/base/node_functions.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia-qt/js_utilities.hpp>

#include <QDebug>
#include <QJSValueIterator>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTimer>

namespace Protocols
{
namespace
{
constexpr int poll_interval = 16;
constexpr int read_chunk = 4096;

QByteArray valueToBytes(const ossia::value& v);

struct value_bytes_visitor
{
  QByteArray operator()(const std::string& s) const noexcept
  {
    return QByteArray::fromStdString(s);
  }
  QByteArray operator()(char c) const noexcept { return QByteArray(1, c); }
  QByteArray operator()(int32_t i) const noexcept
  {
    return QByteArray(1, static_cast<char>(i & 0xFF));
  }
  QByteArray operator()(bool b) const noexcept { return QByteArray(1, b ? 1 : 0); }
  QByteArray operator()(const std::vector<ossia::value>& l) const noexcept
  {
    QByteArray res;
    res.reserve(l.size());
    for(const auto& elt : l)
      res += valueToBytes(elt);
    return res;
  }
  QByteArray operator()(const auto&) const noexcept { return {}; }
  QByteArray operator()() const noexcept { return {}; }
};

QByteArray valueToBytes(const ossia::value& v)
{
  return v.apply(value_bytes_visitor{});
}

void performReplacements(
    const ossia::net::parameter_base& addr, const ossia::value& v, QString& str)
{
  if(!str.contains("$val"))
    return;

  switch(addr.get_value_type())
  {
    case ossia::val_type::FLOAT:
      str.replace("$val", QString::number(ossia::convert<float>(v), 'g', 4));
      break;
    case ossia::val_type::INT:
      str.replace("$val", QString::number(ossia::convert<int32_t>(v)));
      break;
    case ossia::val_type::BOOL:
      str.replace("$val", ossia::convert<bool>(v) ? "1" : "0");
      break;
    case ossia::val_type::STRING:
      str.replace("$val", QString::fromStdString(ossia::convert<std::string>(v)));
      break;
    default:
      str.replace("$val", QString::fromStdString(ossia::value_to_pretty_string(v)));
      break;
  }
}
}

WebSerialProtocol::WebSerialProtocol(
    const QByteArray& code, std::string portId, int baudRate)
    : protocol_base{flags{}}
    , m_code{code}
    , m_portId{std::move(portId)}
    , m_baudRate{baudRate}
{
  m_timer = new QTimer{this};
  m_timer->setInterval(poll_interval);
  QObject::connect(m_timer, &QTimer::timeout, this, [this] { poll(); });
}

WebSerialProtocol::~WebSerialProtocol()
{
  stop();
}

void WebSerialProtocol::set_device(ossia::net::device_base& dev)
{
  m_device = &dev;
  ossia::qt::run_async(this, [this] { startEngine(); });
}

void WebSerialProtocol::startEngine()
{
  if(!m_device)
    return;

  if(m_code.isEmpty())
  {
    makeDefaultTree();
    openPort();
    return;
  }

  m_engine = new QQmlEngine{this};
  m_component = new QQmlComponent{m_engine, this};
  QObject::connect(
      m_component, &QQmlComponent::statusChanged, this,
      &WebSerialProtocol::onComponentStatus);
  m_component->setData(m_code, QUrl{});
}

void WebSerialProtocol::onComponentStatus(QQmlComponent::Status status)
{
  switch(status)
  {
    case QQmlComponent::Status::Loading:
      return;
    case QQmlComponent::Status::Null:
    case QQmlComponent::Status::Error:
      qDebug() << "[WebSerial] QML error:" << m_component->errorString();
      makeDefaultTree();
      openPort();
      return;
    case QQmlComponent::Status::Ready:
      break;
  }

  m_object = m_component->create();
  if(!m_object)
  {
    makeDefaultTree();
    openPort();
    return;
  }
  m_object->setParent(m_engine->rootContext());

  QVariant createTree_ret;
  QMetaObject::invokeMethod(m_object, "createTree", Q_RETURN_ARG(QVariant, createTree_ret));

  auto nodes = ossia::qt::create_device_nodes_deferred<WebSerialProtocol>(
      createTree_ret.value<QJSValue>());

  m_jsObj = m_engine->newQObject(m_object);
  m_onTextMessage = m_jsObj.property("onMessage");
  m_onBinaryMessage = m_jsObj.property("onBinary");
  m_onRead = m_jsObj.property("onRead");

  const auto delim = m_jsObj.property("delimiter").toString();
  const auto frm = m_jsObj.property("framing").toString().toLower();
  if(frm == "none")
  {
    m_lineFraming = false;
  }
  else if(frm == "delimiter" || frm.isEmpty())
  {
    m_lineFraming = true;
    if(!delim.isEmpty())
      m_delimiter = delim.toUtf8();
  }
  else
  {
    qDebug() << "[WebSerial] framing" << frm
             << "is not implemented on WebAssembly, using the delimiter framing";
    m_lineFraming = true;
    if(!delim.isEmpty())
      m_delimiter = delim.toUtf8();
  }

  if(m_onRead.isCallable())
    m_lineFraming = false;

  if(nodes.children.empty())
    makeDefaultTree();
  else
    ossia::qt::apply_deferred_device<
        ossia::net::device_base, ossia::net::serial_node,
        ossia::net::serial_parameter_data>(*m_device, nodes);

  openPort();
}

void WebSerialProtocol::makeDefaultTree()
{
  using data_t = ossia::net::serial_parameter_data;
  ossia::qt::deferred_js_node<data_t> root;

  data_t in{std::string("in")};
  in.type = ossia::val_type::STRING;
  in.value = std::string{};
  in.access = ossia::access_mode::GET;
  root.children.push_back({std::move(in), {}});

  data_t out{std::string("out")};
  out.type = ossia::val_type::STRING;
  out.value = std::string{};
  out.access = ossia::access_mode::SET;
  root.children.push_back({std::move(out), {}});

  ossia::qt::apply_deferred_device<
      ossia::net::device_base, ossia::net::serial_node, data_t>(*m_device, root);
}

void WebSerialProtocol::openPort()
{
  if(!WebSerial::available())
  {
    qDebug() << "[WebSerial] navigator.serial is not available in this browser";
    return;
  }

  m_handle = WebSerial::open(m_portId, m_baudRate);
  if(m_handle <= 0)
  {
    qDebug() << "[WebSerial] no granted port matching"
             << QString::fromStdString(m_portId);
    return;
  }
  m_timer->start();
}

void WebSerialProtocol::poll()
{
  const int st = WebSerial::status(m_handle);
  if(st < 0)
  {
    if(!m_reportedError)
    {
      m_reportedError = true;
      qDebug() << "[WebSerial] port error:"
               << QString::fromStdString(WebSerial::error(m_handle));
    }
    m_timer->stop();
    return;
  }
  if(st == 0)
    return;

  m_opened = true;

  char buf[read_chunk];
  for(;;)
  {
    const int n = WebSerial::read(m_handle, buf, read_chunk);
    if(n <= 0)
      break;

    if(!m_lineFraming)
    {
      onFrame(QByteArray{buf, n});
      continue;
    }

    m_pending.append(buf, n);
    for(;;)
    {
      const int idx = m_pending.indexOf(m_delimiter);
      if(idx < 0)
        break;
      onFrame(m_pending.left(idx));
      m_pending.remove(0, idx + m_delimiter.size());
    }
  }
}

void WebSerialProtocol::onFrame(const QByteArray& frame)
{
  if(frame.isEmpty())
    return;

  QJSValue arr;
  if(m_onTextMessage.isCallable())
  {
    arr = m_onTextMessage.callWithInstance(
        m_jsObj, {m_engine->toScriptValue(QString::fromLatin1(frame))});
  }
  else if(m_onBinaryMessage.isCallable())
  {
    arr = m_onBinaryMessage.callWithInstance(m_jsObj, {m_engine->toScriptValue(frame)});
  }
  else if(m_onRead.isCallable())
  {
    arr = m_onRead.callWithInstance(m_jsObj, {m_engine->toScriptValue(frame)});
  }
  else
  {
    auto n = ossia::net::find_node(m_device->get_root_node(), "/in");
    if(!n)
      return;
    if(auto param = n->get_parameter())
      param->set_value(frame.toStdString());
    return;
  }

  if(!arr.isArray())
    return;

  QJSValueIterator it(arr);
  while(it.hasNext())
  {
    it.next();
    auto val = it.value();
    auto addr = val.property("address");
    if(!addr.isString())
      continue;

    auto n = ossia::net::find_node(m_device->get_root_node(), addr.toString().toStdString());
    if(!n)
      continue;

    auto v = val.property("value");
    if(v.isNull())
      continue;

    if(auto param = n->get_parameter())
      param->set_value(ossia::qt::value_from_js(param->value(), v));
  }
}

bool WebSerialProtocol::push(
    const ossia::net::parameter_base& addr, const ossia::value& v)
{
  ossia::qt::run_async(this, [this, &addr, v] { doWrite(addr, v); });
  return false;
}

void WebSerialProtocol::doWrite(
    const ossia::net::parameter_base& addr, const ossia::value& v)
{
  if(m_handle <= 0)
    return;

  auto& ad = const_cast<ossia::net::serial_parameter&>(
      static_cast<const ossia::net::serial_parameter&>(addr));
  QJSValue& req = ad.data().request;

  if(req.isCallable() && m_engine)
  {
    auto r = req.call({ossia::qt::value_to_js_value(v, *m_engine)});
    if(!r.isError())
    {
      auto var = r.toVariant();
      if(var.typeId() == QMetaType::QByteArray)
        writeBytes(var.toByteArray());
      else
        writeBytes(var.toString().toUtf8());
      return;
    }
    return;
  }

  if(req.isString())
  {
    QString str = req.toString();
    performReplacements(ad, v, str);
    writeBytes(str.toUtf8());
    return;
  }

  writeBytes(valueToBytes(v));
}

void WebSerialProtocol::writeBytes(const QByteArray& b)
{
  if(b.isEmpty() || m_handle <= 0)
    return;

  if(m_lineFraming)
  {
    QByteArray framed = b;
    framed += m_delimiter;
    WebSerial::write(m_handle, framed.data(), framed.size());
  }
  else
  {
    WebSerial::write(m_handle, b.data(), b.size());
  }
}

void WebSerialProtocol::stop()
{
  if(m_timer)
    m_timer->stop();
  if(m_handle > 0)
  {
    WebSerial::close(m_handle);
    m_handle = 0;
  }
}

bool WebSerialProtocol::pull(ossia::net::parameter_base&)
{
  return false;
}

bool WebSerialProtocol::push_raw(const ossia::net::full_parameter_data&)
{
  return false;
}

bool WebSerialProtocol::observe(ossia::net::parameter_base&, bool)
{
  return false;
}

bool WebSerialProtocol::update(ossia::net::node_base&)
{
  return true;
}
}
#endif
