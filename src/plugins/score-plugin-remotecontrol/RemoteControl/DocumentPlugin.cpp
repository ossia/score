#include <Device/Node/DeviceNode.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <JS/ConsolePanel.hpp>
#include <Scenario/Application/ScenarioActions.hpp>

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

#include <RemoteControl/DocumentPlugin.hpp>
#include <RemoteControl/Scenario/Scenario.hpp>
#include <RemoteControl/Settings/Model.hpp>
namespace RemoteControl
{
using namespace std::literals;
DocumentPlugin::DocumentPlugin(
    const score::DocumentContext& doc,
    Id<score::DocumentPlugin> id,
    QObject* parent)
    : score::DocumentPlugin{doc, std::move(id), "RemoteControl::DocumentPlugin", parent}
    , receiver{doc, 10212}
{
  auto& set = m_context.app.settings<Settings::Model>();
  if (set.getEnabled())
  {
    create();
  }

  con(
      set,
      &Settings::Model::EnabledChanged,
      this,
      [=](bool b) {
        if (b)
          create();
        else
          cleanup();
      },
      Qt::QueuedConnection);

  con(doc.coarseUpdateTimer, &QTimer::timeout,
      this, &DocumentPlugin::heartbeat);
}

DocumentPlugin::~DocumentPlugin()
{

}

void DocumentPlugin::heartbeat()
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

      r.stream.Key("Path");
      r.stream.String(it.second.path);

      r.stream.Key("Progress");
      r.stream.Double(*it.second.progress);

      r.stream.EndObject();
    }
  }
  r.stream.EndArray();
  r.stream.EndObject();

  const QString str = r.toString();

  for(auto& clt : receiver.clients())
  {
    clt.socket->sendTextMessage(str);
  }
}

void DocumentPlugin::registerInterval(Scenario::IntervalModel& m)
{
  m_intervals[m.id().val()] = IntervalData{
      &m,
      &m.duration.playPercentage(),
      Path<Scenario::IntervalModel>{m}.unsafePath().toString().toStdString()
  };
}

void DocumentPlugin::unregisterInterval(Scenario::IntervalModel& m)
{
  m_intervals.erase(m.id().val());
}

void DocumentPlugin::on_documentClosing()
{
  cleanup();
}

void DocumentPlugin::create()
{
  if (m_root)
    cleanup();

  auto& doc = m_context.document.model().modelDelegate();
  auto scenar = safe_cast<Scenario::ScenarioDocumentModel*>(&doc);
  auto& cstr = scenar->baseScenario().interval();
  m_root = new Interval(getStrongId(cstr.components()), cstr, *this, this);
  cstr.components().add(m_root);
}

void DocumentPlugin::cleanup()
{
  if (!m_root)
    return;

  // Delete
  auto& doc = m_context.document.model().modelDelegate();
  auto scenar = safe_cast<Scenario::ScenarioDocumentModel*>(&doc);
  auto& cstr = scenar->baseScenario().interval();

  cstr.components().remove(m_root);
  m_root = nullptr;
}

Receiver::Receiver(const score::DocumentContext& doc, quint16 port)
    : m_server{"i-score-ctrl", QWebSocketServer::NonSecureMode}
    , m_dev{doc.plugin<Explorer::DeviceDocumentPlugin>()}
{
  if (m_server.listen(QHostAddress::Any, port))
  {
    connect(&m_server, &QWebSocketServer::newConnection, this, &Receiver::onNewConnection);
  }

  m_answers.insert(std::make_pair("Trigger", [&](const rapidjson::Value& obj, const WSClient&) {
    auto it = obj.FindMember("Path");
    if (it == obj.MemberEnd())
      return;

    auto path = score::unmarshall<Path<Scenario::TimeSyncModel>>(it->value);
    if (!path.valid())
      return;

    if(Scenario::TimeSyncModel* tn = path.try_find(doc))
      tn->triggeredByGui();
    else
      qDebug() << "warning: tried to trigger a non-existing trigger";
  }));

  m_answers.insert(std::make_pair("Message", [this](const rapidjson::Value& obj, const WSClient&) {
    // The message is stored at the "root" level of the json.
    auto it = obj.FindMember(score::StringConstant().Address);
    if (it == obj.MemberEnd())
      return;

    auto message = score::unmarshall<::State::Message>(obj);
    m_dev.updateProxy.updateRemoteValue(message.address.address, message.value);
  }));

  m_answers.insert(std::make_pair("Play", [&](const rapidjson::Value&, const WSClient&) {
    doc.app.actions.action<Actions::Play>().action()->trigger();
  }));
  m_answers.insert(std::make_pair("Pause", [&](const rapidjson::Value&, const WSClient&) {
    doc.app.actions.action<Actions::Play>().action()->trigger();
  }));
  m_answers.insert(std::make_pair("Stop", [&](const rapidjson::Value&, const WSClient&) {
    doc.app.actions.action<Actions::Stop>().action()->trigger();
  }));
  m_answers.insert(std::make_pair("Transport", [&](const rapidjson::Value& v, const WSClient&) {
    if(v.IsObject())
    {
      if(auto it = v.FindMember("Milliseconds"); it != v.MemberEnd())
      {
        if(it->value.IsNumber())
        {
           double ms = it->value.GetDouble();

           auto& ctrl = doc.app.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
           ctrl.execution().playAtDate(TimeVal::fromMsecs(ms));
        }
      }
    }
  }));

  m_answers.insert(std::make_pair("Console", [&](const rapidjson::Value& obj, const WSClient&) {
    auto it = obj.FindMember("Code");
    if (it == obj.MemberEnd())
      return;
    const auto& str = JsonValue{it->value}.toString();
    auto& console = doc.app.panel<JS::PanelDelegate>();
    console.engine().evaluate(str);
  }));

  m_answers.insert(
      std::make_pair("EnableListening", [&](const rapidjson::Value& obj, const WSClient& c) {
        auto it = obj.FindMember(score::StringConstant().Address);
        if (it == obj.MemberEnd())
          return;

        auto addr = score::unmarshall<::State::Address>(it->value);
        auto d = m_dev.list().findDevice(addr.device);
        if (d)
        {
          d->valueUpdated.connect<&Receiver::on_valueUpdated>(*this);
          d->setListening(addr, true);

          m_listenedAddresses.insert(std::make_pair(addr, c));
        }
      }));

  m_answers.insert(
      std::make_pair("DisableListening", [&](const rapidjson::Value& obj, const WSClient&) {
        auto it = obj.FindMember(score::StringConstant().Address);
        if (it == obj.MemberEnd())
          return;

        auto addr = score::unmarshall<::State::Address>(it->value);
        auto d = m_dev.list().findDevice(addr.device);
        if (d)
        {
          d->valueUpdated.disconnect<&Receiver::on_valueUpdated>(*this);
          d->setListening(addr, false);
          m_listenedAddresses.erase(addr);
        }
      }));
}

Receiver::~Receiver()
{
  m_server.close();
  for (auto c : m_clients)
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

  ossia::remove_erase_if(m_handlers,
                         [context] (const auto& p) {
    return p.first == context;
  });
}

void Receiver::registerSync(Path<Scenario::TimeSyncModel> tn)
{
  if (ossia::find(m_activeSyncs, tn) != m_activeSyncs.end())
    return;

  m_activeSyncs.push_back(tn);

  JSONReader r;
  r.stream.StartObject();
  r.obj[score::StringConstant().Message] = "TriggerAdded"sv;
  r.obj[score::StringConstant().Path] = tn;
  r.obj[score::StringConstant().Name] = tn.find(m_dev.context()).metadata().getName();
  r.stream.EndObject();
  const auto& json = r.toString();
  for (auto client : m_clients)
  {
    client.socket->sendTextMessage(json);
  }
}

void Receiver::unregisterSync(Path<Scenario::TimeSyncModel> tn)
{
  if (ossia::find(m_activeSyncs, tn) == m_activeSyncs.end())
    return;

  m_activeSyncs.remove(tn);

  JSONReader r;
  r.stream.StartObject();
  r.obj[score::StringConstant().Message] = "TriggerRemoved"sv;
  r.obj[score::StringConstant().Path] = tn;
  r.stream.EndObject();
  const auto& json = r.toString();
  for (auto client : m_clients)
  {
    client.socket->sendTextMessage(json);
  }
}

void Receiver::onNewConnection()
{
  WSClient client{m_server.nextPendingConnection()};

  connect(client.socket, &QWebSocket::textMessageReceived, this, [=](const auto& b) {
    this->processTextMessage(b, client);
  });
  connect(client.socket, &QWebSocket::binaryMessageReceived, this, [=](const auto& b) {
    this->processBinaryMessage(b, client);
  });
  connect(client.socket, &QWebSocket::disconnected, this, &Receiver::socketDisconnected);

  {
    JSONReader r;
    r.stream.StartObject();
    r.obj[score::StringConstant().Message] = "DeviceTree"sv;
    r.obj["Nodes"] = m_dev.rootNode();
    r.stream.EndObject();

    client.socket->sendTextMessage(r.toString());
  }

  {
    for (auto path : m_activeSyncs)
    {
      JSONReader r;
      r.stream.StartObject();
      r.obj[score::StringConstant().Message] = "TriggerAdded"sv;
      r.obj[score::StringConstant().Path] = path;
      r.obj[score::StringConstant().Name] = path.find(m_dev.context()).metadata().getName();
      r.stream.EndObject();

      client.socket->sendTextMessage(r.toString());
    }
  }

  for(auto& [c,h] : m_handlers)
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

  if (doc.HasParseError())
  {
    return;
  }

  auto it = wr.base.FindMember(score::StringConstant().Message);
  if (it == wr.base.MemberEnd())
    return;

  auto mess = JsonValue{it->value}.toString();

  if (auto it = m_answers.find(mess);
      it != m_answers.end())
  {
    it->second(wr.base, w);
  }

  for(auto& [c,h] : m_handlers)
  {
    if (auto it = h.answers.find(mess);
        it != h.answers.end())
    {
      it->second(wr.base, w);
    }
  }
}

void Receiver::socketDisconnected()
{
  QWebSocket* pClient = qobject_cast<QWebSocket*>(sender());

  if (pClient)
  {
    WSClient clt{pClient};

    for(auto& [c,h] : m_handlers)
    {
      if(h.onClientDisconnection)
        h.onClientDisconnection(clt);
    }

    {
      auto it = ossia::find_if(m_listenedAddresses, [=](const auto& pair) {
        if (pair.second.socket == pClient)
          return true;
        return false;
      });
      if (it != m_listenedAddresses.end())
        m_listenedAddresses.erase(it);
    }

    ossia::remove_erase(m_clients, clt);
    pClient->deleteLater();
  }
}

void Receiver::on_valueUpdated(const ::State::Address& addr, const ossia::value& v)
{
  auto it = m_listenedAddresses.find(addr);
  if (it != m_listenedAddresses.end())
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
