#include "BitfocusContext.hpp"

#include <boost/asio/ip/udp.hpp>

#include <QHostInfo>
#include <QNetworkDatagram>
#include <QPointer>
#include <QTimer>
#include <QUdpSocket>
#include <QVersionNumber>

#include <oscpack/osc/OscOutboundPacketStream.h>

#include <wobjectimpl.h>

#include <charconv>
#include <cmath>
#include <limits>

W_OBJECT_IMPL(bitfocus::module_handler)
namespace bitfocus
{
static const bool g_trace = qEnvironmentVariableIsSet("SCORE_BITFOCUS_TRACE");

static bool isIntegral(const QVariant& v)
{
  if(!v.isValid() || v.isNull())
    return true;
  bool ok{};
  const double d = v.toDouble(&ok);
  return ok && std::floor(d) == d;
}

bool module_data::config_field::isInteger() const noexcept
{
  auto absent = [](const QVariant& v) { return !v.isValid() || v.isNull(); };
  if(absent(default_value) && absent(min) && absent(max))
    return false;
  return isIntegral(default_value) && isIntegral(min) && isIntegral(max)
         && isIntegral(step);
}

double floatToDouble(float f)
{
  char buf[64];
  auto res = std::to_chars(buf, buf + sizeof(buf), f);
  double d{};
  std::from_chars(buf, res.ptr, d);
  return d;
}

QVariant widenFloat(const QVariant& v)
{
  if(v.typeId() == QMetaType::Float)
    return floatToDouble(v.toFloat());
  if(v.typeId() == QMetaType::QVariantList)
  {
    auto list = v.toList();
    for(auto& e : list)
      e = widenFloat(e);
    return list;
  }
  return v;
}

QJsonValue defaultOptionValue(const module_data::config_field& f)
{
  return f.default_json;
}

static QJsonValue choiceValue(const module_data::config_field& f, const QVariant& v)
{
  const QString str = v.toString();
  for(const auto& c : f.choices)
    if(c.id == str)
      return c.value;
  return QJsonValue::fromVariant(v);
}

static bool isDefaultValue(const module_data::config_field& f, const QVariant& v)
{
  const auto& d = f.default_json;
  if(d.isUndefined())
    return false;
  if(d.isNull())
    return !v.isValid() || v.toString().isEmpty();
  if(v.typeId() == QMetaType::Bool && !d.isString())
    return (d.isBool() ? d.toBool() : d.toDouble() != 0.) == v.toBool();
  if(!d.isArray() && v.typeId() == QMetaType::QVariantList)
  {
    const auto list = v.toList();
    return list.size() == 1 && d.toVariant().toString() == list[0].toString();
  }
  if(d.isArray())
  {
    const auto arr = d.toArray();
    const auto list = v.toList();
    if(arr.size() != list.size())
      return false;
    for(int i = 0; i < arr.size(); i++)
      if(arr[i].toVariant().toString() != list[i].toString())
        return false;
    return true;
  }
  return d.toVariant().toString() == v.toString();
}

QJsonValue toModuleValue(const module_data::config_field& f, const QVariant& v_)
{
  const QVariant v = widenFloat(v_);
  if(f.type == "static-text")
    return QJsonValue::Undefined;
  // Companion sends the default as the module wrote it, whatever its type
  if(isDefaultValue(f, v))
    return f.default_json;
  if(f.type == "number")
  {
    if(f.isInteger())
      return (qint64)std::llround(v.toDouble());
    return v.toDouble();
  }
  if(f.type == "checkbox")
    return v.toBool();
  if(f.type == "dropdown")
    return choiceValue(f, v);
  if(f.type == "multidropdown")
  {
    QJsonArray arr;
    if(v.typeId() == QMetaType::QVariantList || v.typeId() == QMetaType::QStringList)
    {
      for(const auto& e : v.toList())
        arr.append(choiceValue(f, e));
    }
    else if(!v.toString().isEmpty())
    {
      arr.append(choiceValue(f, v));
    }
    return arr;
  }
  if(f.type == "colorpicker")
  {
    if(f.default_json.isString() || f.returnType == "string")
      return v.toString();
    return (qint64)v.toLongLong();
  }

  return v.toString();
}

QJsonValue ejsonDecode(const QJsonValue& v)
{
  if(v.isArray())
  {
    QJsonArray arr = v.toArray();
    for(auto it = arr.begin(); it != arr.end(); ++it)
      *it = ejsonDecode(*it);
    return arr;
  }
  if(!v.isObject())
    return v;

  QJsonObject obj = v.toObject();
  if(obj.size() == 1)
  {
    const auto key = obj.begin().key();
    const auto val = obj.begin().value();
    if(key == "$binary")
      return val;
    if(key == "$date")
      return val.toDouble();
    if(key == "$InfNaN")
    {
      const int s = val.toInt();
      return s == 0 ? std::numeric_limits<double>::quiet_NaN()
                    : s * std::numeric_limits<double>::infinity();
    }
    if(key == "$escape")
      return val;
  }
  for(auto it = obj.begin(); it != obj.end(); ++it)
    *it = ejsonDecode(*it);
  return obj;
}

static QByteArray ejsonBinary(const QJsonValue& v)
{
  if(v.isObject())
  {
    auto obj = v.toObject();
    if(auto bin = obj.find("$binary"); bin != obj.end())
      return QByteArray::fromBase64(bin->toString().toLatin1());
    // Buffer.toJSON()
    if(obj["type"] == QStringLiteral("Buffer"))
    {
      QByteArray res;
      for(auto b : obj["data"].toArray())
        res.push_back((char)b.toInt());
      return res;
    }
  }
  if(v.isArray())
  {
    QByteArray res;
    for(auto b : v.toArray())
      res.push_back((char)b.toInt());
    return res;
  }
  return v.toString().toUtf8();
}

static QJsonObject ejsonBinaryValue(const QByteArray& data)
{
  return QJsonObject{{"$binary", QString::fromLatin1(data.toBase64())}};
}

// Companion shares a UDP port between all the connections which listen on it
struct shared_udp_port : QObject
{
  QUdpSocket socket;
  QString family;
  int port{};
  std::vector<std::pair<QPointer<module_handler>, QString>> members;

  static auto& registry()
  {
    static std::map<std::pair<QString, int>, std::weak_ptr<shared_udp_port>> reg;
    return reg;
  }

  void on_read()
  {
    while(socket.hasPendingDatagrams())
    {
      auto dgram = socket.receiveDatagram();
      auto sender = dgram.senderAddress();
      const bool v6 = sender.protocol() == QAbstractSocket::IPv6Protocol
                      && !sender.toIPv4Address();
      const QString address = v6 ? sender.toString()
                                 : QHostAddress(sender.toIPv4Address()).toString();
      auto members_copy = members;
      for(auto& [h, id] : members_copy)
        if(h)
          h->sharedUdpSocketMessage(
              id, port, dgram.data(), address, dgram.senderPort(), v6);
    }
  }
};

module_handler::~module_handler()
{
  destroy();
  for(auto& [id, port] : m_shared_udp_handles)
    std::erase_if(port->members, [id = id](auto& m) { return m.second == id; });
}

module_handler::module_handler(
    QString path, QString entrypoint, QString nodeVersion, QString apiversion,
    module_configuration conf, QString label, bool firstInit,
    std::optional<int> upgradeIndex)
    : module_handler_base{
        nodeExecutable(nodeVersion), path, entrypoint,
        QUuid::createUuid().toString(QUuid::WithoutBraces)}
    , m_label{label.isEmpty() ? QStringLiteral("connection") : label}
    , m_firstInit{firstInit}
{
  for(QChar& c : apiversion)
    if(!c.isDigit() && c != '.')
      c = '0';

  m_expects_label_updates
      = QVersionNumber::fromString(apiversion) >= QVersionNumber(1, 2);

  this->m_model.config = std::move(conf);
  this->m_model.upgradeIndex = upgradeIndex;
  m_uptime.start();

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

  // -> updateFeedback
  // <- response

  // -> requestConfigFields
  // <- response
}

void module_handler::do_write(QString str)
{
  auto res = str.toUtf8().append('\n');
  if(g_trace)
    fprintf(stderr, "[bitfocus] -> %s", res.constData());
  module_handler_base::do_write(std::string_view(res.data(), res.size()));
}

QString module_handler::jsonToString(QJsonObject obj)
{
  return QJsonDocument{obj}.toJson(QJsonDocument::Compact);
}

void module_handler::afterRegistration(std::function<void()> f)
{
  if(m_registered)
    f();
  else
    m_afterRegistrationQueue.push_back(std::move(f));
}

void module_handler::processMessage(std::string_view v)
{
  if(g_trace)
    fprintf(stderr, "[bitfocus] <- %.*s\n", (int)v.size(), v.data());
  auto doc = QJsonDocument::fromJson(QByteArray::fromRawData(v.data(), v.size()));

  auto dobj = doc.object();
  QJsonValue id = dobj["callbackId"];
  QJsonValue direction = dobj["direction"];
  QJsonValue pay = dobj["payload"];
  QJsonValue name = dobj["name"];
  QJsonValue success = dobj["success"];

  auto payload_json = QJsonDocument::fromJson(pay.toString().toUtf8());

  if(direction == "call")
  {
    // Before 1.5 module-base used string callback ids
    const bool hasCallback = !id.isUndefined() && !id.isNull();
    if(name == "register")
    {
      // First message
      on_register(id);

      // The fields do not wait for init, which may wait for the device
      QMetaObject::invokeMethod(this, [this] {
        m_init_msg_id = init(m_label);
        m_req_cfg_id = requestConfigFields();
      });
    }
    else if(name == "upgradedItems")
      QMetaObject::invokeMethod(this, [this, id] { send_success(id); });
    else if(name == "setActionDefinitions")
      on_setActionDefinitions(payload_json["actions"].toArray());
    else if(name == "setFeedbackDefinitions")
      on_setFeedbackDefinitions(payload_json["feedbacks"].toArray());
    else if(name == "setVariableDefinitions")
      on_setVariableDefinitions(
          payload_json["variables"].toArray(), payload_json["newValues"].toArray());
    else if(name == "setPresetDefinitions")
      on_setPresetDefinitions(payload_json["presets"].toArray());
    else if(name == "setVariableValues")
      on_setVariableValues(payload_json["newValues"].toArray());
    else if(name == "log-message")
      on_log_message(payload_json.object());
    else if(name == "set-status")
      on_set_status(payload_json.object());
    else if(name == "updateFeedbackValues")
      on_updateFeedbackValues(payload_json.object());
    else if(name == "saveConfig")
      on_saveConfig(payload_json.object());
    else if(name == "send-osc")
      on_send_osc(payload_json.object());
    else if(name == "parseVariablesInString")
      on_parseVariablesInString(id, payload_json.object());
    else if(name == "recordAction")
      on_recordAction(payload_json.object());
    else if(name == "setCustomVariable")
      on_setCustomVariable(payload_json.object());
    else if(name == "sharedUdpSocketJoin")
      on_sharedUdpSocketJoin(id, payload_json.object());
    else if(name == "sharedUdpSocketLeave")
    {
      on_sharedUdpSocketLeave(payload_json.object());
      send_success(id);
    }
    else if(name == "sharedUdpSocketSend")
    {
      on_sharedUdpSocketSend(payload_json.object());
      send_success(id);
    }
    else
    {
      qDebug() << "Bitfocus: unhandled call" << name;
      if(hasCallback)
        writeReply(
            id,
            QJsonObject{{"message", QStringLiteral("Unknown command \"%1\"").arg(name.toString())}},
            false);
    }
  }
  else if(direction == "response")
  {
    const int idInt = id.toInt(-1);
    if(idInt == waiting_reply)
      reply_received = true;

    if(idInt == m_init_msg_id)
    {
      if(success.toBool())
        on_init_response(payload_json.object());
      else
        qWarning() << "Bitfocus:" << m_label << "init failed:"
                   << payload_json["message"].toString();
      m_initDone = true;
      completeRegistration();
    }
    else if(idInt == m_req_cfg_id)
    {
      on_response_configFields(payload_json["fields"].toArray());
      m_fieldsDone = true;
      completeRegistration();
    }
    else if(auto act = m_pendingActions.find(idInt); act != m_pendingActions.end())
    {
      const bool ok = success.toBool() && payload_json["success"].toBool(true);
      if(!ok)
      {
        auto msg = payload_json["errorMessage"].toString();
        if(msg.isEmpty())
          msg = payload_json["message"].toString();
        qWarning() << "Bitfocus:" << m_label << "action" << act->second
                   << "failed:" << msg;
      }
      m_pendingActions.erase(act);
    }
    else if(auto it = m_httpCallbacks.find(idInt); it != m_httpCallbacks.end())
    {
      auto response = payload_json["response"].toObject();
      int status = response["status"].toInt(200);
      QString respBody = response["body"].toString();
      QMap<QString, QString> respHeaders;
      auto hObj = response["headers"].toObject();
      for(const auto& k : hObj.keys())
        respHeaders[k] = hObj[k].toString();
      it->second(status, respHeaders, respBody);
      m_httpCallbacks.erase(it);
    }
  }
}

int module_handler::writeRequest(QString name, QString p)
{
  int id = m_cbid++;
  QJsonObject obj;
  obj["direction"] = "call";
  obj["name"] = name;
  obj["payload"] = p;
  obj["callbackId"] = id;

  do_write(jsonToString(obj));
  return id;
}

void module_handler::writeNotification(QString name, QString p)
{
  QJsonObject obj;
  obj["direction"] = "call";
  obj["name"] = name;
  obj["payload"] = p;

  do_write(jsonToString(obj));
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
void module_handler::writeReply(QJsonValue id, QString p, bool success)
{
  QJsonObject obj;
  obj["direction"] = "response";
  obj["payload"] = p;
  obj["success"] = success;
  obj["callbackId"] = id;

  do_write(jsonToString(obj));
}

void module_handler::writeReply(QJsonValue id, QJsonObject p, bool success)
{
  return writeReply(id, QJsonDocument(p).toJson(QJsonDocument::Compact), success);
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

void module_handler::on_process_exited()
{
  if(m_destroyed)
    return;

  // As companion does: start the module again, backing off if it keeps failing
  if(m_uptime.isValid() && m_uptime.elapsed() > 60000)
    m_restarts = 0;
  const int delay = std::min(30000, 1000 << std::min(m_restarts, 5));
  m_restarts++;
  qWarning() << "Bitfocus:" << m_label << "module exited, restarting in" << delay << "ms";

  m_registered = false;
  m_initDone = false;
  m_fieldsDone = false;
  m_firstInit = false;
  m_pendingActions.clear();
  m_httpCallbacks.clear();
  for(auto& [id, port] : m_shared_udp_handles)
    std::erase_if(port->members, [id = id](auto& m) { return m.second == id; });
  m_shared_udp_handles.clear();

  QTimer::singleShot(delay, this, [this] {
    if(!m_destroyed && restart_process())
      m_uptime.start();
  });
}

void module_handler::completeRegistration()
{
  if(!m_initDone || !m_fieldsDone || m_registered)
    return;
  m_registered = true;
  if(std::exchange(m_everRegistered, true))
    reregistered();
  auto queue = std::move(m_afterRegistrationQueue);
  m_afterRegistrationQueue.clear();
  for(auto& fun : queue)
    fun();
}

void module_handler::notifyDefinitionsChanged(DefinitionCategory c)
{
  m_changedDefinitions |= c;
  if(!m_registered || m_definitionsPending)
    return;
  m_definitionsPending = true;
  QTimer::singleShot(0, this, [this] {
    m_definitionsPending = false;
    definitionsChanged();
  });
}

void module_handler::on_setActionDefinitions(QJsonArray actions)
{
  if(actions == m_lastActions)
    return;
  m_lastActions = actions;
  m_model.actions.clear();
  for(auto act : actions)
  {
    auto obj = act.toObject();
    bitfocus::module_data::action_definition def;
    def.hasLearn = obj["hasLearn"].toBool();
    def.name = obj["name"].toString();
    for(auto opt : obj["options"].toArray())
      def.options.push_back(parseConfigField(opt.toObject()));

    m_model.actions.emplace(obj["id"].toString(), std::move(def));
  }
  notifyDefinitionsChanged(Actions);
}

void module_handler::on_setVariableDefinitions(QJsonArray vars, QJsonArray values)
{
  if(vars == m_lastVariables)
  {
    on_setVariableValues(values);
    return;
  }
  m_lastVariables = vars;
  auto old = std::move(m_model.variables);
  m_model.variables.clear();
  for(auto var : vars)
  {
    auto obj = var.toObject();
    const auto id = obj["id"].toString();
    bitfocus::module_data::variable_definition def;
    def.name = obj["name"].toString();
    if(auto it = old.find(id); it != old.end())
      def.value = it->second.value;
    m_model.variables[id] = std::move(def);
  }
  on_setVariableValues(values);
  notifyDefinitionsChanged(Variables);
}

void module_handler::on_setFeedbackDefinitions(QJsonArray fbs)
{
  if(fbs == m_lastFeedbacks)
    return;
  m_lastFeedbacks = fbs;
  m_model.feedbacks.clear();
  for(auto fb : fbs)
  {
    auto obj = fb.toObject();
    bitfocus::module_data::feedback_definition def;
    def.hasLearn = obj["hasLearn"].toBool();
    def.name = obj["name"].toString();
    def.type = obj["type"].toString();
    for(auto opt : obj["options"].toArray())
      def.options.push_back(parseConfigField(opt.toObject()));

    m_model.feedbacks.emplace(obj["id"].toString(), std::move(def));
  }
  notifyDefinitionsChanged(Feedbacks);
}

void module_handler::on_setPresetDefinitions(QJsonArray presets)
{
  m_model.presets.clear();
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
    // An undefined value removes the variable
    if(!obj.contains("value"))
      continue;
    auto& vv = m_model.variables[id];
    vv.value = ejsonDecode(obj["value"]).toVariant();
    variableChanged(id, vv.value);
  }
}

module_data::config_field module_handler::parseConfigField(const QJsonObject& f)
{
  module_data::config_field res;
  res.id = f["id"].toString();
  res.label = f["label"].toString();
  res.type = f["type"].toString();
  res.regex = f["regex"].toString();
  res.tooltip = f["tooltip"].toString();
  res.isVisibleFn = f["isVisibleFn"].toString();
  res.value = f["value"].toVariant();
  res.default_json = f.contains("default") ? f["default"] : QJsonValue(QJsonValue::Undefined);
  res.default_value = f["default"].toVariant();
  res.min = f["min"].toVariant();
  res.max = f["max"].toVariant();
  res.step = f["step"].toVariant();
  res.width = f["width"].toDouble();
  res.allowCustom = f["allowCustom"].toBool();
  res.returnType = f["returnType"].toString();
  {
    for(auto choice_obj : f["choices"].toArray())
    {
      module_data::config_field::choice c;
      auto choice = choice_obj.toObject();
      c.value = choice["id"];
      c.id = c.value.toVariant().toString();
      c.label = choice["label"].toString();
      if(!c.value.isUndefined() && !c.value.isNull())
        res.choices.push_back(std::move(c));
    }
  }
  return res;
}

void module_handler::on_response_configFields(QJsonArray fields)
{
  m_model.config_fields.clear();
  m_secretFields.clear();
  for(auto obj : fields)
  {
    auto field = parseConfigField(obj.toObject());
    if(field.type.startsWith("secret"))
      m_secretFields.insert(field.id);
    m_model.config_fields.push_back(std::move(field));
  }

  configurationParsed();
}

static void writeOscArgument(oscpack::OutboundPacketStream& p, const QJsonValue& arg)
{
  switch(arg.type())
  {
    case QJsonValue::Type::Null:
    case QJsonValue::Type::Undefined:
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
    case QJsonValue::Type::Array:
      for(const auto& a : arg.toArray())
        writeOscArgument(p, a);
      break;
    case QJsonValue::Type::Object: {
      auto obj = arg.toObject();
      if(obj.contains("$binary"))
      {
        auto blob = ejsonBinary(obj);
        p << oscpack::Blob(blob.data(), blob.size());
        break;
      }
      const auto t = obj["type"].toString();
      const auto v = obj["value"];
      if(t == "i")
        p << (int32_t)v.toDouble();
      else if(t == "h")
        p << (int64_t)v.toDouble();
      else if(t == "f")
        p << (float)v.toDouble();
      else if(t == "d")
        p << v.toDouble();
      else if(t == "s" || t == "S")
        p << v.toString().toStdString();
      else if(t == "b")
      {
        auto blob = ejsonBinary(v);
        p << oscpack::Blob(blob.data(), blob.size());
      }
      else if(t == "T")
        p << true;
      else if(t == "F")
        p << false;
      else if(t == "I")
        p << oscpack::Infinitum();
      else if(t == "c")
        p << (char)(v.isString() ? v.toString().toLatin1().append('\0').at(0) : v.toInt());
      else if(t == "t")
        p << oscpack::TimeTag((uint64_t)v.toDouble());
      else if(t == "r")
      {
        auto c = v.toObject();
        p << oscpack::RgbaColor(
            (uint32_t(c["r"].toInt()) << 24) | (uint32_t(c["g"].toInt()) << 16)
            | (uint32_t(c["b"].toInt()) << 8)
            | uint32_t(std::lround(c["a"].toDouble(1.) * 255.)));
      }
      else if(t == "m")
      {
        auto bytes = ejsonBinary(v);
        bytes.resize(4);
        p << oscpack::MidiMessage(
            (uint32_t(uint8_t(bytes[0])) << 24) | (uint32_t(uint8_t(bytes[1])) << 16)
            | (uint32_t(uint8_t(bytes[2])) << 8) | uint32_t(uint8_t(bytes[3])));
      }
      break;
    }
  }
}

void module_handler::on_send_osc(QJsonObject obj)
{
  const std::string host = obj["host"].toString().toStdString();
  const auto pp = obj["port"];
  const int port = pp.isDouble() ? pp.toInt() : pp.toString().toInt();
  const std::string path = obj["path"].toString().toStdString();

  char buf[65535];
  oscpack::OutboundPacketStream p{buf, 65535};
  try
  {
    p << oscpack::BeginMessageN(path);
    writeOscArgument(p, obj["args"]);
    p << oscpack::EndMessage();
  }
  catch(...)
  {
    return;
  }

  try
  {
    boost::system::error_code ec;
    auto addr = boost::asio::ip::make_address(host, ec);
    if(ec)
    {
      boost::asio::ip::udp::resolver resolver{m_send_service};
      auto res = resolver.resolve(boost::asio::ip::udp::v4(), host, "", ec);
      if(ec || res.empty())
        return;
      addr = res.begin()->endpoint().address();
    }
    boost::asio::ip::udp::endpoint endpoint{addr, (uint16_t)port};
    m_socket.send_to(boost::asio::const_buffer(p.Data(), p.Size()), endpoint, 0, ec);
  }
  catch(...)
  {
  }
}

QJsonObject module_handler::configObject(bool secrets) const
{
  // Until the fields are known, secrets cannot be told apart
  const bool known = !m_model.config_fields.empty();
  QJsonObject config;
  for(auto& [k, v] : this->m_model.config)
  {
    const bool isSecret = m_secretFields.contains(k);
    if(!known || isSecret == secrets)
      config[k] = QJsonValue::fromVariant(v);
  }
  return config;
}

int module_handler::init(QString label)
{
  QJsonObject obj;
  obj["label"] = label;
  obj["isFirstInit"] = m_firstInit;
  obj["config"] = configObject(false);
  obj["secrets"] = configObject(true);
  // Without a recorded index the saved configuration is assumed current
  obj["lastUpgradeIndex"]
      = m_firstInit ? -1 : m_model.upgradeIndex.value_or(std::numeric_limits<int>::max());
  obj["actions"] = QJsonObject{};
  obj["feedbacks"] = QJsonObject{};

  return writeRequest("init", jsonToString(obj));
}

void module_handler::on_init_response(const QJsonObject& payload)
{
  m_hasHttpHandler = payload["hasHttpHandler"].toBool();
  bool changed = false;
  if(auto idx = payload["newUpgradeIndex"]; idx.isDouble())
  {
    changed = m_model.upgradeIndex != idx.toInt();
    m_model.upgradeIndex = idx.toInt();
  }

  auto merge = [&](const QJsonValue& v) {
    if(!v.isObject())
      return;
    const auto obj = v.toObject();
    for(auto it = obj.begin(); it != obj.end(); ++it)
    {
      auto val = it.value().toVariant();
      auto& cur = m_model.config[it.key()];
      if(cur != val)
      {
        cur = std::move(val);
        changed = true;
      }
    }
  };
  merge(payload["updatedConfig"]);
  merge(payload["updatedSecrets"]);
  if(changed || m_firstInit)
    configurationSaved();
  m_firstInit = false;
}

void module_handler::send_success(QJsonValue id)
{
  if(id.isUndefined() || id.isNull())
    return;
  QJsonObject obj;
  obj["direction"] = "response";
  obj["callbackId"] = id;
  obj["success"] = true;
  do_write(jsonToString(obj));
}

void module_handler::on_log_message(QJsonObject obj)
{
  const auto level = obj["level"].toString();
  if(level == "error" || level == "warn")
    qWarning().noquote() << "Bitfocus:" << m_label << obj["message"].toString();
}

void module_handler::on_set_status(QJsonObject obj) { }

void module_handler::on_saveConfig(QJsonObject obj)
{
  auto apply = [this](const QJsonValue& v, bool secrets) {
    if(!v.isObject())
      return;
    std::erase_if(m_model.config, [&](const auto& kv) {
      return m_secretFields.contains(kv.first) == secrets;
    });
    const auto conf = v.toObject();
    for(auto it = conf.begin(); it != conf.end(); ++it)
      m_model.config[it.key()] = it.value().toVariant();
  };
  apply(obj["config"], false);
  apply(obj["secrets"], true);
  configurationSaved();
}

void module_handler::on_parseVariablesInString(QJsonValue id, QJsonObject obj)
{
  QString text = obj["text"].toString();
  QJsonArray varIds;

  static const QRegularExpression varRegex(R"(\$\(([^:$)]+):([^)$]+)\))");
  QString parsed;
  qsizetype last = 0;
  auto matchIt = varRegex.globalMatch(text);
  while(matchIt.hasNext())
  {
    auto match = matchIt.next();
    const QString label = match.captured(1);
    const QString varName = match.captured(2);
    varIds.append(QString(label + ":" + varName));

    parsed += QStringView{text}.mid(last, match.capturedStart() - last);
    auto vit = m_model.variables.find(varName);
    if(label == m_label && vit != m_model.variables.end() && vit->second.value.isValid())
      parsed += vit->second.value.toString();
    else
      parsed += QStringLiteral("$NA");
    last = match.capturedEnd();
  }
  parsed += QStringView{text}.mid(last);

  writeReply(id, QJsonObject{{"text", parsed}, {"variableIds", varIds}}, true);
}

void module_handler::on_updateFeedbackValues(QJsonObject obj)
{
  auto values = obj["values"].toArray();
  for(const auto& val : values)
  {
    auto v = val.toObject();
    QString id = v["id"].toString();
    QString controlId = v["controlId"].toString();
    QVariant value = ejsonDecode(v["value"]).toVariant();
    feedbackValueChanged(id, controlId, value);
  }
}
void module_handler::on_recordAction(QJsonObject obj) { }
void module_handler::on_setCustomVariable(QJsonObject obj) { }
void module_handler::on_sharedUdpSocketJoin(QJsonValue id, QJsonObject obj)
{
  const QString family = obj["family"].toString() == "udp6" ? "udp6" : "udp4";
  const int portNumber = obj["portNumber"].toInt();

  auto& reg = shared_udp_port::registry();
  auto port = reg[{family, portNumber}].lock();
  if(!port)
  {
    port = std::make_shared<shared_udp_port>();
    port->family = family;
    port->port = portNumber;
    const bool ok = port->socket.bind(
        family == "udp6" ? QHostAddress::AnyIPv6 : QHostAddress::AnyIPv4, portNumber,
        QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint);
    if(!ok)
    {
      writeReply(id, QJsonObject{{"message", port->socket.errorString()}}, false);
      return;
    }
    QObject::connect(
        &port->socket, &QUdpSocket::readyRead, port.get(), &shared_udp_port::on_read);
    reg[{family, portNumber}] = port;
  }

  const QString handleId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  port->members.emplace_back(this, handleId);
  m_shared_udp_handles[handleId] = port;

  writeReply(id, QStringLiteral("\"%1\"").arg(handleId), true);
}

void module_handler::sharedUdpSocketMessage(
    const QString& handleId, int port, const QByteArray& data, const QString& address,
    int sourcePort, bool ipv6)
{
  QJsonObject msg;
  msg["handleId"] = handleId;
  msg["portNumber"] = port;
  msg["message"] = ejsonBinaryValue(data);
  msg["source"] = QJsonObject{
      {"address", address},
      {"family", ipv6 ? "IPv6" : "IPv4"},
      {"port", sourcePort},
      {"size", (int)data.size()}};
  writeNotification("sharedUdpSocketMessage", jsonToString(msg));
}

void module_handler::sharedUdpSocketError(
    const QString& handleId, int port, const QString& message)
{
  QJsonObject err;
  err["handleId"] = handleId;
  err["portNumber"] = port;
  err["error"] = QJsonObject{{"message", message}};
  writeNotification("sharedUdpSocketError", jsonToString(err));
}

void module_handler::on_sharedUdpSocketLeave(QJsonObject obj)
{
  QString handleId = obj["handleId"].toString();
  if(auto it = m_shared_udp_handles.find(handleId); it != m_shared_udp_handles.end())
  {
    std::erase_if(
        it->second->members, [&](auto& m) { return m.second == handleId; });
    m_shared_udp_handles.erase(it);
  }
}

void module_handler::on_sharedUdpSocketSend(QJsonObject obj)
{
  QString handleId = obj["handleId"].toString();
  auto it = m_shared_udp_handles.find(handleId);
  if(it == m_shared_udp_handles.end())
    return;

  const QByteArray data = ejsonBinary(obj["message"]);
  const QString address = obj["address"].toString();
  const int port = obj["port"].toInt();

  QHostAddress dest{address};
  if(dest.isNull())
  {
    const auto addrs = QHostInfo::fromName(address).addresses();
    if(addrs.isEmpty())
      return;
    dest = addrs.front();
  }
  it->second->socket.writeDatagram(data, dest, port);
}

void module_handler::updateConfigAndLabel(QString label, module_configuration conf)
{
  if(label.isEmpty())
    label = m_label;
  if(label == m_label && conf == m_model.config)
    return;

  m_label = label;
  this->m_model.config = std::move(conf);

  if(m_expects_label_updates)
  {
    QJsonObject obj;
    obj["config"] = configObject(false);
    obj["secrets"] = configObject(true);
    obj["label"] = label;
    writeRequest("updateConfigAndLabel", jsonToString(obj));
  }
  else
  {
    writeRequest("updateConfig", jsonToString(configObject(false)));
  }
}

int module_handler::requestConfigFields()
{
  return writeRequest("getConfigFields", jsonToString(QJsonObject{}));
}

void module_handler::updateFeedbacks(
    const std::map<QString, module_data::feedback_instance>& feedbacks)
{
  QJsonObject fb_map;
  for(const auto& [id, fb] : feedbacks)
  {
    if(fb.disabled)
    {
      fb_map[id] = QJsonValue::Null;
    }
    else
    {
      const int upgradeIndex
          = fb.upgradeIndex >= 0 ? fb.upgradeIndex : m_model.upgradeIndex.value_or(-1);
      fb_map[id] = QJsonObject{
          {"id", id},
          {"controlId", fb.controlId},
          {"feedbackId", fb.definitionId},
          {"options", QJsonObject::fromVariantMap(fb.options)},
          {"isInverted", fb.isInverted},
          {"image", QJsonObject{{"width", fb.imageWidth}, {"height", fb.imageHeight}}},
          {"upgradeIndex", upgradeIndex >= 0 ? QJsonValue(upgradeIndex) : QJsonValue::Null},
          {"disabled", false},
      };
    }
  }

  writeRequest("updateFeedbacks", jsonToString(QJsonObject{{"feedbacks", fb_map}}));
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

void module_handler::actionRun(std::string_view act, QVariantMap options)
{
  const auto actionId = QString::fromUtf8(act.data(), act.size());
  QJsonObject act_object;
  act_object["id"] = QStringLiteral("score-%1").arg(++m_actionId);
  // One control per action node: modules keep per-button state by control id
  act_object["controlId"] = QString(QStringLiteral("action/") + actionId);
  act_object["actionId"] = actionId;
  act_object["options"] = QJsonObject::fromVariantMap(options);
  act_object["upgradeIndex"] = QJsonValue{QJsonValue::Type::Null};
  act_object["disabled"] = false;
  QJsonObject root;
  root["action"] = act_object;

  const int id = writeRequest("executeAction", jsonToString(root));
  m_pendingActions[id] = actionId;
}

void module_handler::destroy()
{
  if(m_destroyed)
    return;
  m_destroyed = true;

  for(auto& [id, port] : m_shared_udp_handles)
    std::erase_if(port->members, [id = id](auto& m) { return m.second == id; });
  m_shared_udp_handles.clear();

  m_httpCallbacks.clear();

  // Let the module close its connections before the process gets killed
  if(m_registered)
    wait_for_reply(writeRequest("destroy", "{}"), 1000);
}

void module_handler::executeHttpRequest(
    const QString& method, const QString& path, const QString& body,
    const QMap<QString, QString>& headers, const QMap<QString, QString>& query,
    std::function<void(int status, QMap<QString, QString> respHeaders, QString respBody)>
        callback)
{
  if(!m_hasHttpHandler)
  {
    callback(404, {}, R"({"status":404,"message":"Not Found"})");
    return;
  }

  QJsonObject request;
  request["method"] = method;
  request["path"] = path;
  request["body"] = body;
  request["baseUrl"] = "";
  request["hostname"] = "localhost";
  request["originalUrl"] = path;

  QJsonObject headersObj;
  for(auto it = headers.begin(); it != headers.end(); ++it)
    headersObj[it.key()] = it.value();
  request["headers"] = headersObj;

  QJsonObject queryObj;
  for(auto it = query.begin(); it != query.end(); ++it)
    queryObj[it.key()] = it.value();
  request["query"] = queryObj;

  int msgId = writeRequest("handleHttpRequest", jsonToString(QJsonObject{{"request", request}}));
  m_httpCallbacks[msgId] = std::move(callback);
}

void module_handler::startStopRecordingActions()
{
  qDebug() << "TODO" << Q_FUNC_INFO;
}

const module_data& module_handler::model()
{
  return m_model;
}
}
