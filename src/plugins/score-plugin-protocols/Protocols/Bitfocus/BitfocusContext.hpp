#pragma once
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QUuid>

#include <array>
#include <functional>
#include <optional>
#include <set>

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

using module_configuration = std::map<QString, QVariant>;

//! Path to the node binary bundled for a manifest's runtime type, e.g. "node22"
QString nodeExecutable(const QString& nodeVersion);

struct module_data
{
  struct config_field
  {
    struct choice
    {
      QString id;
      QString label;
      //! The id as the module declared it: modules compare it strictly, 1 != "1"
      QJsonValue value;
    };

    QString id;
    QString label;
    // "static-text", "textinput", "secret-text", "number", "checkbox", "dropdown",
    // "multidropdown", "colorpicker", "bonjour-device", "custom-variable"
    QString type;
    QVariant value;
    QString tooltip;
    QString regex;
    // ex. :  "e=>!![\"TF\",\"DM3\",\"DM7\"].includes(e.model)&&(e.kaInterval=e.kaIntervalH,!0)",
    QString isVisibleFn;
    QVariant min;
    QVariant max;
    QVariant step;
    std::vector<choice> choices;
    QVariant default_value{}; // true, a number, a string etc
    QJsonValue default_json{QJsonValue::Undefined};
    QString returnType;
    double width{};
    bool allowCustom{};

    bool hasDefault() const noexcept { return !default_json.isUndefined(); }
    //! Integral numbers: all of default, min, max and step are integers
    bool isInteger() const noexcept;
  };

  struct action_definition
  {
    bool hasLearn{};
    QString name;
    std::vector<config_field> options;
  };
  struct variable_definition
  {
    QString name;
    QVariant value;
  };
  struct feedback_definition
  {
    bool hasLearn{};
    bool isInverted{false};
    bool disabled{false};
    int upgradeIndex{0};
    QString name;
    std::vector<config_field> options;
    QString type;
    struct
    {
      QColor bgcolor;
      QColor color;
    } defaultStyle;
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

  struct feedback_instance
  {
    QString id;
    QString controlId;
    QString definitionId;
    QVariantMap options;
    int imageWidth{72};
    int imageHeight{58};
    int upgradeIndex{-1};
    bool disabled{false};
    bool isInverted{false};
  };

  std::map<QString, action_definition> actions;
  std::map<QString, variable_definition> variables;
  std::map<QString, feedback_definition> feedbacks;
  std::map<QString, preset_definition> presets;
  std::vector<config_field> config_fields;
  module_configuration config;
  //! Upgrade scripts the configuration went through, as reported by init
  std::optional<int> upgradeIndex;
};

//! The value companion puts in an option the user did not touch
QJsonValue defaultOptionValue(const module_data::config_field& f);

//! Converts a value edited in score back to the type the module declared
QJsonValue toModuleValue(const module_data::config_field& f, const QVariant& v);

//! The number a float stands for, e.g. 0.1f -> 0.1 rather than 0.10000000149
double floatToDouble(float f);

//! A QVariant holding a float becomes the double the float stands for
QVariant widenFloat(const QVariant& v);

//! Decodes the EJSON extensions (binary, dates, non-finite numbers) in a value
QJsonValue ejsonDecode(const QJsonValue& v);

// note: callback id shared between both ends so every message has to be processed in order
#if defined(_WIN32)
struct win32_handles;
struct module_handler_base : public QObject
{
  std::unique_ptr<win32_handles> handles{};
  explicit module_handler_base(
      QString node_path, QString module_path, QString entrypoint,
      QString connection_id);
  virtual ~module_handler_base();
  void do_write(std::string_view res);
  //! Starts the module again, false where unsupported
  bool restart_process();
  virtual void processMessage(std::string_view) = 0;
  virtual void on_process_exited() { }

  //! How long the process gets to shut down once released
  int release_grace_ms{};
};
#else
struct module_handler_base : public QObject
{
  char buf[16 * 4096]{};
  std::vector<char> queue;
  std::unique_ptr<QProcess> process;
  QSocketNotifier* socket{};
  int pfd[2]{-1, -1};

  explicit module_handler_base(
      QString node_path, QString module_path, QString entrypoint,
      QString connection_id);
  virtual ~module_handler_base();

  void on_read(QSocketDescriptor, QSocketNotifier::Type);
  void do_write(std::string_view res);
  //! Starts the module again, false where unsupported
  bool restart_process();

  virtual void processMessage(std::string_view) = 0;
  virtual void on_process_exited() { }

  //! How long the process gets to shut down once released
  int release_grace_ms{};

private:
  void start_process();
  void release_process(int grace_ms);
  void process_queue();

  QString m_nodePath, m_modulePath, m_entrypoint, m_connectionId;
};
#endif

struct shared_udp_port;
struct module_handler final : public module_handler_base
{
  W_OBJECT(module_handler)
public:
  explicit module_handler(
      QString path, QString entrypoint, QString nodeVersion, QString apiversion,
      module_configuration config, QString label = {}, bool firstInit = false,
      std::optional<int> upgradeIndex = {},
      std::optional<std::set<QString>> secretKeys = {});
  virtual ~module_handler();

  using module_handler_base::do_write;
  void do_write(QString res);
  QString jsonToString(QJsonObject obj);

  void afterRegistration(std::function<void()>);

  void processMessage(std::string_view v) override;
  void on_process_exited() override;

  int writeRequest(QString name, QString p);
  void writeNotification(QString name, QString p);
  void writeReply(QJsonValue id, QString p);
  void writeReply(QJsonValue id, QJsonObject p);
  void writeReply(QJsonValue id, QString p, bool success);
  void writeReply(QJsonValue id, QJsonObject p, bool success);

  // Module -> app (requests handling)
  void on_register(QJsonValue id);
  void on_setActionDefinitions(QJsonArray obj);
  void on_setVariableDefinitions(QJsonArray obj, QJsonArray values);
  void on_setFeedbackDefinitions(QJsonArray obj);
  void on_setPresetDefinitions(QJsonArray obj);
  void on_setVariableValues(QJsonArray obj);
  void on_set_status(QJsonObject obj);
  void on_log_message(QJsonObject obj);
  void on_saveConfig(QJsonObject obj);
  void on_parseVariablesInString(QJsonValue id, QJsonObject obj);
  void on_updateFeedbackValues(QJsonObject obj);
  void on_recordAction(QJsonObject obj);
  void on_setCustomVariable(QJsonObject obj);
  void on_sharedUdpSocketJoin(QJsonValue id, QJsonObject obj);
  void on_sharedUdpSocketLeave(QJsonObject obj);
  void on_sharedUdpSocketSend(QJsonObject obj);
  void on_send_osc(QJsonObject obj);

  module_data::config_field parseConfigField(const QJsonObject& f);

  // Module -> app (replies handling)
  void on_response_configFields(QJsonArray fields);

  // App -> module

  int init(QString label);
  void send_success(QJsonValue id);
  void updateConfigAndLabel(QString label, module_configuration conf);
  int requestConfigFields();
  void updateFeedbacks(const std::map<QString, module_data::feedback_instance>& feedbacks);
  void feedbackLearnValues();
  void feedbackDelete();
  void variablesChanged();
  void actionUpdate();
  void actionDelete();
  void actionLearnValues();
  void actionRun(std::string_view act, QVariantMap options);
  void destroy();
  void executeHttpRequest(
      const QString& method, const QString& path, const QString& body,
      const QMap<QString, QString>& headers, const QMap<QString, QString>& query,
      std::function<void(int status, QMap<QString, QString> respHeaders, QString respBody)>
          callback);
  void startStopRecordingActions();
  void sharedUdpSocketMessage(
      const QString& handleId, int port, const QByteArray& data, const QString& address,
      int sourcePort, bool ipv6);
  void sharedUdpSocketError(const QString& handleId, int port, const QString& message);
  const bitfocus::module_data& model();
  //! The configuration keys of the secret fields, once known
  std::optional<std::set<QString>> secretKeys() const;

  enum DefinitionCategory
  {
    Actions = 1,
    Feedbacks = 2,
    Variables = 4,
    All = 7
  };
  //! The categories changed since the last call
  int takeChangedDefinitions() noexcept { return std::exchange(m_changedDefinitions, 0); }

  void configurationParsed() W_SIGNAL(configurationParsed);
  //! The module registered again after its process was restarted
  void reregistered() W_SIGNAL(reregistered);
  //! Action, feedback or variable definitions changed after registration
  void definitionsChanged() W_SIGNAL(definitionsChanged);
  //! The configuration the module asked to persist
  void configurationSaved() W_SIGNAL(configurationSaved);
  void variableChanged(QString var, QVariant val) W_SIGNAL(variableChanged, var, val);
  void feedbackValueChanged(QString id, QString controlId, QVariant value)
      W_SIGNAL(feedbackValueChanged, id, controlId, value);

private:
  QJsonObject configObject(bool secrets) const;
  void notifyDefinitionsChanged(DefinitionCategory c);
  void completeRegistration();
  void on_init_response(const QJsonObject& payload);

  bitfocus::module_data m_model;

  boost::asio::io_context m_send_service;
  boost::asio::ip::udp::socket m_socket{m_send_service};

  std::map<QString, std::shared_ptr<shared_udp_port>> m_shared_udp_handles;
  std::map<int, std::function<void(int, QMap<QString, QString>, QString)>> m_httpCallbacks;
  std::map<int, QString> m_pendingActions;

  std::vector<std::function<void()>> m_afterRegistrationQueue;
  std::set<QString> m_secretFields;
  bool m_secretsKnown{};
  QString m_label;
  int m_cbid{1};
  int m_actionId{};

  int m_init_msg_id{-1};
  int m_req_cfg_id{-1};

  bool m_expects_label_updates{true};
  bool m_hasHttpHandler{false};
  bool m_registered{false};
  bool m_firstInit{false};
  bool m_destroyed{false};
  bool m_definitionsPending{false};
  int m_changedDefinitions{};
  QJsonArray m_lastActions, m_lastFeedbacks, m_lastVariables;
  bool m_initDone{false};
  bool m_everRegistered{false};
  int m_restarts{0};
  QElapsedTimer m_uptime;
  bool m_fieldsDone{false};
};

} // namespace bitfocus
