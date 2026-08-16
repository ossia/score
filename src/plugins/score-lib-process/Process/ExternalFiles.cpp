#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/ExternalFiles.hpp>
#include <Process/Process.hpp>

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Explorer/Commands/Update/UpdateDeviceSettings.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/tools/File.hpp>

#include <QFileInfo>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/network/value/value_conversion.hpp>

namespace Process
{

ExternalFileMap::ExternalFileMap(ExternalFileMapper mapper) noexcept
    : m_mapper{std::move(mapper)}
{
}

ExternalFileMap::~ExternalFileMap()
{
  // Commands nobody claimed would otherwise leak: the caller either takes
  // them (and pushes them on the stack) or the whole run was abandoned.
  for(auto* cmd : m_commands)
    delete cmd;
}

QString ExternalFileMap::map(ExternalFileRef ref)
{
  if(ref.path.isEmpty())
    return {};

  if(ref.owner.isEmpty())
    ref.owner = owner;
  if(ref.kind == score::FileKind::Unknown && !ref.directory)
    ref.kind = score::guessFileKind(ref.path);

  if(!m_mapper || !ref.rewritable)
    return {};

  QString next = m_mapper(ref);
  if(next == ref.path)
    return {};
  return next;
}

void ExternalFileMap::addCommand(score::Command* cmd)
{
  if(cmd)
    m_commands.push_back(cmd);
}

std::vector<score::Command*> ExternalFileMap::takeCommands() noexcept
{
  return std::move(m_commands);
}

void ExternalFileMap::readOnly(const QString& path, score::FileKind kind)
{
  map({.path = path,
       .kind = kind,
       .usage = FileUsage::Input,
       .directory = false,
       .rewritable = false,
       .owner = owner});
}

//! Rewrite a single path held in an ossia::value, in place.
static bool
mapStringValue(ExternalFileMap& self, ossia::value& v, const ExternalFileRef& proto)
{
  auto str = v.target<std::string>();
  if(!str)
    return false;

  const QString cur = QString::fromStdString(*str).trimmed();
  if(cur.isEmpty())
    return false;

  ExternalFileRef ref = proto;
  ref.path = cur;
  const QString next = self.map(std::move(ref));
  if(next.isEmpty())
    return false;

  v = ossia::value{next.toStdString()};
  return true;
}

void ExternalFileMap::control(
    Process::ControlInlet& inlet, score::FileKind kind, FileUsage usage)
{
  const ExternalFileRef proto{
      .path = {},
      .kind = kind,
      .usage = usage,
      .directory = false,
      .rewritable = true,
      .owner = owner};

  ossia::value v = inlet.value();
  if(v.target<std::string>())
  {
    if(mapStringValue(*this, v, proto))
      addCommand(new Process::SetControlValue{inlet, v});
  }
  else if(auto* list = v.target<std::vector<ossia::value>>())
  {
    // Image lists and the like: one control, many paths.
    bool changed = false;
    for(auto& elt : *list)
      changed |= mapStringValue(*this, elt, proto);

    if(changed)
      addCommand(new Process::SetControlValue{inlet, v});
  }
}

void ExternalFileMap::folder(Process::ControlInlet& inlet)
{
  ossia::value v = inlet.value();
  const ExternalFileRef proto{
      .path = {},
      .kind = score::FileKind::Folder,
      .usage = FileUsage::Input,
      .directory = true,
      .rewritable = true,
      .owner = owner};

  if(mapStringValue(*this, v, proto))
    addCommand(new Process::SetControlValue{inlet, v});
}

void ProcessModel::mapExternalFiles(Process::ExternalFileMap& map)
{
  // Every avendish process, and every process built out of the standard
  // widget ports, holds its files here.
  for(auto* inlet : m_inlets)
  {
    if(auto* audio = qobject_cast<Process::AudioFileChooser*>(inlet))
      map.control(*audio, score::FileKind::Audio);
    else if(auto* video = qobject_cast<Process::VideoFileChooser*>(inlet))
      map.control(*video, score::FileKind::Video);
    else if(auto* file = qobject_cast<Process::FileChooserBase*>(inlet))
      map.control(*file, score::FileKind::Unknown);
    else if(auto* dir = qobject_cast<Process::FolderChooser*>(inlet))
      map.folder(*dir);
  }
}

bool looksLikeExistingFile(
    const QString& value, const score::DocumentContext& ctx) noexcept
{
  const QString trimmed = value.trimmed();
  // Cheap rejections first: a script body is long and has newlines, and
  // stat()ing a few kilobytes of QML would be silly.
  if(trimmed.isEmpty() || trimmed.size() > 4096)
    return false;
  if(trimmed.contains('\n') || trimmed.contains('\r'))
    return false;

  return QFileInfo::exists(score::locateFilePath(trimmed, ctx));
}

//! Devices can reference files too (a video-file input, an encoder's output
//! destination). Their settings are opaque QVariants, so the protocol factory
//! is the only thing that can read and rebuild them.
static void mapDeviceExternalFiles(const score::DocumentContext& ctx, ExternalFileMap& map)
{
  auto* devplug = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  if(!devplug)
    return;

  const auto& protocols = ctx.app.interfaces<Device::ProtocolFactoryList>();
  for(auto* dev : devplug->list().devices())
  {
    if(!dev)
      continue;

    const Device::DeviceSettings& settings = dev->settings();
    auto* factory = protocols.get(settings.protocol);
    if(!factory)
      continue;

    map.owner = settings.name;
    const QVariant updated = factory->relocateExternalFiles(
        settings.deviceSpecificSettings,
        [&](const QString& path, score::FileKind kind, bool output) {
      return map.map(
          {.path = path,
           .kind = kind,
           .usage = output ? FileUsage::Output : FileUsage::Input,
           .directory = false,
           .rewritable = true,
           .owner = map.owner});
        });

    if(updated.isValid())
    {
      Device::DeviceSettings next = settings;
      next.deviceSpecificSettings = updated;
      map.addCommand(
          new Explorer::Command::UpdateDeviceSettings{*devplug, settings.name, next});
    }
  }
  map.owner.clear();
}

void mapDocumentExternalFiles(const score::DocumentContext& ctx, ExternalFileMap& map)
{
  // findChildren walks the whole model tree, which is what we want: processes
  // nest (scenario -> interval -> process -> scenario -> ...) and a flat list
  // of the top-level ones would miss most of a real document.
  const auto processes = ctx.document.model().findChildren<Process::ProcessModel*>();
  for(auto* proc : processes)
  {
    map.owner = proc->prettyName();
    proc->mapExternalFiles(map);
  }
  map.owner.clear();

  mapDeviceExternalFiles(ctx, map);
}
}
