#include "BitfocusContext.hpp"

#include <boost/asio/ip/udp.hpp>

#include <oscpack/osc/OscOutboundPacketStream.h>

#include <wobjectdefs.h>

W_OBJECT_IMPL(bitfocus::module_handler)
namespace bitfocus
{
module_handler_base::~module_handler_base() = default;
module_handler::~module_handler() = default;

module_handler::module_handler(QString path)
    : module_handler_base{path}
{
}

QString module_handler::toPayload(QJsonObject obj)
{
  return QJsonDocument{obj}.toJson(QJsonDocument::Compact);
}

void module_handler::processMessage(std::string_view v)
{
  auto doc = QJsonDocument::fromJson(QByteArray::fromRawData(v.data(), v.size()));
  auto id = doc.object()["callbackId"];
  auto direction = doc.object()["direction"];
  auto pay = doc.object()["payload"];
  auto name = doc.object()["name"];
  auto success = doc.object()["success"];

  auto payload_json = QJsonDocument::fromJson(pay.toString().toUtf8());
  auto pretty = [&] { qDebug() << payload_json.toJson().toStdString().data(); };

  qDebug() << id.toString() << name.toString() << direction.toString();
  qDebug() << payload_json.toJson().toStdString().data();
  if(direction == "call")
  {
    if(name == "register")
    {
      // First message
      on_register();

      QMetaObject::invokeMethod(this, [this] { init_msg_id = init(); });
    }
    else if(name == "upgradedItems")
      QMetaObject::invokeMethod(this, [this, n = id.toInt()] { send_success(n); });
    else if(name == "setActionDefinitions")
      on_setActionDefinitions(payload_json["actions"].toArray());
    else if(name == "setFeedbackDefinitions")
      on_setFeedbackDefinitions(payload_json["feedbacks"].toArray());
    else if(name == "setVariableDefinitions")
      on_setVariableDefinitions(payload_json["variables"].toArray());
    else if(name == "setPresetDefinitions")
      on_setPresetDefinitions(payload_json["presets"].toArray());
    else if(name == "setVariableValues")
      on_setVariableValues(payload_json["newValues"].toArray());
    else if(name == "log-message")
    {
      // qDebug() << " !! Unhandled !! " << name;
      // pretty();
    }
    else if(name == "set-status")
    {
      // on_set_status(payload_json.object());
    }
    else if(name == "updateFeedbackValues")
      on_updateFeedbackValues(payload_json.object());
    else if(name == "saveConfig")
      on_saveConfig(payload_json.object());
    else if(name == "send-osc")
      on_send_osc(payload_json.object());
    else if(name == "parseVariablesInString")
    {
      on_parseVariablesInString(id.toInt(), payload_json.object());
    }
    else if(name == "recordAction")
      on_recordAction(payload_json.object());
    else if(name == "setCustomVariable")
      on_setCustomVariable(payload_json.object());
    else if(name == "sharedUdpSocketJoin")
      on_sharedUdpSocketJoin(payload_json.object());
    else if(name == "sharedUdpSocketLeave")
      on_sharedUdpSocketLeave(payload_json.object());
    else if(name == "sharedUdpSocketSend")
      on_sharedUdpSocketSend(payload_json.object());
    else
      qDebug() << "Unhandled: " << name;
  }
  else if(direction == "response")
  {
    if(id == init_msg_id)
    {
      // Update:
      //{
      //    "hasHttpHandler": false,
      //    "hasRecordActionsHandler": false,
      //    "newUpgradeIndex": -1,
      //    "updatedConfig": {
      //        "localport": 35550,
      //        "remotehost": "127.0.0.1",
      //        "remoteport": 35551
      //    }
      //}

      // Query config field
      req_cfg_id = this->requestConfigFields();
    }
    else if(id == req_cfg_id)
    {
      on_response_configFields(payload_json["fields"].toArray());
    }
  }
}

int module_handler::writeRequest(QString name, QString p)
{
  int id = cbid++;
  QJsonObject obj;
  obj["direction"] = "call";
  obj["name"] = name;
  obj["payload"] = p;
  obj["callbackId"] = id;

  auto res = toPayload(obj).toUtf8().append('\n');
  qDebug().noquote().nospace() << "sending: " << res.size();
  do_write(res);
  return id;
}

void module_handler::writeReply(int id, QString p)
{
  QJsonObject obj;
  obj["direction"] = "response";
  obj["payload"] = p;
  obj["callbackId"] = id;
  auto res = toPayload(obj).toUtf8().append('\n');
  qDebug() << "sending: " << res;
  do_write(res);
}

void module_handler::writeReply(int id, QJsonObject p)
{
  return writeReply(id, QJsonDocument(p).toJson(QJsonDocument::Compact));
}

void module_handler::on_register()
{
  // Payload in ejson format.
  std::string_view res
      = R"_({"direction":"response","callbackId":1,"success":true,"payload":"{}"})_"
        "\n";
  qDebug().noquote().nospace() << "sending: " << res.data();
  do_write(res);
  cbid = 1;
}

void module_handler::on_setActionDefinitions(QJsonArray actions)
{
  for(auto act : actions)
  {
    auto obj = act.toObject();
    bitfocus::connection::action_definition def;
    def.hasLearn = obj["hasLearn"].toBool();
    def.name = obj["name"].toString();
    for(auto opt : obj["options"].toArray())
      def.options.push_back(opt.toObject().toVariantMap());

    m_model.actions.emplace(obj["id"].toString(), std::move(def));
  }
}

void module_handler::on_setVariableDefinitions(QJsonArray vars)
{
  for(auto var : vars)
  {
    auto obj = var.toObject();
    bitfocus::connection::variable_definition def;
    def.name = obj["name"].toString();
    m_model.variables[obj["id"].toString()] = std::move(def);
  }
}
void module_handler::on_setFeedbackDefinitions(QJsonArray fbs)
{
  for(auto fb : fbs)
  {
    auto obj = fb.toObject();
    bitfocus::connection::feedback_definition def;
    def.hasLearn = obj["hasLearn"].toBool();
    def.name = obj["name"].toString();
    def.type = obj["type"].toString();
    for(auto opt : obj["options"].toArray())
      def.options.push_back(opt.toObject().toVariantMap());

    m_model.feedbacks.emplace(obj["id"].toString(), std::move(def));
  }
}
void module_handler::on_setPresetDefinitions(QJsonArray presets)
{
  for(auto preset : presets)
  {
    auto obj = preset.toObject();
    bitfocus::connection::preset_definition def;
    def.name = obj["name"].toString();
    def.text = obj["text"].toString();
    def.category = obj["category"].toString();
    def.type = obj["type"].toString();

    for(auto opt : obj["feedbacks"].toArray())
      def.feedbacks.push_back(opt.toObject().toVariantMap());
    for(auto opt : obj["steps"].toArray())
      def.steps.push_back(opt.toObject().toVariantMap());
    m_model.presets.emplace(obj["id"].toString(), std::move(def));
  }
}
void module_handler::on_setVariableValues(QJsonArray vars)
{
  for(auto var : vars)
  {
    auto obj = var.toObject();
    m_model.variables[obj["id"].toString()].value = obj["value"].toVariant();
  }
}
void module_handler::on_response_configFields(QJsonArray fields)
{
  m_model.config_fields.clear();
  for(auto obj : fields)
  {
    auto f = obj.toObject();
    connection::config_field res;
    res.id = f["id"].toString();
    res.label = f["label"].toString();
    res.type = f["type"].toString();
    res.regex = f["regex"].toString();
    res.value = f["value"].toVariant();
    res.default_value = f["default"].toVariant();
    res.width = f["width"].toDouble();
    {
      for(auto choice_obj : f["choices"].toArray())
      {
        connection::config_field::choice c;
        auto choice = choice_obj.toObject();
        c.id = choice["id"].toString();
        c.label = choice["label"].toString();
        if(!c.id.isEmpty())
          res.choices.push_back(std::move(c));
      }
    }
    m_model.config_fields.push_back(std::move(res));
  }

  configurationParsed();
}

void module_handler::on_send_osc(QJsonObject obj)
{
  qDebug() << "TODO send osc" << obj;
  const std::string host = obj["host"].toString().toStdString();
  const int port = obj["port"].toInt();
  const std::string path = obj["path"].toString().toStdString();
  const auto args = obj["args"].toArray();

  char buf[65535];
  oscpack::OutboundPacketStream p{buf, 65535};
  p << oscpack::BeginMessageN(path);
  for(auto arg : args)
  {
    switch(arg.type())
    {
      case QJsonValue::Type::Null:
        // p << oscpack::OscNil();
        break;
      case QJsonValue::Type::Undefined:
        p << oscpack::Infinitum();
        break;
      case QJsonValue::Type::Bool:
        p << arg.toBool();
        break;
      case QJsonValue::Type::Double:
        p << (float)arg.toDouble();
        break;
      case QJsonValue::Type::String:
        p << arg.toString().toStdString();
        break;
      case QJsonValue::Type::Object: {
        auto obj = arg.toObject();
        auto t = obj["type"].toString();
        if(t == "i")
          p << (int)obj["value"].toDouble();
        else if(t == "f")
          p << (float)obj["value"].toDouble();
        else if(t == "d")
          p << obj["value"].toDouble();
        else if(t == "s")
          p << obj["value"].toString().toStdString();
        else if(t == "b")
        {
          auto blob = obj["value"].toString().toStdString();
          p << oscpack::Blob(blob.data(), blob.size());
        }

        //auto v = obj["value"].toString();
        break;
      }
      case QJsonValue::Type::Array:
        // FIXME
        // FIXME UInt8Array ???
        break;
    }
  }
  p << oscpack::EndMessage();

  try
  {
    boost::system::error_code ec;
    boost::asio::ip::udp::socket socket{m_send_service};
    socket.open(boost::asio::ip::udp::v4(), ec);
    if(ec != boost::system::error_code{})
      return;
    socket.set_option(boost::asio::ip::udp::socket::reuse_address(true), ec);
    if(ec != boost::system::error_code{})
      return;
    socket.set_option(boost::asio::socket_base::broadcast(true), ec);
    if(ec != boost::system::error_code{})
      return;
    boost::asio::ip::udp::endpoint endpoint{
        boost::asio::ip::make_address(host, ec), (uint16_t)port};
    if(ec != boost::system::error_code{})
      return;
    socket.send_to(boost::asio::const_buffer(p.Data(), p.Size()), endpoint, 0, ec);
  }
  catch(...)
  {
  }
}

int module_handler::init()
{
  QJsonObject obj;
  obj["label"] = "OSCPoint";
  obj["isFirstInit"] = false;
  obj["config"] = QJsonObject{
      {"remotehost", "127.0.0.1"}, {"remoteport", 35551}, {"localport", 35550}};
  obj["lastUpgradeIndex"] = -1;
  obj["actions"] = QJsonObject{};
  obj["feedbacks"] = QJsonObject{};

  return writeRequest("init", toPayload(obj));
}

void module_handler::send_success(int id)
{
  QJsonObject obj;
  obj["direction"] = "response";
  obj["callbackId"] = id;
  obj["success"] = true;
  auto res = toPayload(obj).toUtf8().append('\n');
  do_write(res);
}

void module_handler::on_set_status(QJsonObject obj) { }
void module_handler::on_saveConfig(QJsonObject obj) { }
void module_handler::on_parseVariablesInString(int id, QJsonObject obj)
{
  QJsonObject p;
  p["text"] = ""; // FIXME
  p["variableIds"] = QJsonArray{};
  writeReply(id, p);
}
void module_handler::on_updateFeedbackValues(QJsonObject obj) { }
void module_handler::on_recordAction(QJsonObject obj) { }
void module_handler::on_setCustomVariable(QJsonObject obj) { }
void module_handler::on_sharedUdpSocketJoin(QJsonObject obj) { }
void module_handler::on_sharedUdpSocketLeave(QJsonObject obj) { }
void module_handler::on_sharedUdpSocketSend(QJsonObject obj) { }
void module_handler::updateConfigAndLabel()
{
  qDebug() << "TODO" << Q_FUNC_INFO;
}

int module_handler::requestConfigFields()
{
  return writeRequest("getConfigFields", toPayload(QJsonObject{}));
}

void module_handler::updateFeedbacks()
{
  qDebug() << "TODO" << Q_FUNC_INFO;
}

void module_handler::feedbackLearnValues()
{
  qDebug() << "TODO" << Q_FUNC_INFO;
}

void module_handler::feedbackDelete()
{
  qDebug() << "TODO" << Q_FUNC_INFO;
}

void module_handler::variablesChanged()
{
  qDebug() << "TODO" << Q_FUNC_INFO;
  // {"direction":"call","name":"variablesChanged","payload":"{\"variablesIds\":[\"internal:time_hms\",\"internal:time_s\",\"internal:time_unix\",\"internal:time_hms_12\",\"internal:uptime\"]}"}
}

void module_handler::actionUpdate()
{
  qDebug() << "TODO" << Q_FUNC_INFO;
}

void module_handler::actionDelete()
{
  qDebug() << "TODO" << Q_FUNC_INFO;
}

void module_handler::actionLearnValues()
{
  qDebug() << "TODO" << Q_FUNC_INFO;
}

void module_handler::actionRun(std::string_view act)
{
  QJsonObject act_object;
  act_object["id"] = QString("foo");
  act_object["controlId"] = QString("bank:0");
  act_object["actionId"] = QString::fromUtf8(act.data(), act.size());
  act_object["options"] = QJsonObject{}; // TODO
  act_object["upgradeIndex"] = QJsonValue{QJsonValue::Type::Null};
  act_object["disabled"] = false;
  QJsonObject root;
  root["action"] = act_object;
  root["surfaceId"] = QString("hot:tablet"); // could be undefined

  writeRequest("executeAction", toPayload(root));
}

void module_handler::destroy()
{
  qDebug() << "TODO" << Q_FUNC_INFO;
}

void module_handler::executeHttpRequest()
{
  qDebug() << "TODO" << Q_FUNC_INFO;
}

void module_handler::startStopRecordingActions()
{
  qDebug() << "TODO" << Q_FUNC_INFO;
}

void module_handler::sharedUdpSocketMessage()
{
  qDebug() << "TODO" << Q_FUNC_INFO;
}

void module_handler::sharedUdpSocketError()
{
  qDebug() << "TODO" << Q_FUNC_INFO;
}

const connection& module_handler::model()
{
  return m_model;
}
}
