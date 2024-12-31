#include "BitfocusContext.hpp"

#include <boost/asio/ip/udp.hpp>

#include <QVersionNumber>

#include <oscpack/osc/OscOutboundPacketStream.h>

#include <wobjectimpl.h>

W_OBJECT_IMPL(bitfocus::module_handler)
namespace bitfocus
{

module_handler::~module_handler() { }

module_handler::module_handler(
    QString path, QString apiversion, module_configuration conf)
    : module_handler_base{path}
{
  for(QChar& c : apiversion)
    if(!c.isDigit() && c != '.')
      c = '0';

  m_expects_label_updates
      = QVersionNumber::fromString(apiversion) >= QVersionNumber(1, 2);

  this->m_model.config = std::move(conf);
  // Init an udp socket for sending osc
  boost::system::error_code ec;
  m_socket.open(boost::asio::ip::udp::v4(), ec);
  if(ec != boost::system::error_code{})
    return;
  m_socket.set_option(boost::asio::ip::udp::socket::reuse_address(true), ec);
  if(ec != boost::system::error_code{})
    return;
  m_socket.set_option(boost::asio::socket_base::broadcast(true), ec);
  if(ec != boost::system::error_code{})
    return;

  /// Connection flow:
  // Create process
  // <- register call
  // -> register response

  // -> init call
  //   <- upgradedItems
  //   <- setActionDefinitions
  //   <- setVariableDefinitions
  //   <- etc.
  // <- init response
}

void module_handler::do_write(QString str)
{
  auto res = str.toUtf8().append('\n');
  module_handler_base::do_write(std::string_view(res.data(), res.size()));
}

QString module_handler::jsonToString(QJsonObject obj)
{
  return QJsonDocument{obj}.toJson(QJsonDocument::Compact);
}

void module_handler::processMessage(std::string_view v)
{
  auto doc = QJsonDocument::fromJson(QByteArray::fromRawData(v.data(), v.size()));

  auto dobj = doc.object();
  QJsonValue id = dobj["callbackId"];
  QJsonValue direction = dobj["direction"];
  QJsonValue pay = dobj["payload"];
  QJsonValue name = dobj["name"];
  QJsonValue success = dobj["success"];

  auto payload_json = QJsonDocument::fromJson(pay.toString().toUtf8());

  auto pretty = [&] {
    //  qDebug() << payload_json.toJson().toStdString().data();
  };

  // qDebug() << id.toString() << name.toString() << direction.toString();
  //qDebug() << payload_json.toJson().toStdString().data();
  if(direction == "call")
  {
    if(name == "register")
    {
      // First message
      on_register(id);

      QMetaObject::invokeMethod(this, [this] { init_msg_id = init(); });
    }
    else if(name == "upgradedItems")
      QMetaObject::invokeMethod(this, [this, id] { send_success(id); });
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
      on_parseVariablesInString(id, payload_json.object());
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
      // Query config field
      req_cfg_id = this->requestConfigFields();
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

  do_write(jsonToString(obj));
  return id;
}

void module_handler::writeReply(QJsonValue id, QString p)
{
  QJsonObject obj;
  obj["direction"] = "response";
  obj["payload"] = p;
  obj["callbackId"] = id;

  do_write(jsonToString(obj));
}

void module_handler::writeReply(QJsonValue id, QJsonObject p)
{
  return writeReply(id, QJsonDocument(p).toJson(QJsonDocument::Compact));
}

void module_handler::on_register(QJsonValue id)
{
  QJsonObject obj;
  obj["direction"] = "response";
  obj["callbackId"] = id;
  obj["success"] = true;
  obj["payload"] = "{}";
  do_write(jsonToString(obj));
}

void module_handler::on_setActionDefinitions(QJsonArray actions)
{
  for(auto act : actions)
  {
    auto obj = act.toObject();
    bitfocus::module_data::action_definition def;
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
    bitfocus::module_data::variable_definition def;
    def.name = obj["name"].toString();
    m_model.variables[obj["id"].toString()] = std::move(def);
  }
}

void module_handler::on_setFeedbackDefinitions(QJsonArray fbs)
{
  for(auto fb : fbs)
  {
    auto obj = fb.toObject();
    bitfocus::module_data::feedback_definition def;
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
    bitfocus::module_data::preset_definition def;
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
  for(const auto& var : vars)
  {
    auto obj = var.toObject();
    const auto& id = obj["id"].toString();
    auto& vv = m_model.variables[id];
    vv.value = obj["value"].toVariant();
    variableChanged(id, vv.value);
  }
}

void module_handler::on_response_configFields(QJsonArray fields)
{
  qDebug() << Q_FUNC_INFO << fields.size();

  m_model.config_fields.clear();
  for(auto obj : fields)
  {
    auto f = obj.toObject();
    module_data::config_field res;
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
        module_data::config_field::choice c;
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
    boost::asio::ip::udp::endpoint endpoint{
        boost::asio::ip::make_address(host, ec), (uint16_t)port};
    if(ec != boost::system::error_code{})
      return;
    m_socket.send_to(boost::asio::const_buffer(p.Data(), p.Size()), endpoint, 0, ec);
  }
  catch(...)
  {
  }
}

int module_handler::init()
{
  QJsonObject obj;
  obj["label"] = "OSCPoint";
  obj["isFirstInit"] = true;
  QJsonObject config;
  for(auto& [k, v] : this->m_model.config)
  {
    config[k] = QJsonValue::fromVariant(v);
  }
  obj["config"] = std::move(config);
  obj["lastUpgradeIndex"] = -1;
  obj["actions"] = QJsonObject{};
  obj["feedbacks"] = QJsonObject{};

  return writeRequest("init", jsonToString(obj));
}

void module_handler::send_success(QJsonValue id)
{
  QJsonObject obj;
  obj["direction"] = "response";
  obj["callbackId"] = id;
  obj["success"] = true;
  do_write(jsonToString(obj));
}

void module_handler::on_set_status(QJsonObject obj) { }
void module_handler::on_saveConfig(QJsonObject obj) { }
void module_handler::on_parseVariablesInString(QJsonValue id, QJsonObject obj)
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
void module_handler::updateConfigAndLabel(QString label, module_configuration conf)
{
  this->m_model.config = std::move(conf);

  QJsonObject config;
  for(auto& [k, v] : this->m_model.config)
  {
    config[k] = QJsonValue::fromVariant(v);
  }

  if(m_expects_label_updates)
  {
    QJsonObject obj;
    obj["config"] = std::move(config);
    obj["label"] = label;
    writeRequest("updateConfigAndLabel", jsonToString(obj));
  }
  else
  {
    writeRequest("updateConfig", jsonToString(config));
  }
}

int module_handler::requestConfigFields()
{
  qDebug() << Q_FUNC_INFO;
  return writeRequest("getConfigFields", jsonToString(QJsonObject{}));
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

  writeRequest("executeAction", jsonToString(root));
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

const module_data& module_handler::model()
{
  return m_model;
}
}
