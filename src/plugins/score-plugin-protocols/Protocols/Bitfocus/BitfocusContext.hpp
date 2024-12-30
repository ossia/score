#pragma once
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSocketNotifier>

#include <verdigris>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <sys/un.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#endif
namespace bitfocus
{
struct module
{
};

struct connection
{
  struct action_definition
  {
    bool hasLearn;
    QString name;
    std::vector<QVariantMap> options;
  };
  struct variable_definition
  {
    QString name;
    QVariant value;
  };
  struct feedback_definition
  {
    bool hasLearn;
    QString name;
    std::vector<QVariantMap> options;
    QString type;

    //{"defaultStyle":{"bgcolor":16711680,"color":0},
    // "hasLearn":false,
    // "id":"showState",
    // "name":"Show state feedback",
    // "options":[{"choices":[{"id":"slideshow","label":"Slide show"},{"id":"edit","label":"Edit"}],"default":"slideshow","id":"state","label":"State","type":"dropdown"}],
    // "type":"boolean"
    // }
  };
  struct preset_definition
  {
    QString name;
    QString category;
    QString text;
    QString type;
    std::vector<QVariantMap> feedbacks;
    std::vector<QVariantMap> steps;
  };

  std::map<QString, action_definition> actions;
  std::map<QString, variable_definition> variables;
  std::map<QString, feedback_definition> feedbacks;
  std::map<QString, preset_definition> presets;

  struct config_field
  {
  };

  struct config_ui
  {
    std::vector<config_field> fields;
    std::map<std::string, std::string> config;
  };
};

struct companion
{
  std::map<std::string, module> modules;
  std::map<std::string, connection> connections;
};

// note: callback id shared between both ends so every message has to be processed in order
#if defined(_WIN32)
struct module_handler_base : public QObject
{
  explicit module_handler_base(QString module_path)
  {
    // https://doc.qt.io/qt-6/qwineventnotifier.html
    // https://forum.qt.io/topic/146343/qsocketnotifier-with-win32-namedpipes/9
    // Or maybe QLocalSocket just works on windows?

    // FIXME
  }

  virtual ~module_handler_base();
  void do_write(std::string_view res)
  {
    // FIXME
  }
  void do_write(const QByteArray& res)
  {
    // FIXME
  }
  virtual void processMessage(std::string_view) = 0;
};
#else
struct module_handler_base : public QObject
{
  char buf[16 * 4096];
  QProcess process;
  QSocketNotifier* socket{};
  int pfd[2];

  explicit module_handler_base(QString module_path)
  {
    // Create socketpair
    socketpair(PF_LOCAL, SOCK_STREAM, 0, pfd);

    // Create env
    auto genv = QProcessEnvironment::systemEnvironment();
    genv.insert("CONNECTION_ID", "connectionId");
    genv.insert("VERIFICATION_TOKEN", "foobar");
    genv.insert("MODULE_MANIFEST", module_path + "/companion/manifest.json");
    genv.insert("NODE_CHANNEL_SERIALIZATION_MODE", "json");
    genv.insert("NODE_CHANNEL_FD", QString::number(pfd[1]).toUtf8());

    auto socket = new QSocketNotifier(pfd[0], QSocketNotifier::Read, this);
    QObject::connect(
        socket, &QSocketNotifier::activated, this, &module_handler_base::on_read);

    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.setProgram("node");
    process.setArguments({"main.js"}); // FIXME entrypoint from spec
    process.setWorkingDirectory(module_path);
    process.setProcessEnvironment(genv);

    process.start();

    // See https://forum.qt.io/topic/33964/solved-child-qprocess-that-dies-with-parent/10

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

  virtual ~module_handler_base();

  void on_read(QSocketDescriptor, QSocketNotifier::Type)
  {
    ssize_t rl = ::read(pfd[0], buf, sizeof(buf));
    if(rl <= 0)
      return;
    char* pos = buf;
    char* idx = buf;
    char* const end = pos + rl;
    do
    {
      idx = std::find(pos, end, '\n');
      if(idx < end)
      {
        std::ptrdiff_t diff = idx - pos;
        std::string_view message(pos, diff);
        this->processMessage(message);
        pos = idx + 1;
        continue;
      }
    } while(idx < end);
  }

  void do_write(std::string_view res) { ::write(pfd[0], res.data(), res.size()); }
  void do_write(const QByteArray& res) { ::write(pfd[0], res.data(), res.size()); }

  virtual void processMessage(std::string_view) = 0;
};
#endif
struct module_handler final : public module_handler_base
{
  W_OBJECT(module_handler)
public:
  explicit module_handler(QString path)
      : module_handler_base{path}
  {
  }

  virtual ~module_handler();

  QString toPayload(QJsonObject obj)
  {
    return QJsonDocument{obj}.toJson(QJsonDocument::Compact);
  }

  void processMessage(std::string_view v) override
  {
    auto doc = QJsonDocument::fromJson(QByteArray::fromRawData(v.data(), v.size()));
    auto id = doc.object()["callbackId"];
    auto direction = doc.object()["direction"];
    auto pay = doc.object()["payload"];
    auto name = doc.object()["name"];
    auto success = doc.object()["success"];

    auto payload_json = QJsonDocument::fromJson(pay.toString().toUtf8());
    auto pretty = [&] { qDebug() << payload_json.toJson().toStdString().data(); };

    if(direction == "call")
    {
      if(name == "register")
      {
        // First message
        on_register();

        QMetaObject::invokeMethod(this, [this] { init_msg_id = init(); });
      }
      else if(name == "upgradedItems")
      {
        QMetaObject::invokeMethod(this, [this, n = id.toInt()] { send_success(n); });
      }
      else if(name == "setActionDefinitions")
      {
        auto actions = payload_json["actions"].toArray();
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
      else if(name == "setFeedbackDefinitions")
      {
        auto fbs = payload_json["feedbacks"].toArray();
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
      else if(name == "setVariableDefinitions")
      {
        auto vars = payload_json["variables"].toArray();
        for(auto var : vars)
        {
          auto obj = var.toObject();
          bitfocus::connection::variable_definition def;
          def.name = obj["name"].toString();
          m_model.variables[obj["id"].toString()] = std::move(def);
        }
      }
      else if(name == "setPresetDefinitions")
      {
        auto vars = payload_json["presets"].toArray();
        for(auto var : vars)
        {
          auto obj = var.toObject();
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
      else if(name == "setVariableValues")
      {
        auto vars = payload_json["newValues"].toArray();
        for(auto var : vars)
        {
          auto obj = var.toObject();
          m_model.variables[obj["id"].toString()].value = obj["value"].toVariant();
        }
      }
      else if(name == "log-message")
      {
        // qDebug() << " !! Unhandled !! " << name;
        // pretty();
      }
      else if(name == "set-status")
      {
        qDebug() << " !! Unhandled !! " << name;
        pretty();
      }
      else if(name == "updateFeedbackValues")
      {
        qDebug() << " !! Unhandled !! " << name;
        pretty();
      }
      else if(name == "saveConfig")
      {
        qDebug() << " !! Unhandled !! " << name;
        pretty();
      }
      else if(name == "send-osc")
      {
        on_send_osc(payload_json.object());
      }
      else if(name == "parseVariablesInString")
      {
        qDebug() << " !! Unhandled !! " << name;
        pretty();
      }
      else if(name == "recordAction")
      {
        qDebug() << " !! Unhandled !! " << name;
        pretty();
      }
      else if(name == "setCustomVariable")
      {
        qDebug() << " !! Unhandled !! " << name;
        pretty();
      }
      else if(name == "sharedUdpSocketJoin")
      {
        qDebug() << " !! Unhandled !! " << name;
        pretty();
      }
      else if(name == "sharedUdpSocketLeave")
      {
        qDebug() << " !! Unhandled !! " << name;
        pretty();
      }
      else if(name == "sharedUdpSocketSend")
      {
        qDebug() << " !! Unhandled !! " << name;
        pretty();
      }
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
        this->requestConfigFields();
      }
    }
  }

  int writeRequest(QString name, QString p)
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

  void writeReply(int id, QString p)
  {
    QJsonObject obj;
    obj["direction"] = "response";
    obj["payload"] = p;
    obj["callbackId"] = id;
    auto res = toPayload(obj).toUtf8().append('\n');
    qDebug() << "sending: " << res;
    do_write(res);
  }

  // Module -> app
  void on_register()
  {
    // Payload in ejson format.
    std::string_view res
        = R"_({"direction":"response","callbackId":1,"success":true,"payload":"{}"})_"
          "\n";
    qDebug().noquote().nospace() << "sending: " << res.data();
    do_write(res);
    cbid = 1;
  }

  void on_send_osc(QJsonObject obj)
  {
    const QString host = obj["host"].toString();
    const int port = obj["port"].toInt();
    const QString path = obj["path"].toString();
    const auto args = obj["args"].toArray();

    qDebug() << "TODO send osc" << path << args;
  }

  // App -> module

  int init()
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

  void send_success(int id)
  {
    QJsonObject obj;
    obj["direction"] = "response";
    obj["callbackId"] = id;
    obj["success"] = true;
    auto res = toPayload(obj).toUtf8().append('\n');
    do_write(res);
  }

  void updateConfigAndLabel() { qDebug() << "TODO" << Q_FUNC_INFO; }
  int requestConfigFields()
  {
    return writeRequest("getConfigFields", toPayload(QJsonObject{}));
  }
  void updateFeedbacks() { qDebug() << "TODO" << Q_FUNC_INFO; }
  void feedbackLearnValues() { qDebug() << "TODO" << Q_FUNC_INFO; }
  void feedbackDelete() { qDebug() << "TODO" << Q_FUNC_INFO; }
  void variablesChanged()
  {
    qDebug() << "TODO" << Q_FUNC_INFO;
    // {"direction":"call","name":"variablesChanged","payload":"{\"variablesIds\":[\"internal:time_hms\",\"internal:time_s\",\"internal:time_unix\",\"internal:time_hms_12\",\"internal:uptime\"]}"}
  }
  void actionUpdate() { qDebug() << "TODO" << Q_FUNC_INFO; }
  void actionDelete() { qDebug() << "TODO" << Q_FUNC_INFO; }
  void actionLearnValues() { qDebug() << "TODO" << Q_FUNC_INFO; }
  void actionRun()
  {
    // {"direction":"call",
    // "name":"executeAction",
    // "payload":
    // "{\"action\":
    //    {\"id\":\"ZNn2OwGmoX3ApvT2MdfkM\",
    //     \"controlId\":\"bank:j_kyzoAM5Cwj0Lb1dx5MS\",
    //     \"actionId\":\"goto_first_slide\",
    //     \"options\":{},
    //     \"upgradeIndex\":null,
    //     \"disabled\":false
    //    },\"surfaceId\":\"hot:tablet\"}",
    // "callbackId":3}

    qDebug() << "TODO" << Q_FUNC_INFO;
  }
  void destroy() { qDebug() << "TODO" << Q_FUNC_INFO; }
  void executeHttpRequest() { qDebug() << "TODO" << Q_FUNC_INFO; }
  void startStopRecordingActions() { qDebug() << "TODO" << Q_FUNC_INFO; }
  void sharedUdpSocketMessage() { qDebug() << "TODO" << Q_FUNC_INFO; }
  void sharedUdpSocketError() { qDebug() << "TODO" << Q_FUNC_INFO; }
  const bitfocus::connection& model() { return m_model; }

  void configurationParsed() W_SIGNAL(configurationParsed);

private:
  bitfocus::connection m_model;
  int cbid = 0;

  int init_msg_id{};
};

} // namespace bitfocus
