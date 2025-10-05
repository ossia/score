#include "EffectModel.hpp"

#include "Widgets.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Audio/Settings/Model.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Vst/ApplicationPlugin.hpp>
#include <Vst/Commands.hpp>
#include <Vst/Control.hpp>
#include <Vst/Loader.hpp>
#include <Vst/Window.hpp>

#include <score/tools/DeleteAll.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/detail/math.hpp>
#include <ossia/detail/pod_vector.hpp>

#include <ossia-qt/invoke.hpp>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QInputDialog>
#include <QTimer>

#include <websocketpp/base64/base64.hpp>

#include <cmath>
#include <wobjectimpl.h>

#include <memory>
W_OBJECT_IMPL(vst::Model)

// If a VST has less than this many parameters they will be shown by default.
#define VST_DEFAULT_PARAM_NUMBER_CUTOFF 10
namespace Process
{
template <>
QString EffectProcessFactory_T<vst::Model>::customConstructionData() const noexcept
{
  auto& app = score::GUIAppContext().applicationPlugin<vst::ApplicationPlugin>();
  QStringList vsts;
  vsts.reserve(app.vst_infos.size());
  QMap<QString, int32_t> ids;
  for(vst::VSTInfo& i : app.vst_infos)
  {
    if(i.isValid)
    {
      auto name = i.prettyName;
      if(i.isSynth)
        name = "♪ " + name;
      vsts.push_back(name);
      ids.insert(name, i.uniqueID);
    }
  }
  ossia::sort(vsts);
  bool ok = false;
  auto res = QInputDialog::getItem(
      nullptr, QObject::tr("Select a VST plug-in"), QObject::tr("VST plug-in"), vsts, 0,
      false, &ok);
  if(ok)
    return QString::number(ids[res]);
  return {};
}

template <>
Process::Descriptor
EffectProcessFactory_T<vst::Model>::descriptor(QString d) const noexcept
{
  Process::Descriptor desc;
  auto& app = score::GUIAppContext().applicationPlugin<vst::ApplicationPlugin>();

  auto it = ossia::find_if(
      app.vst_infos, [d](const vst::VSTInfo& vst) { return vst.uniqueID == d.toInt(); });
  desc.documentationLink = QUrl(
      "https://ossia.io/score-docs/processes/"
      "audio-plugins.html#common-formats-vst-vst3-lv2-jsfx");
  if(it != app.vst_infos.end())
  {
    desc.prettyName = it->displayName;
    if(desc.prettyName.isEmpty())
      desc.prettyName = it->prettyName;

    desc.author = it->author;

    if(it->isSynth)
    {
      desc.category = Process::ProcessCategory::Synth;

      auto inlets = std::vector<Process::PortType>{Process::PortType::Midi};
      for(int i = 0; i < it->controls; i++)
        inlets.push_back(Process::PortType::Message);
      desc.inlets = std::move(inlets);

      desc.outlets = {std::vector<Process::PortType>{Process::PortType::Audio}};
    }
    else
    {
      desc.category = Process::ProcessCategory::AudioEffect;

      auto inlets = std::vector<Process::PortType>{Process::PortType::Audio};
      for(int i = 0; i < it->controls; i++)
        inlets.push_back(Process::PortType::Message);
      desc.inlets = std::move(inlets);

      desc.outlets = {std::vector<Process::PortType>{Process::PortType::Audio}};
    }
  }
  return desc;
}

template <>
Process::Descriptor EffectProcessFactory_T<vst::Model>::descriptor(
    const Process::ProcessModel& d) const noexcept
{
  return descriptor(d.effect());
}
}

namespace vst
{
QString Model::getString(AEffectOpcodes op, int param)
{
  char paramName[512] = {0};
  dispatch(op, param, 0, paramName);
  return QString::fromUtf8(paramName);
}

Model::Model(
    TimeVal t, const QString& path, const Id<Process::ProcessModel>& id, QObject* parent)
    : ProcessModel{t, id, "vst", parent}
    , m_effectId{path.toInt()}
    , m_registration{*this}
{
  init();
  create();
}

Model::~Model()
{
  // close will set the pointer to nullptr
  if(this->externalUI)
    this->externalUI->close();

  if(fx && fx->fx)
    fx->fx->resvd1 = 0;

  fx.reset();
}

Process::ProcessFlags Model::flags() const noexcept
{
  auto flags = Metadata<Process::ProcessFlags_k, Model>::get();
  if(fx && fx->fx)
  {
    if((fx->fx->numParams >= VST_DEFAULT_PARAM_NUMBER_CUTOFF)
       && (fx->fx->flags & VstAEffectFlags::effFlagsHasEditor))
    {
      flags |= Process::CanCreateControls;
    }
  }
  if(m_createControls)
    flags |= Process::CreateControls;
  return flags;
}

void Model::setCreatingControls(bool ok)
{
  if(ok != m_createControls)
  {
    m_createControls = ok;
    creatingControlsChanged(ok);
  }
}

QString Model::prettyName() const noexcept
{
  return metadata().getLabel();
}

bool Model::hasExternalUI() const noexcept
{
  if(!fx)
    return false;

#if defined(__linux__)
  static const thread_local bool is_wayland = qGuiApp->platformName() == "wayland";
  if(is_wayland)
    return false;
#endif

  return bool(fx->fx->flags & VstAEffectFlags::effFlagsHasEditor);
}

void Model::removeControl(int fxNum)
{
  auto it = controls.find(fxNum);
  SCORE_ASSERT(it != controls.end());
  auto ctrl = it->second;
  controls.erase(it);
  for(auto it = m_inlets.begin(); it != m_inlets.end(); ++it)
  {
    if(*it == ctrl)
    {
      m_inlets.erase(it);
      break;
    }
  }
  controlRemoved(*ctrl);
  delete ctrl;
}

void Model::removeControl(const Id<Process::Port>& id)
{
  auto it = ossia::find_if(m_inlets, [&](const auto& inl) { return inl->id() == id; });

  SCORE_ASSERT(it != m_inlets.end());
  auto ctrl = safe_cast<ControlInlet*>(*it);

  controls.erase(ctrl->fxNum);
  m_inlets.erase(it);

  controlRemoved(*ctrl);
  delete ctrl;
}

ControlInlet* Model::getControl(const Id<Process::Port>& p) const
{
  for(auto e : m_inlets)
    if(e->id() == p)
      return static_cast<ControlInlet*>(e);
  return nullptr;
}

QString Model::effect() const noexcept
{
  return QString::number(this->m_effectId);
}

void Model::init()
{
  //  connect(this, &Model::addControl, this, &Model::on_addControl);

  connect(this, &Model::resetExecution, this, [this] {
    for(auto [index, ctl] : this->controls)
    {
      fx->setParameter(index, ctl->value());
    }
  });
}

void Model::setControlName(int fxnum, ControlInlet* ctrl)
{
  auto name = getString(effGetParamName, fxnum);
  auto label = getString(effGetParamLabel, fxnum);
  // auto display = get_string(effGetParamDisplay, i);

  // Get the name
  QString str = name;
  if(!label.isEmpty())
    str += "(" + label + ")";

  ctrl->setName(name);
}

void Model::on_addControl(int i, float v)
{
  if(controls.find(i) != controls.end())
  {
    return;
  }

  SCORE_ASSERT(controls.find(i) == controls.end());
  auto ctrl = new ControlInlet{Id<Process::Port>(getStrongId(inlets()).val()), this};
  ctrl->displayHandledExplicitly = true;
  ctrl->fxNum = i;
  ctrl->setValue(v);

  // Metadata
  setControlName(i, ctrl);

  on_addControl_impl(ctrl);
}

void Model::on_addControl_impl(ControlInlet* ctrl)
{
  connect(
      ctrl, &ControlInlet::valueChanged, this,
      [this, i = ctrl->fxNum](const ossia::value& newval) {
    on_controlChangedFromScore(i, ossia::convert<float>(newval));
  });

  {
    /*
    VstParameterProperties props;
    auto res = dispatch(effGetParameterProperties, i, 0, &props);
    if(res == 1)
    {
      // apparently there's exactly 0 plug-ins supporting this
      qDebug() << props.label << props.minInteger << props.maxInteger <<
    props.smallStepFloat << props.stepFloat;
    }
    */
  }
  m_inlets.push_back(ctrl);
  SCORE_ASSERT(controls.find(ctrl->fxNum) == controls.end());
  controls.insert({ctrl->fxNum, ctrl});
  SCORE_ASSERT(controls.find(ctrl->fxNum) != controls.end());
  controlAdded(ctrl->id());
  // FIXME inletsChanged
}

void Model::on_controlChangedFromScore(int i, float newval)
{
  if(std::abs(newval - fx->getParameter(i)) > 0.0001)
    fx->setParameter(i, newval);
}

void Model::reloadControls()
{
  if(!fx)
    return;

  for(auto ctrl : controls)
  {
    setControlName(ctrl.first, ctrl.second);
    ctrl.second->setValue(fx->getParameter(ctrl.first));
  }
}

void Model::reloadPrograms()
{
  m_programs.clear();
  for(int i = 0; i < fx->fx->numPrograms; i++)
  {
    char effName[128] = {0};
    fx->dispatch(effGetProgramNameIndexed, i, 0, effName);
    if(effName[0] != '\0')
    {
      m_programs.emplace_back(effName, i);
    }
  }
}

intptr_t vst_host_callback(
    AEffect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr, float opt)
{
  intptr_t result = 0;

  if(effect)
  {
    switch(opcode)
    {
      case audioMasterGetTime: {
        if(auto vst = reinterpret_cast<Model*>(effect->resvd1))
        {
          result = reinterpret_cast<intptr_t>(&vst->fx->info);
        }
        break;
      }
      case audioMasterSizeWindow: {
        auto vst = reinterpret_cast<Model*>(effect->resvd1);
        if(vst && vst->externalUI)
        {
          auto window = ((Window*)vst->externalUI);
          ossia::qt::run_async(
              window, [window, index, value] { window->resize(index, value); });
        }
        result = 1;
        break;
      }

      case audioMasterNeedIdle: {
        effect->dispatcher(effect, effEditIdle, 0, 0, nullptr, 0);
        result = 1;
        break;
      }

      case audioMasterIdle: {
        if(auto vst = reinterpret_cast<Model*>(effect->resvd1))
          vst->needIdle.store(true, std::memory_order_release);
        result = 1;
        break;
      }

      case audioMasterCurrentId:
        result = effect->uniqueID;
        break;

      case audioMasterUpdateDisplay: {
        if(auto vst = reinterpret_cast<Model*>(effect->resvd1))
        {
          ossia::qt::run_async(vst, [vst] {
            vst->reloadControls();
            vst->reloadPrograms();
          });
        }
        break;
      }

      case audioMasterAutomate: {
        if(auto vst = reinterpret_cast<Model*>(effect->resvd1))
        {
          if(vst->flags() & Process::CreateControls)
          {
            ossia::qt::run_async(vst, [vst, index, opt] {
              auto ctrl_it = vst->controls.find(index);
              if(ctrl_it != vst->controls.end())
              {
                ctrl_it->second->setValue(opt);
              }
              else
              {
                auto& ctx = score::IDocument::documentContext(*vst);
                CommandDispatcher<>{ctx.commandStack}.submit<CreateControl>(
                    *vst, index, opt);
              }
            });
          }
        }

        break;
      }
    }
  }

  switch(opcode)
  {
    case audioMasterProcessEvents:
      break;
    case audioMasterIOChanged:
      break;
    case audioMasterSizeWindow:
      result = 1;
      break;
    case audioMasterGetInputLatency: {
      static const auto& context = score::AppContext();
      static const auto& settings = context.settings<Audio::Settings::Model>();
      result = settings.getBufferSize() / double(settings.getRate());
      break;
    }
    case audioMasterGetOutputLatency: {
      static const auto& context = score::AppContext();
      static const auto& settings = context.settings<Audio::Settings::Model>();
      result = settings.getBufferSize() / double(settings.getRate());
      break;
    }
    case audioMasterVersion:
      result = kVstVersion;
      break;
    case audioMasterGetSampleRate: {
      static const auto& context = score::AppContext();
      static const auto& settings = context.settings<Audio::Settings::Model>();
      result = settings.getRate();
      break;
    }
    case audioMasterGetBlockSize: {
      static const auto& context = score::AppContext();
      static const auto& settings = context.settings<Audio::Settings::Model>();
      result = settings.getBufferSize();
      break;
    }
    case audioMasterGetCurrentProcessLevel: {
      static const auto& context = score::GUIAppContext();
      static const auto& plug = context.applicationPlugin<vst::ApplicationPlugin>();
      auto this_t = std::this_thread::get_id();
      if(this_t == plug.mainThreadId())
      {
        result = kVstProcessLevelUser;
      }
      else
      {
        result = kVstProcessLevelRealtime;
      }

      break;
    }
    case audioMasterGetAutomationState:
      result = kVstAutomationUnsupported;
      break;
    case audioMasterGetLanguage:
      result = kVstLangEnglish;
      break;
    case audioMasterGetVendorVersion:
      result = 1;
      break;
    case audioMasterGetVendorString:
      std::copy_n("ossia", 6, static_cast<char*>(ptr));
      result = 1;
      break;
    case audioMasterGetProductString:
      std::copy_n("score", 6, static_cast<char*>(ptr));
      result = 1;
      break;
    case audioMasterBeginEdit:
      break;
    case audioMasterEndEdit:
      // TODO use this to trigger undo-redo commands.
      break;
    case audioMasterOpenFileSelector:
      break;
    case audioMasterCloseFileSelector:
      break;
    case audioMasterCanDo: {
      static const ossia::flat_set<std::string_view> supported{
          HostCanDos::canDoSendVstEvents,
          HostCanDos::canDoSendVstMidiEvent,
          HostCanDos::canDoSendVstTimeInfo,
          HostCanDos::canDoSendVstMidiEventFlagIsRealtime,
          HostCanDos::canDoSizeWindow,
          HostCanDos::canDoHasCockosViewAsConfig};
      if(supported.find(static_cast<const char*>(ptr)) != supported.end())
        result = 1;
      break;
    }
  }
  return result;
}

void Model::closePlugin()
{
  if(fx)
  {
    fx->fx->resvd1 = 0;
    std::atomic_thread_fence(std::memory_order_seq_cst);
    if(externalUI)
    {
      auto w = reinterpret_cast<Window*>(externalUI);
      delete w;
    }

    fx.reset();
  }
  auto old_inlets = score::clearAndDeleteLater(m_inlets);
  auto old_outlets = score::clearAndDeleteLater(m_outlets);
  metadata().setLabel("Dead VST");
}

AEffect* getPluginInstance(const QString& name)
{
  auto& app = score::GUIAppContext().applicationPlugin<vst::ApplicationPlugin>();

  auto info_it = ossia::find_if(
      app.vst_infos, [&](const vst::VSTInfo& i) { return i.prettyName == name; });
  if(info_it != app.vst_infos.end())
  {
    auto it = app.vst_modules.find(info_it->uniqueID);
    if(it != app.vst_modules.end())
    {
      if(!it->second)
        it->second = new vst::Module{info_it->path.toStdString()};

      if(auto m = it->second->getMain())
      {
        return m(vst_host_callback);
      }
    }
    else
    {
      auto plugin = new vst::Module{info_it->path.toStdString()};

      if(auto m = plugin->getMain())
      {
        if(auto p = (AEffect*)m(vst::vst_host_callback))
        {
          app.vst_modules.insert({p->uniqueID, plugin});
          return p;
        }
      }

      delete plugin;
    }
  }

  return nullptr;
}
AEffect* getPluginInstance(int32_t id)
{
  auto& app = score::GUIAppContext().applicationPlugin<vst::ApplicationPlugin>();

  auto info_it = ossia::find_if(
      app.vst_infos, [&](const vst::VSTInfo& i) { return i.uniqueID == id; });
  if(info_it != app.vst_infos.end())
  {
    auto it = app.vst_modules.find(info_it->uniqueID);
    if(it != app.vst_modules.end())
    {
      if(!it->second)
        it->second = new vst::Module{info_it->path.toStdString()};
      if(auto m = it->second->getMain())
      {
        if(auto plug = m(vst_host_callback))
        {
          it->second->use_count++;
          return plug;
        }
      }
      if(it->second && it->second->use_count == 0)
      {
        delete it->second;
        it->second = nullptr;
      }
    }
    else
    {
      auto plugin = new vst::Module{info_it->path.toStdString()};

      if(auto m = plugin->getMain())
      {
        if(auto p = (AEffect*)m(vst::vst_host_callback))
        {
          app.vst_modules.insert({p->uniqueID, plugin});
          return p;
        }
      }

      delete plugin;
    }
  }

  return nullptr;
}

void releasePluginInstance(int uid)
{
  auto& app = score::GUIAppContext().applicationPlugin<vst::ApplicationPlugin>();

  auto it = app.vst_modules.find(uid);
  if(it != app.vst_modules.end())
  {
    if(it->second)
    {
      it->second->use_count--;
      if(it->second->use_count == 0)
      {
        delete it->second;
        it->second = nullptr;
      }
    }
  }
}

void Model::initFx()
{
  fx = std::make_shared<AEffectWrapper>(getPluginInstance(m_effectId));
  if(!fx->fx)
  {
    fx = std::make_shared<AEffectWrapper>(getPluginInstance(metadata().getLabel()));
    if(!fx->fx)
    {
      qDebug() << "plugin was not created";
      fx.reset();
      return;
    }
  }

  fx->fx->resvd1 = reinterpret_cast<intptr_t>(this);

  auto& ctx = score::GUIAppContext();
  auto& media = ctx.settings<Audio::Settings::Model>();
  const int blockSize = media.getBufferSize();

  // JUCE hosts do it before effOpen
  dispatch(effSetSampleRate, 0, media.getRate(), nullptr, media.getRate());
  dispatch(effSetBlockSize, 0, blockSize, nullptr, blockSize);

  dispatch(effOpen);

  // Others like Reaper do it after - this is also done in node.hpp
  dispatch(effSetSampleRate, 0, media.getRate(), nullptr, media.getRate());
  dispatch(effSetBlockSize, 0, blockSize, nullptr, blockSize);

  auto& app = ctx.applicationPlugin<vst::ApplicationPlugin>();
  auto it = ossia::find_if(
      app.vst_infos, [this](auto& i) { return i.uniqueID == fx->fx->uniqueID; });

  if(it != app.vst_infos.end())
  {
    metadata().setName(it->prettyName);
    if(!it->displayName.isEmpty())
      metadata().setLabel(it->displayName);
  }
  else
  {
    qDebug() << "warning! " << fx->fx->uniqueID
             << "not found but plug-in found by name.";
  }

  reloadPrograms();

  if(fx->fx->numPrograms > 0)
    fx->dispatch(effSetProgram, 0, 0);
}

void Model::create()
{
  SCORE_ASSERT(!fx);

  initFx();
  if(!fx)
    return;

  int inlet_i = 0;

  m_inlets.push_back(
      new Process::AudioInlet("Audio In", Id<Process::Port>{inlet_i++}, this));
  if(fx->fx->flags & effFlagsIsSynth)
  {
    m_inlets.push_back(
        new Process::MidiInlet("MIDI In", Id<Process::Port>{inlet_i++}, this));
  }

  if(fx->fx->numParams < VST_DEFAULT_PARAM_NUMBER_CUTOFF
     || !(fx->fx->flags & VstAEffectFlags::effFlagsHasEditor))
  {
    for(int i = 0; i < fx->fx->numParams; i++)
    {
      on_addControl(i, fx->getParameter(i));
    }
  }

  auto out = new Process::AudioOutlet("Audio Out", Id<Process::Port>{}, this);
  out->setPropagate(true);
  m_outlets.push_back(out);
}

void Model::load()
{
  SCORE_ASSERT(!fx);
  initFx();
  if(!fx)
  {
    qDebug() << "not loading (no fx)";
    return;
  }

  const bool isSynth = fx->fx->flags & effFlagsIsSynth;
  for(std::size_t i = VST_FIRST_CONTROL_INDEX(isSynth); i < m_inlets.size(); i++)
  {
    auto inlet = safe_cast<ControlInlet*>(m_inlets[i]);
    int ctrl = inlet->fxNum;

    connect(
        inlet, &ControlInlet::valueChanged, this,
        [this, ctrl](const ossia::value& newval) {
      on_controlChangedFromScore(ctrl, ossia::convert<float>(newval));
    });
    controls.insert({ctrl, inlet});
  }
}

#define SCORE_DATASTREAM_IDENTIFY_VST_CHUNK int32_t(0xABABABAB)
#define SCORE_DATASTREAM_IDENTIFY_VST_PARAMS int32_t(0x10101010)

static void saveVstChunkToDatastream(DataStreamInput& stream, const vst::Model& eff)
{
  if(eff.fx)
  {
    if(eff.fx->fx->flags & effFlagsProgramChunks)
    {
      stream << SCORE_DATASTREAM_IDENTIFY_VST_CHUNK;
      void* ptr{};
      auto res = eff.fx->dispatch(effGetChunk, 0, 0, &ptr, 0.f);
      std::string encoded;
      if(ptr && res > 0)
      {
        encoded.assign((const char*)ptr, res);
      }
      stream << encoded;

      // FIXME the memory should be freed at the next effSuspend / effResume call.
    }
    else
    {
      stream << SCORE_DATASTREAM_IDENTIFY_VST_PARAMS;
      ossia::float_vector arr(eff.fx->fx->numParams);
      for(int i = 0; i < eff.fx->fx->numParams; i++)
        arr[i] = eff.fx->getParameter(i);
      stream << arr;
    }
  }
}

static void loadVstChunkFromDatastream(DataStreamOutput& stream, vst::Model& eff)
{
  int32_t kind = 0;
  stream >> kind;

  // First set the values in the VST
  if(kind == SCORE_DATASTREAM_IDENTIFY_VST_CHUNK)
  {
    std::string chunk;
    stream >> chunk;
    if(!chunk.empty())
    {
      if(eff.fx)
      {
        if(eff.fx->fx->flags & effFlagsProgramChunks)
        {
          eff.fx->dispatch(effSetChunk, 0, chunk.size(), chunk.data(), 0.f);
        }
      }
    }
  }
  else if(kind == SCORE_DATASTREAM_IDENTIFY_VST_PARAMS)
  {
    ossia::float_vector params;
    stream >> params;

    if(eff.fx)
    {
      for(std::size_t i = 0; i < params.size(); i++)
      {
        eff.fx->setParameter(i, params[i]);
      }
    }
  }

  // Then reload our UI controls at the correct values.
  if(eff.fx && eff.fx->fx)
  {
    const bool isSynth = eff.fx->fx->flags & effFlagsIsSynth;
    for(std::size_t i = VST_FIRST_CONTROL_INDEX(isSynth); i < eff.inlets().size(); i++)
    {
      auto inlet = safe_cast<vst::ControlInlet*>(eff.inlets()[i]);
      inlet->setValue(eff.fx->getParameter(inlet->fxNum));
    }
  }
}

void Model::loadPreset(const Process::Preset& preset)
{
  const rapidjson::Document doc = readJson(preset.data);
  if(!doc.IsObject())
    return;
  auto obj = doc.GetObject();

  if(auto it = obj.FindMember("Chunk"); it != obj.MemberEnd())
  {
    QByteArray data = QByteArray::fromBase64(JsonValue{it->value}.toByteArray());
    QDataStream s{data};
    DataStreamOutput out{s};
    loadVstChunkFromDatastream(out, *this);
  }
  else if(auto it = obj.FindMember("ProgramIndex"); it != obj.MemberEnd())
  {
    auto idx = JsonValue{it->value}.toInt();
    if(this->fx)
      this->fx->dispatch(effSetProgram, 0, idx);
  }
}

Process::Preset Model::savePreset() const noexcept
{
  Process::Preset p;
  p.name = this->metadata().getName();
  p.key.key = this->concreteKey();
  p.key.effect = this->effect();

  QByteArray data;
  {
    QDataStream s{&data, QIODevice::WriteOnly};
    DataStreamInput in{s};
    saveVstChunkToDatastream(in, *this);
  }

  JSONReader r;
  r.stream.StartObject();
  r.obj["Chunk"] = data.toBase64();
  r.stream.EndObject();
  p.data = r.toByteArray();
  return p;
}

std::vector<Process::Preset> Model::builtinPresets() const noexcept
{
  if(!fx)
    return {};

  std::vector<Process::Preset> presets;
  for(auto& prog : m_programs)
  {
    Process::Preset p;
    p.key.key = this->static_concreteKey();
    p.key.effect = QString::number(m_effectId);
    p.name = QString::fromStdString(prog.first);
    p.data = QStringLiteral(R"({ "ProgramIndex": %1 } )").arg(prog.second).toLatin1();
    presets.push_back(p);
  }

  return presets;
}

AEffectWrapper::~AEffectWrapper()
{
  if(fx)
  {
    ossia::qt::run_async(QCoreApplication::instance(), [fx = fx] {
      int uid = fx->uniqueID;
      fx->dispatcher(fx, effClose, 0, 0, nullptr, 0.f);

      releasePluginInstance(uid);
    });
  }
}

Model::vst_context_handler::vst_context_handler(Model& self)
    : self{self}
{
  auto& app = score::GUIAppContext().applicationPlugin<vst::ApplicationPlugin>();
  app.registerRunningVST(&self);
}

Model::vst_context_handler::~vst_context_handler()
{
  auto& app = score::GUIAppContext().applicationPlugin<vst::ApplicationPlugin>();
  app.unregisterRunningVST(&self);
  self.closePlugin();
}

}

template <>
void DataStreamReader::read(const vst::Model& eff)
{
  readPorts(*this, eff.m_inlets, eff.m_outlets);
  m_stream << eff.m_effectId;

  saveVstChunkToDatastream(m_stream, eff);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(vst::Model& eff)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);

  m_stream >> eff.m_effectId;

  eff.load();

  loadVstChunkFromDatastream(m_stream, eff);

  checkDelimiter();
}

template <>
void JSONReader::read(const vst::Model& eff)
{
  readPorts(*this, eff.m_inlets, eff.m_outlets);
  obj["EffectId"] = eff.m_effectId;

  if(eff.fx)
  {
    if(eff.fx->fx->flags & effFlagsProgramChunks)
    {
      void* ptr{};
      auto res = eff.fx->dispatch(effGetChunk, 0, 0, &ptr, 0.f);
      if(ptr && res > 0)
      {
        obj["Data"] = websocketpp::base64_encode((const unsigned char*)ptr, res);
      }
    }
    else
    {
      stream.Key("Params");
      stream.StartArray();
      for(int i = 0; i < eff.fx->fx->numParams; i++)
        stream.Double(eff.fx->getParameter(i));
      stream.EndArray();
    }
  }
  else
  {
    // VST could not be loaded, keep its internal data that was previously saved
    // TODO do the same with the DataStream one
    if(!eff.m_backup_chunk.empty())
    {
      obj["Data"] = eff.m_backup_chunk;
    }
    if(!eff.m_backup_float_data.empty())
    {
      stream.Key("Params");
      stream.StartArray();
      for(float param : eff.m_backup_float_data)
        stream.Double(param);
      stream.EndArray();
    }
  }
}

template <>
void JSONWriter::write(vst::Model& eff)
{
  auto it = base.FindMember("EffectId");
  if(it != base.MemberEnd())
  {
    eff.m_effectId = it->value.GetInt();
  }
  else
  {
    auto str = obj["Effect"].toString();

    auto& app = score::GUIAppContext().applicationPlugin<vst::ApplicationPlugin>();
    auto it
        = ossia::find_if(app.vst_infos, [&](const auto& i) { return i.path == str; });
    if(it != app.vst_infos.end())
    {
      eff.m_effectId = it->uniqueID;
    }
  }

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);

  eff.load();

  // Reload controls
  auto data_it = base.FindMember("Data");
  auto params_it = base.FindMember("Params");
  if(eff.fx)
  {
    // Set the parameters in the VST
    if(eff.fx->fx->flags & effFlagsProgramChunks)
    {
      if(data_it != base.MemberEnd())
      {
        auto b64 = websocketpp::base64_decode(JsonValue{data_it->value}.toStdString());
        eff.fx->dispatch(effSetChunk, 0, b64.size(), b64.data(), 0.f);
      }
    }
    else
    {
      if(params_it != base.MemberEnd())
      {
        const auto& arr = params_it->value.GetArray();
        for(std::size_t i = 0; i < arr.Size(); i++)
        {
          eff.fx->setParameter(i, arr[i].GetDouble());
        }
      }
    }

    // Reload the vst parameters in the score UI controls
    const bool isSynth = eff.fx->fx->flags & effFlagsIsSynth;
    for(std::size_t i = VST_FIRST_CONTROL_INDEX(isSynth); i < eff.inlets().size(); i++)
    {
      auto inlet = safe_cast<vst::ControlInlet*>(eff.inlets()[i]);
      inlet->setValue(eff.fx->getParameter(inlet->fxNum));
    }
  }
  else
  {
    // Couldn't load the VST, keep the vst data so that we can resave it
    // TODO do the same with the DataStream one
    if(data_it != base.MemberEnd())
    {
      eff.m_backup_chunk = JsonValue{data_it->value}.toStdString();
    }

    if(params_it != base.MemberEnd())
    {
      const auto& arr = params_it->value.GetArray();
      eff.m_backup_float_data.resize(arr.Size());
      for(std::size_t i = 0; i < arr.Size(); i++)
      {
        eff.m_backup_float_data[i] = arr[i].GetDouble();
      }
    }
  }
}

template <>
void DataStreamReader::read(const vst::ControlInlet& p)
{
  m_stream << p.fxNum << p.m_value;
}
template <>
void DataStreamWriter::write(vst::ControlInlet& p)
{
  m_stream >> p.fxNum >> p.m_value;
}

template <>
void JSONReader::read(const vst::ControlInlet& p)
{
  obj["FxNum"] = p.fxNum;
  obj["Value"] = p.value();
}
template <>
void JSONWriter::write(vst::ControlInlet& p)
{
  p.fxNum = obj["FxNum"].toInt();
  p.setValue(obj["Value"].toDouble());
}
