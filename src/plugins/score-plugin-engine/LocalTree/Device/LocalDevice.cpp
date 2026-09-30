// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LocalDevice.hpp"

#include <Explorer/DeviceLogging.hpp>

#include <LocalTree/Device/LocalSpecificSettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/widgets/Pixmap.hpp>
#if defined(OSSIA_PROTOCOL_OSCQUERY)
#include <ossia/network/oscquery/oscquery_server.hpp>
#endif
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/local/local.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>

#include <ossia-qt/invoke.hpp>

#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QTimer>

#include <ossia-config.hpp>

#include <vector>

namespace Protocols
{
namespace
{
//! One per open document, all wanting the same ports: only the first one
//! listens on them.
std::vector<const LocalDevice*>& localDevices()
{
  static std::vector<const LocalDevice*> devices;
  return devices;
}
}

LocalDevice::LocalDevice(
    ossia::net::device_base& dev, const score::DocumentContext& ctx,
    const Device::DeviceSettings& settings)
    : DeviceInterface{settings}
    , m_ctx{ctx}
    , m_dev{dev}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canRemoveNode = false;
  m_capas.canSerialize = false;

  auto& proto = dynamic_cast<ossia::net::multiplex_protocol&>(dev.get_protocol());

  m_proto = &proto;
  setLogging_impl(Device::get_cur_logging(isLogging()));
  // FIXME instead make the logging a property and bind to it.

  enableCallbacks();

  localDevices().push_back(this);
}

LocalDevice::~LocalDevice()
{
  ossia::remove_one(localDevices(), this);
}

bool LocalDevice::listensOn(int oscPort, int wsPort) const noexcept
{
#if defined(OSSIA_PROTOCOL_OSCQUERY) && !defined(__EMSCRIPTEN__)
  return m_oscqProto
         && (m_oscqProto->get_osc_port() == oscPort
             || m_oscqProto->get_ws_port() == wsPort);
#else
  return false;
#endif
}

static bool heldByAnotherDocument(const LocalDevice& self, int oscPort, int wsPort)
{
  return ossia::any_of(localDevices(), [&](const LocalDevice* dev) {
    return dev != &self && dev->listensOn(oscPort, wsPort);
  });
}

static void
exposeZeroconf(std::string name, LocalSpecificSettings set, QPointer<LocalDevice> self)
{
  ossia::net::zeroconf_server ws;
  ossia::net::zeroconf_server osc;
  // Port 0 lets the system pick one, which nothing can be told about.
  if(set.wsPort == 0 || set.oscPort == 0)
    return;
  try
  {
    ws = ossia::net::make_zeroconf_server(name, "_oscjson._tcp", "", set.wsPort, 0);
  }
  catch(const std::exception& e)
  {
    ossia::logger().error("LocalDevice::createZeroconf: {}", e.what());
  }
  catch(...)
  {
    ossia::logger().error("LocalDevice::createZeroconf: error.");
  }

  try
  {
    osc = ossia::net::make_zeroconf_server(name, "_osc._udp", "", set.oscPort, 0);
  }
  catch(const std::exception& e)
  {
    ossia::logger().error("LocalDevice::createZeroconf: {}", e.what());
  }
  catch(...)
  {
    ossia::logger().error("LocalDevice::createZeroconf: error.");
  }

  if(!self)
    return;

  ossia::qt::run_async(
      QCoreApplication::instance(),
      [self, ws = std::move(ws), osc = std::move(osc)]() mutable {
    if(!self)
      return;
    if(auto proto = self->oscqProto())
      proto->set_zeroconf_servers(std::move(ws), std::move(osc));
  });
}

//! Shown once the document is built, and without a nested event loop: a modal
//! exec() would run the queued work of this and other documents (command
//! replays of a restore, device reconnections, document switches, closing)
//! underneath it.
static void warnPortsUnavailable(const QString& problem)
{
  const QString text
      = problem + ' '
        + QObject::tr(
            "Choose other ports by editing the \"score\" device in the Device "
            "explorer, or start score with --local-osc-port and --local-ws-port.");
  qWarning().noquote() << text;
  if(score::AppContext().applicationSettings.gui)
    QTimer::singleShot(0, qApp, [text] {
      auto box = new QMessageBox{
          QMessageBox::Warning, QObject::tr("Local device"), text, QMessageBox::Ok,
          QApplication::activeWindow()};
      box->setIconPixmap(score::get_pixmap(QStringLiteral(":/icons/message_warning.png")));
      box->setAttribute(Qt::WA_DeleteOnClose);
      box->setWindowModality(Qt::NonModal);
      box->show();
    });
}

void LocalDevice::init()
{
  m_dev.set_name(m_settings.name.toStdString());

  if(!m_proto)
    return;

  if(qEnvironmentVariableIsSet("SCORE_DISABLE_LOCALTREE"))
    return;

#if defined(OSSIA_PROTOCOL_OSCQUERY) && !defined(__EMSCRIPTEN__)
  m_oscqProto = nullptr;
  m_proto->clear();

  const auto set = m_settings.deviceSpecificSettings.value<LocalSpecificSettings>();
  QString error;
  try
  {
    auto proto = std::make_unique<ossia::oscquery::oscquery_server_protocol>(
        set.oscPort, set.wsPort);
    proto->disable_zeroconf();
    auto& exposed = *proto;
    // Listens on the WebSocket port, and throws when it is taken.
    m_proto->expose_to(std::move(proto));
    m_oscqProto = &exposed;
  }
  catch(const std::exception& e)
  {
    error = QString::fromUtf8(e.what());
  }
  catch(...)
  {
    error = QObject::tr("unknown error");
  }

  if(!m_oscqProto)
  {
    // What failed is listening on the WebSocket port.
    if(heldByAnotherDocument(*this, -1, set.wsPort))
    {
      qDebug() << "Local device: ports" << set.oscPort << set.wsPort
               << "are used by another open document";
      return;
    }
    warnPortsUnavailable(
        QObject::tr("score could not listen on OSC port %1 and WebSocket port %2 (%3): "
                    "it cannot be controlled over OSC or OSCQuery.")
            .arg(set.oscPort)
            .arg(set.wsPort)
            .arg(error));
    return;
  }

  // A taken UDP port does not fail: the OSC server takes the next free one.
  if(const int osc = m_oscqProto->get_osc_port();
     osc != set.oscPort && !heldByAnotherDocument(*this, set.oscPort, -1))
    warnPortsUnavailable(
        QObject::tr("score could not listen on OSC port %1, which another program "
                    "uses, and listens on port %2 instead.")
            .arg(set.oscPort)
            .arg(osc));

  const auto zeroconfName
      = QString("%1 (%2)")
            .arg(m_settings.name, m_ctx.document.metadata().documentName())
            .toStdString();
  if(auto plug = m_ctx.findPlugin<Explorer::DeviceDocumentPlugin>())
  {
    QPointer<LocalDevice> self = this;
    boost::asio::post(plug->networkContext()->context,
        [=, name = zeroconfName] { exposeZeroconf(name, set, self); });
  }
  else
  {
    exposeZeroconf(zeroconfName, set, this);
  }
#endif
}

void LocalDevice::disconnect()
{
  // TODO handle listening ??
  setLogging_impl(Device::get_cur_logging(false));
}

bool LocalDevice::reconnect()
{
  m_callbacks.clear();
  init();
  return connected();
}

Device::Node LocalDevice::refresh()
{
  return simple_refresh();
}
}
