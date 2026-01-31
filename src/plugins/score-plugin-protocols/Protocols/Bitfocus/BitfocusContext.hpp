#pragma once
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include <QColor>
#include <QCoreApplication>
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
struct module_data
{
  struct config_field
  {
    struct choice
    {
      QString id;
      QString label;
    };

    QString id;
    QString label;
    // "static-text", "textinput", "number", "checkbox", "choices", "bonjour-device", "dropdown"
    QString type;
    QVariant value;
    QString tooltip;
    QString regex;
    // ex. :  "e=>!![\"TF\",\"DM3\",\"DM7\"].includes(e.model)&&(e.kaInterval=e.kaIntervalH,!0)",
    QString isVisibleFn;
    QVariant min;
    QVariant max;
    std::vector<choice> choices;
    QVariant default_value{}; // true, a number, a string etc
    double width{};
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
    int imageHeight{72};
    int upgradeIndex{-1};
    bool disabled{false};
  };

  std::map<QString, action_definition> actions;
  std::map<QString, variable_definition> variables;
  std::map<QString, feedback_definition> feedbacks;
  std::map<QString, preset_definition> presets;
  std::vector<config_field> config_fields;
  module_configuration config;
};

// note: callback id shared between both ends so every message has to be processed in order
#if defined(_WIN32)
struct win32_handles;
struct module_handler_base : public QObject
{
  std::unique_ptr<win32_handles> handles{};
  explicit module_handler_base(
      QString node_path, QString module_path, QString entrypoint);
  virtual ~module_handler_base();
  void do_write(std::string_view res);
  virtual void processMessage(std::string_view) = 0;
};
#else
struct module_handler_base : public QObject
{
  char buf[16 * 4096]{};
  std::vector<char> queue;
  QProcess process{};
  QSocketNotifier* socket{};
  int pfd[2]{};

  explicit module_handler_base(
      QString node_path, QString module_path, QString entrypoint);
  virtual ~module_handler_base();

  void on_read(QSocketDescriptor, QSocketNotifier::Type);
  void do_write(std::string_view res);

  virtual void processMessage(std::string_view) = 0;
};
#endif

struct module_handler final : public module_handler_base
{
  W_OBJECT(module_handler)
public:
  explicit module_handler(
      QString path, QString entrypoint, QString nodeVersion, QString apiversion,
      module_configuration config);
  virtual ~module_handler();

  using module_handler_base::do_write;
  void do_write(QString res);
  QString jsonToString(QJsonObject obj);

  void afterRegistration(std::function<void()>);

  void processMessage(std::string_view v) override;

  int writeRequest(QString name, QString p);
  void writeReply(QJsonValue id, QString p);
  void writeReply(QJsonValue id, QJsonObject p);
  void writeReply(QJsonValue id, QString p, bool success);
  void writeReply(QJsonValue id, QJsonObject p, bool success);

  // Module -> app (requests handling)
  void on_register(QJsonValue id);
  void on_setActionDefinitions(QJsonArray obj);
  void on_setVariableDefinitions(QJsonArray obj);
  void on_setFeedbackDefinitions(QJsonArray obj);
  void on_setPresetDefinitions(QJsonArray obj);
  void on_setVariableValues(QJsonArray obj);
  void on_set_status(QJsonObject obj);
  void on_saveConfig(QJsonObject obj);
  void on_parseVariablesInString(QJsonValue id, QJsonObject obj);
  void on_updateFeedbackValues(QJsonObject obj);
  void on_recordAction(QJsonObject obj);
  void on_setCustomVariable(QJsonObject obj);
  void on_sharedUdpSocketJoin(QJsonValue id, QJsonObject obj);
  void on_sharedUdpSocketLeave(QJsonObject obj);
  void on_sharedUdpSocketSend(QJsonObject obj);
  void on_send_osc(QJsonObject obj);

  module_data::config_field parseConfigField(QJsonObject f);

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
  void sharedUdpSocketMessage();
  void sharedUdpSocketError();
  const bitfocus::module_data& model();

  void configurationParsed() W_SIGNAL(configurationParsed);
  void variableChanged(QString var, QVariant val) W_SIGNAL(variableChanged, var, val);
  void feedbackValueChanged(QString id, QString controlId, QVariant value)
      W_SIGNAL(feedbackValueChanged, id, controlId, value);

private:
  struct shared_udp_handle
  {
    QString handleId;
    QString family;
    int portNumber{};
    boost::asio::ip::udp::socket socket;
    boost::asio::ip::udp::endpoint sender_endpoint;
    std::array<char, 65536> recv_buffer{};

    explicit shared_udp_handle(boost::asio::io_context& ctx)
        : socket(ctx)
    {
    }
  };

  void start_udp_receive(shared_udp_handle* h);

  bitfocus::module_data m_model;

  boost::asio::io_context m_send_service;
  boost::asio::ip::udp::socket m_socket{m_send_service};

  std::map<QString, std::unique_ptr<shared_udp_handle>> m_shared_udp_handles;
  std::map<int, std::function<void(int, QMap<QString, QString>, QString)>> m_httpCallbacks;

  std::vector<std::function<void()>> m_afterRegistrationQueue;
  int m_cbid{1};

  int m_init_msg_id{-1};
  int m_req_cfg_id{-1};

  bool m_expects_label_updates{true};
  bool m_hasHttpHandler{false};
  bool m_registered{false};
};

} // namespace bitfocus
