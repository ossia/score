#pragma once
#include <Vst3/Plugin.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <ossia/detail/fmt.hpp>

#include <QProcess>
#include <QWebSocketServer>
#include <QElapsedTimer>

#include <memory>
#include <stdexcept>

namespace vst3
{
struct vst_error: public std::runtime_error
{
  template<typename... Args>
  vst_error(Args&&... args) noexcept
    : runtime_error{fmt::format(args...)}
  {

  }
};

struct AvailablePlugin
{
  QString path;
  QString name;
  std::vector<VST3::Hosting::ClassInfo> classInfo;

  bool isValid{};
};

struct ApplicationPlugin
    : public QObject
    , public score::ApplicationPlugin
{
  W_OBJECT(ApplicationPlugin)
public:
  ApplicationPlugin(const score::ApplicationContext& ctx);

  void initialize() override;

  VST3::Hosting::Module::Ptr getModule(const std::string& path);

  void rescan(const QStringList& paths);
  void vstChanged() W_SIGNAL(vstChanged)

  void processIncomingMessage(const QString& txt);
  void addInvalidVST(const QString& path);
  void addVST(const QString& path, const QJsonObject& json);

  void scanVSTsEvent();

  struct ScanningProcess
  {
    QString path;
    std::unique_ptr<QProcess> process;
    bool scanning{};
    QElapsedTimer timer;
  };

  std::vector<ScanningProcess> m_processes;

  QWebSocketServer m_wsServer;
  Steinberg::Vst::HostApplication m_host;
  ossia::string_map<VST3::Hosting::Module::Ptr> modules;
  std::vector<AvailablePlugin> vst_infos;
};
}
