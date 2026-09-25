#include <Device/Node/DeviceNode.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <JS/ConsolePanel.hpp>

#include <score/actions/Action.hpp>
#include <score/actions/ActionManager.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/Bind.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QBuffer>
#include <QDebug>
#include <QJSEngine>
#include <LocalTree/ScriptableProcessComponent.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <QTimer>

#include <RemoteControl/Settings/Model.hpp>
#include <RemoteControl/Websockets/DocumentPlugin.hpp>
#include <RemoteControl/Websockets/Scenario/Scenario.hpp>

namespace RemoteControl::WS
{
using namespace std::literals;
DocumentPlugin::DocumentPlugin(const score::DocumentContext& doc, QObject* parent)
    : score::DocumentPlugin{doc, "RemoteControl::WS::DocumentPlugin", parent}
    , receiver{doc, 10212}
{
  auto& set = m_context.app.settings<Settings::Model>();
  if(set.getEnabled())
  {
    create();
  }

  con(
      set, &Settings::Model::EnabledChanged, this,
      [this](bool b) {
    if(b)
      create();
    else
      cleanup();
      },
      Qt::QueuedConnection);

  // TODO put this as a setting instead
  startTimer(100);
}

DocumentPlugin::~DocumentPlugin() { }

void DocumentPlugin::timerEvent(QTimerEvent* event)
{
  if(receiver.clients().size() == 0)
    return;

  JSONReader r;
  r.stream.StartObject();

  r.stream.Key("Intervals");
  r.stream.StartArray();
  for(auto& it : this->m_intervals)
  {
    if(*it.second.progress > 0.)
    {
      r.stream.StartObject();

      r.obj[score::StringConstant().Path] = it.second.p;

      r.stream.Key("Progress");
      r.stream.Double(*it.second.progress);

      r.stream.Key("Speed");
      r.stream.Double(it.second.model->duration.speed());

      r.stream.Key("Gain");
      r.stream.Double(it.second.model->outlet->gain());

      r.stream.EndObject();
    }
  }
  r.stream.EndArray();
  r.stream.EndObject();

  receiver.sendMessage(r.toString());
}

void DocumentPlugin::registerInterval(Scenario::IntervalModel& m)
{
  m_intervals[m.id().val()] = IntervalData{&m, &m.duration.playPercentage(), m};
}

void DocumentPlugin::unregisterInterval(Scenario::IntervalModel& m)
{
  m_intervals.erase(m.id().val());
}

void DocumentPlugin::on_documentClosing()
{
  // Do not notify clients of the nodes removed while the document closes
  receiver.detachLocal();
  cleanup();
}

void DocumentPlugin::create()
{
  if(m_root)
    cleanup();

  auto& doc = m_context.document.model().modelDelegate();
  auto scenar = safe_cast<Scenario::ScenarioDocumentModel*>(&doc);
  auto& cstr = scenar->baseScenario().interval();
  m_root = new Interval(cstr, *this, this);
  cstr.components().add(m_root);
}

void DocumentPlugin::cleanup()
{
  if(!m_root)
    return;

  // Delete
  auto& doc = m_context.document.model().modelDelegate();
  auto scenar = safe_cast<Scenario::ScenarioDocumentModel*>(&doc);
  auto& cstr = scenar->baseScenario().interval();

  cstr.components().remove(m_root);
  m_root = nullptr;
}

template <typename T>
static Path<T> readPathFromValue(const rapidjson::Value& val)
{
  if(val.IsString())
  {
    auto str = QString::fromUtf8(val.GetString(), val.GetStringLength());

    auto path = ObjectPath::fromString(str);
    if(path.vec().empty())
      return {};

    return Path<T>(std::move(path), typename Path<T>::UnsafeDynamicCreation{});
  }
  else if(val.IsArray())
  {
    return score::unmarshall<Path<T>>(val);
  }
  else
  {
    return {};
  }
}

namespace
{
bool isScriptable(const ::State::Address& addr)
{
  if(addr.path.empty())
    return false;
  const auto& root = addr.path[0];
  return root == "controls" || root == "triggers" || root == "conditions";
}
}

Receiver::Receiver(const score::DocumentContext& doc, quint16 port)
    : m_server{"i-score-ctrl", QWebSocketServer::NonSecureMode}
    , m_dev{doc.plugin<Explorer::DeviceDocumentPlugin>()}
{
  if(m_server.listen(QHostAddress::Any, port))
  {
    connect(
        &m_server, &QWebSocketServer::newConnection, this, &Receiver::onNewConnection);
  }
  else
  {
    qWarning() << "Remote control: cannot listen on port" << port << ":"
               << m_server.errorString();
  }

  // The local device can be set after construction, or replaced
  auto& list = m_dev.list();
  attachLocal(list.localDevice());
  connect(&list, &Device::DeviceList::deviceAdded, this, [this](Device::DeviceInterface* dev) {
    if(dev && dev == m_dev.list().localDevice() && dev != m_localInterface)
    {
      attachLocal(dev);
      scheduleScriptableMessage();
    }
  });
  connect(
      &list, &Device::DeviceList::deviceRemoved, this, [this](Device::DeviceInterface* dev) {
    if(dev && dev == m_localInterface)
      detachLocal();
  });

  m_answers.insert(
      std::make_pair("Trigger", [&](const rapidjson::Value& obj, const WSClient&) {
        auto it = obj.FindMember("Path");
        if(it == obj.MemberEnd())
          return;

        auto path = readPathFromValue<Scenario::TimeSyncModel>(it->value);
        if(!path.valid())
          return;

        if(Scenario::TimeSyncModel* tn = path.try_find(doc))
          tn->triggeredByGui();
        else
          qDebug() << "warning: tried to trigger a non-existing trigger";
      }));

  m_answers.insert(
      std::make_pair("Message", [this](const rapidjson::Value& obj, const WSClient&) {
        // The message is stored at the "root" level of the json.
        auto it = obj.FindMember(score::StringConstant().Address);
        if(it == obj.MemberEnd())
          return;

        auto message = score::unmarshall<::State::Message>(obj);
        m_dev.updateProxy.updateRemoteValue(message.address.address, message.value);
      }));

  m_answers.insert(std::make_pair("Play", [&](const rapidjson::Value&, const WSClient&) {
    doc.app.actions.action<Actions::Play>().action()->trigger();
  }));
  m_answers.insert(
      std::make_pair("Pause", [&](const rapidjson::Value&, const WSClient&) {
        doc.app.actions.action<Actions::Play>().action()->trigger();
      }));
  m_answers.insert(std::make_pair("Stop", [&](const rapidjson::Value&, const WSClient&) {
    doc.app.actions.action<Actions::Stop>().action()->trigger();
  }));
  m_answers.insert(
      std::make_pair("Transport", [&](const rapidjson::Value& v, const WSClient&) {
        if(v.IsObject())
        {
          if(auto it = v.FindMember("Milliseconds"); it != v.MemberEnd())
          {
            if(it->value.IsNumber())
            {
              double ms = it->value.GetDouble();

              auto& ctrl
                  = doc.app.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
              ctrl.execution().playAtDate(TimeVal::fromMsecs(ms));
            }
          }
        }
      }));

  m_answers.insert(
      std::make_pair("Console", [&](const rapidjson::Value& obj, const WSClient&) {
        auto it = obj.FindMember("Code");
        if(it == obj.MemberEnd())
          return;
        const auto& str = JsonValue{it->value}.toString();
        auto& console = doc.app.panel<JS::PanelDelegate>();
        console.engine().evaluate(str);
      }));

  m_answers.insert(std::make_pair(
      "EnableListening", [&](const rapidjson::Value& obj, const WSClient& c) {
        auto it = obj.FindMember(score::StringConstant().Address);
        if(it == obj.MemberEnd())
          return;

        auto addr = score::unmarshall<::State::Address>(it->value);
        auto d = m_dev.list().findDevice(addr.device);
        if(d)
        {
          d->valueUpdated.connect<&Receiver::on_valueUpdated>(*this);
          d->setListening(addr, true);

          m_listenedAddresses.insert(std::make_pair(addr, c));
        }
      }));

  m_answers.insert(std::make_pair(
      "DisableListening", [&](const rapidjson::Value& obj, const WSClient&) {
        auto it = obj.FindMember(score::StringConstant().Address);
        if(it == obj.MemberEnd())
          return;

        auto addr = score::unmarshall<::State::Address>(it->value);
        auto d = m_dev.list().findDevice(addr.device);
        if(d)
        {
          d->valueUpdated.disconnect<&Receiver::on_valueUpdated>(*this);
          d->setListening(addr, false);
          m_listenedAddresses.erase(addr);
        }
      }));
}

void Receiver::attachLocal(Device::DeviceInterface* local)
{
  detachLocal();
  if(!local)
    return;
  m_localInterface = local;

  auto changed = [this](const ::State::Address& addr) {
    if(isScriptable(addr))
      scheduleScriptableMessage();
  };
  m_localConnections.push_back(
      connect(local, &Device::DeviceInterface::pathAdded, this, changed));
  m_localConnections.push_back(
      connect(local, &Device::DeviceInterface::pathRemoved, this, changed));

  if(auto dev = local->getDevice())
  {
    m_local = dev;
    dev->on_node_renamed.connect<&Receiver::onLocalRenamed>(this);
    dev->on_node_removing.connect<&Receiver::onLocalRemoving>(this);
    dev->on_attribute_modified.connect<&Receiver::onLocalAttribute>(this);
  }
}

void Receiver::detachLocal()
{
  for(auto& c : m_localConnections)
    QObject::disconnect(c);
  m_localConnections.clear();
  if(m_local)
  {
    m_local->on_node_renamed.disconnect<&Receiver::onLocalRenamed>(this);
    m_local->on_node_removing.disconnect<&Receiver::onLocalRemoving>(this);
    m_local->on_attribute_modified.disconnect<&Receiver::onLocalAttribute>(this);
  }
  m_local = nullptr;
  m_localInterface = nullptr;
}

Receiver::~Receiver()
{
  detachLocal();
  m_server.close();
  for(auto c : m_clients)
    delete c.socket;
}

void Receiver::addHandler(QObject* context, Handler&& handler)
{
  if(handler.onAdded)
  {
    handler.onAdded(m_clients);
  }

  m_handlers.emplace_back(context, std::move(handler));
}

void Receiver::removeHandler(QObject* context)
{
  for(auto& [c, h] : m_handlers)
  {
    if(c == context)
      if(h.onRemoved)
        h.onRemoved(m_clients);
  }

  ossia::remove_erase_if(
      m_handlers, [context](const auto& p) { return p.first == context; });
}

void Receiver::registerSync(Path<Scenario::TimeSyncModel> tn)
{
  if(ossia::find(m_activeSyncs, tn) != m_activeSyncs.end())
    return;

  m_activeSyncs.push_back(tn);

  JSONReader r;
  r.stream.StartObject();
  r.obj[score::StringConstant().Message] = "TriggerAdded"sv;
  r.obj[score::StringConstant().Path] = tn;
  r.obj[score::StringConstant().Name] = tn.find(m_dev.context()).metadata().getName();
  r.stream.EndObject();
  const auto& json = r.toString();
  for(auto client : m_clients)
  {
    client.socket->sendTextMessage(json);
  }
}

void Receiver::unregisterSync(Path<Scenario::TimeSyncModel> tn)
{
  if(ossia::find(m_activeSyncs, tn) == m_activeSyncs.end())
    return;

  m_activeSyncs.remove(tn);

  JSONReader r;
  r.stream.StartObject();
  r.obj[score::StringConstant().Message] = "TriggerRemoved"sv;
  r.obj[score::StringConstant().Path] = tn;
  r.stream.EndObject();
  const auto& json = r.toString();
  for(auto client : m_clients)
  {
    client.socket->sendTextMessage(json);
  }
}


namespace
{
bool underScriptableRoots(const ossia::net::node_base& node)
{
  const ossia::net::node_base* n = &node;
  while(n->get_parent() && n->get_parent()->get_parent())
    n = n->get_parent();
  if(!n->get_parent())
    return false;
  const auto& name = n->get_name();
  return name == "controls" || name == "triggers" || name == "conditions";
}

QString removedMessage(const ossia::net::node_base& node)
{
  JSONReader r;
  r.stream.StartObject();
  r.obj[score::StringConstant().Message] = "ScriptableRemoved"sv;
  r.obj[score::StringConstant().Address] = LocalTree::addressOfNode(node).toString();
  r.stream.EndObject();
  return r.toString();
}
}

void Receiver::onLocalRenamed(ossia::net::node_base& node, std::string old)
{
  if(m_clients.empty() || ossia::net::get_zombie(node) || !underScriptableRoots(node))
    return;
  auto now = LocalTree::addressOfNode(node);
  auto before = now;
  before.path.last() = QString::fromStdString(old);

  JSONReader r;
  r.stream.StartObject();
  r.obj[score::StringConstant().Message] = "ScriptableRenamed"sv;
  r.obj["Old"] = before.toString();
  r.obj["New"] = now.toString();
  r.stream.EndObject();
  sendMessage(r.toString());
}

void Receiver::onLocalRemoving(ossia::net::node_base& node)
{
  // Zombie nodes were announced as removed when retired
  if(m_clients.empty() || ossia::net::get_zombie(node) || !underScriptableRoots(node))
    return;
  sendMessage(removedMessage(node));
}

void Receiver::onLocalAttribute(ossia::net::node_base& node, const std::string& key)
{
  if(m_clients.empty() || key != ossia::net::text_zombie() || !underScriptableRoots(node))
    return;
  if(ossia::net::get_zombie(node))
    sendMessage(removedMessage(node));
  else
    // Shown again, e.g. by undo
    scheduleScriptableMessage();
}

namespace
{
//! Removes the nodes kept hidden in the device
void removeHidden(Device::Node& node, ossia::net::node_base& real)
{
  for(auto it = node.begin(); it != node.end();)
  {
    auto child = real.find_child(it->displayName().toStdString());
    if(!child || ossia::net::get_zombie(*child))
    {
      it = node.erase(it);
      continue;
    }
    removeHidden(*it, *child);
    ++it;
  }
}
}

QString Receiver::scriptableMessage() const
{
  auto local = m_dev.list().localDevice();
  if(!local)
    return {};
  auto dev = local->getDevice();

  JSONReader r;
  r.stream.StartObject();
  r.obj[score::StringConstant().Message] = "Scriptable"sv;
  for(const char* root : {"controls", "triggers", "conditions"})
  {
    auto node = local->getNode(::State::Address{local->name(), {QString::fromUtf8(root)}});
    if(dev)
      if(auto real = dev->get_root_node().find_child(std::string_view{root}))
        removeHidden(node, *real);
    r.obj[std::string_view{root}] = node;
  }
  r.stream.EndObject();
  return r.toString();
}

void Receiver::scheduleScriptableMessage()
{
  // Coalesce: publishing one process changes several nodes in a row
  if(m_clients.empty() || m_scriptableScheduled)
    return;
  m_scriptableScheduled = true;
  QTimer::singleShot(0, this, [this] {
    m_scriptableScheduled = false;
    if(auto msg = scriptableMessage(); !msg.isEmpty())
      sendMessage(msg);
  });
}

void Receiver::onNewConnection()
{
  WSClient client{m_server.nextPendingConnection()};

  connect(
      client.socket, &QWebSocket::textMessageReceived, this,
      [this, client](const auto& b) { this->processTextMessage(b, client); });
  connect(
      client.socket, &QWebSocket::binaryMessageReceived, this,
      [this, client](const auto& b) { this->processBinaryMessage(b, client); });
  connect(client.socket, &QWebSocket::disconnected, this, &Receiver::socketDisconnected);

  {
    JSONReader r;
    r.stream.StartObject();
    r.obj[score::StringConstant().Message] = "DeviceTree"sv;
    r.obj["Nodes"] = m_dev.rootNode();
    r.stream.EndObject();

    client.socket->sendTextMessage(r.toString());
  }

  if(auto msg = scriptableMessage(); !msg.isEmpty())
    client.socket->sendTextMessage(msg);

  {
    for(auto path : m_activeSyncs)
    {
      JSONReader r;
      r.stream.StartObject();
      r.obj[score::StringConstant().Message] = "TriggerAdded"sv;
      r.obj[score::StringConstant().Path] = path;
      r.obj[score::StringConstant().Name]
          = path.find(m_dev.context()).metadata().getName();
      r.stream.EndObject();

      client.socket->sendTextMessage(r.toString());
    }
  }

  for(auto& [c, h] : m_handlers)
  {
    if(h.onClientConnection)
      h.onClientConnection(client);
  }

  m_clients.push_back(client);
}

void Receiver::processTextMessage(const QString& message, const WSClient& w)
{
  processBinaryMessage(message.toLatin1(), w);
}

void Receiver::processBinaryMessage(QByteArray message, const WSClient& w)
{
  auto doc = readJson(message);
  JSONWriter wr{doc};

  if(doc.HasParseError())
  {
    return;
  }

  auto it = wr.base.FindMember(score::StringConstant().Message);
  if(it == wr.base.MemberEnd())
    return;

  auto mess = JsonValue{it->value}.toString();

  if(auto it = m_answers.find(mess); it != m_answers.end())
  {
    it->second(wr.base, w);
  }

  for(auto& [c, h] : m_handlers)
  {
    if(auto it = h.answers.find(mess); it != h.answers.end())
    {
      it->second(wr.base, w);
    }
  }
}

void Receiver::sendMessage(const QString& str)
{
  for(auto& clt : m_clients)
  {
    clt.socket->sendTextMessage(str);
  }
}

void Receiver::socketDisconnected()
{
  QWebSocket* pClient = qobject_cast<QWebSocket*>(sender());

  if(pClient)
  {
    WSClient clt{pClient};

    for(auto& [c, h] : m_handlers)
    {
      if(h.onClientDisconnection)
        h.onClientDisconnection(clt);
    }

    {
      auto it = ossia::find_if(m_listenedAddresses, [=](const auto& pair) {
        if(pair.second.socket == pClient)
          return true;
        return false;
      });
      if(it != m_listenedAddresses.end())
        m_listenedAddresses.erase(it);
    }

    ossia::remove_erase(m_clients, clt);
    pClient->deleteLater();
  }
}

void Receiver::on_valueUpdated(const ::State::Address& addr, const ossia::value& v)
{
  auto it = m_listenedAddresses.find(addr);
  if(it != m_listenedAddresses.end())
  {
    ::State::Message m{::State::AddressAccessor{addr}, v};

    JSONObject::Serializer s;
    s.readFrom(m);
    s.obj[score::StringConstant().Message] = score::StringConstant().Message;
    QWebSocket* w = it->second.socket;
    w->sendTextMessage(s.toString());
  }
}

}
