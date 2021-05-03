#pragma once
#include <Vst3/Plugin.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <ossia/detail/fmt.hpp>
#include <ossia/detail/string_map.hpp>

#include <QElapsedTimer>
#include <QProcess>
#include <QWebSocketServer>

#include <pluginterfaces/vst/ivstmessage.h>

#include <memory>
#include <stdexcept>

namespace vst3
{
struct vst_error : public std::runtime_error
{
  template <typename... Args>
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

struct HostApp final : public Steinberg::Vst::IHostApplication
{
  Steinberg::Vst::PlugInterfaceSupport m_support;
  HostApp() { }
  virtual ~HostApp() { }
  Steinberg::tresult getName(Steinberg::Vst::String128 name) override
  {
    Steinberg::String str("ossia score");
    str.copyTo16(name, 0, 127);
    return Steinberg::kResultTrue;
  }

  Steinberg::tresult
  createInstance(Steinberg::TUID cid, Steinberg::TUID _iid, void** obj) override
  {
    using namespace Steinberg;
    using namespace Steinberg::Vst;
    FUID classID(FUID::fromTUID(cid));
    FUID interfaceID(FUID::fromTUID(_iid));
    if (classID == Vst::IMessage::iid && interfaceID == Vst::IMessage::iid)
    {
      *obj = new HostMessage;
      return kResultTrue;
    }
    else if (
        classID == Vst::IAttributeList::iid
        && interfaceID == Vst::IAttributeList::iid)
    {
      *obj = new Vst::HostAttributeList;
      return kResultTrue;
    }
    *obj = nullptr;
    return kResultFalse;
  }

  Steinberg::tresult queryInterface(const char* _iid, void** obj) override
  {
    using namespace Steinberg;
    using namespace Steinberg::Vst;
    QUERY_INTERFACE(_iid, obj, FUnknown::iid, IHostApplication)
    QUERY_INTERFACE(_iid, obj, IHostApplication::iid, IHostApplication)

    if (m_support.queryInterface(iid, obj) == kResultTrue)
      return kResultOk;

    *obj = nullptr;
    return kResultFalse;
  }

  Steinberg::uint32 addRef() override { return 1; }

  Steinberg::uint32 release() override { return 1; }
};

class ApplicationPlugin
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
  HostApp m_host;
  ossia::string_map<VST3::Hosting::Module::Ptr> modules;
  std::vector<AvailablePlugin> vst_infos;
};
}
