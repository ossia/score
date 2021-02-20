#include "EffectModel.hpp"

#include "Widgets.hpp"

#include <Vst/ApplicationPlugin.hpp>
#include <Vst/Commands.hpp>
#include <Vst/Control.hpp>
#include <Vst/Loader.hpp>
#include <Vst/Window.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/tools/DeleteAll.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/pod_vector.hpp>

#include <QInputDialog>
#include <QTimer>

#include <Audio/Settings/Model.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <cmath>
#include <websocketpp/base64/base64.hpp>
#include <wobjectimpl.h>

#include <memory>
#include <set>
W_OBJECT_IMPL(vst::Model)

// If a VST has less than this many parameters they will be shown by default.
#define VST_DEFAULT_PARAM_NUMBER_CUTOFF 10
namespace Process
{
template <>
QString EffectProcessFactory_T<vst::Model>::customConstructionData() const
{
  auto& app = score::GUIAppContext().applicationPlugin<vst::ApplicationPlugin>();
  QStringList vsts;
  vsts.reserve(app.vst_infos.size());
  QMap<QString, int32_t> ids;
  for (vst::VSTInfo& i : app.vst_infos)
  {
    if (i.isValid)
    {
      auto name = i.prettyName;
      if (i.isSynth)
        name = "♪ " + name;
      vsts.push_back(name);
      ids.insert(name, i.uniqueID);
    }
  }
  ossia::sort(vsts);
  bool ok = false;
  auto res = QInputDialog::getItem(
      nullptr,
      QObject::tr("Select a VST plug-in"),
      QObject::tr("VST plug-in"),
      vsts,
      0,
      false,
      &ok);
  if (ok)
    return QString::number(ids[res]);
  return {};
}

template <>
Process::Descriptor EffectProcessFactory_T<vst::Model>::descriptor(QString d) const
{
  Process::Descriptor desc;
  auto& app = score::GUIAppContext().applicationPlugin<vst::ApplicationPlugin>();

  auto it = ossia::find_if(app.vst_infos, [=](const vst::VSTInfo& vst) {
    return vst.uniqueID == d.toInt();
  });
  if (it != app.vst_infos.end())
  {
    desc.prettyName = it->displayName;
    desc.author = it->author;

    if (it->isSynth)
    {
      desc.category = Process::ProcessCategory::Synth;

      auto inlets = std::vector<Process::PortType>{Process::PortType::Midi};
      for (int i = 0; i < it->controls; i++)
        inlets.push_back(Process::PortType::Message);
      desc.inlets = std::move(inlets);

      desc.outlets = {std::vector<Process::PortType>{Process::PortType::Audio}};
    }
    else
    {
      desc.category = Process::ProcessCategory::AudioEffect;

      auto inlets = std::vector<Process::PortType>{Process::PortType::Audio};
      for (int i = 0; i < it->controls; i++)
        inlets.push_back(Process::PortType::Message);
      desc.inlets = std::move(inlets);

      desc.outlets = {std::vector<Process::PortType>{Process::PortType::Audio}};
    }
  }
  return desc;
}
}

namespace vst
{
Model::Model(
    TimeVal t,
    const QString& path,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : ProcessModel{t, id, "vst", parent}, m_effectId{path.toInt()}
{
  init();
  create();
}

Model::~Model()
{
  closePlugin();
}

QString Model::prettyName() const noexcept
{
  return metadata().getLabel();
}

bool Model::hasExternalUI() const noexcept
{
  if (!fx)
    return false;
  return bool(fx->fx->flags & VstAEffectFlags::effFlagsHasEditor);
}

void Model::removeControl(int fxNum)
{
  qDebug() << "removeControl(int) " << fxNum;
  auto it = controls.find(fxNum);
  SCORE_ASSERT(it != controls.end());
  auto ctrl = it->second;
  controls.erase(it);
  for (auto it = m_inlets.begin(); it != m_inlets.end(); ++it)
  {
    if (*it == ctrl)
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

  qDebug() << "removeControl(id) " << ctrl->fxNum;
  controls.erase(ctrl->fxNum);
  m_inlets.erase(it);

  controlRemoved(*ctrl);
  delete ctrl;
}

ControlInlet* Model::getControl(const Id<Process::Port>& p) const
{
  for (auto e : m_inlets)
    if (e->id() == p)
      return static_cast<ControlInlet*>(e);
  return nullptr;
}

void Model::init()
{
//  connect(this, &VSTEffectModel::addControl, this, &VSTEffectModel::on_addControl);
}

void Model::on_addControl(int i, float v)
{
  if(controls.find(i) != controls.end())
  {
    return;
  }

  SCORE_ASSERT(controls.find(i) == controls.end());
  auto ctrl = new ControlInlet{Id<Process::Port>(getStrongId(inlets()).val()), this};
  ctrl->hidden = true;
  ctrl->fxNum = i;
  ctrl->setValue(v);

  // Metadata
  {
    auto name = getString(effGetParamName, i);
    auto label = getString(effGetParamLabel, i);
    // auto display = get_string(effGetParamDisplay, i);

    // Get the nameq
    QString str = name;
    if (!label.isEmpty())
      str += "(" + label + ")";

    ctrl->setCustomData(name);
  }

  on_addControl_impl(ctrl);
}

void Model::on_addControl_impl(ControlInlet* ctrl)
{
  connect(ctrl, &ControlInlet::valueChanged, this, [this, i = ctrl->fxNum](float newval) {
    if (std::abs(newval - fx->getParameter(i)) > 0.0001)
      fx->setParameter(i, newval);
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
}

void Model::reloadControls()
{
  if (!fx)
    return;

  for (auto ctrl : controls)
  {
    ctrl.second->setValue(fx->getParameter(ctrl.first));
  }
}

QString Model::getString(AEffectOpcodes op, int param)
{
  char paramName[512] = {0};
  dispatch(op, param, 0, paramName);
  return QString::fromUtf8(paramName);
}

intptr_t vst_host_callback(
    AEffect* effect,
    int32_t opcode,
    int32_t index,
    intptr_t value,
    void* ptr,
    float opt)
{
  intptr_t result = 0;

  if (effect)
  {
    switch (opcode)
    {
      case audioMasterGetTime:
      {
        auto vst = reinterpret_cast<Model*>(effect->resvd1);
        if (vst)
        {
          result = reinterpret_cast<intptr_t>(&vst->fx->info);
        }
        break;
      }
      case audioMasterSizeWindow:
      {
        auto vst = reinterpret_cast<Model*>(effect->resvd1);
        if (vst && vst->externalUI)
        {
          auto window = ((Window*)vst->externalUI);
          ossia::qt::run_async(window, [=] { window->resize(index, value); });
        }
        result = 1;
        break;
      }
      case audioMasterNeedIdle:
        break;

      case audioMasterIdle:
        effect->dispatcher(effect, effEditIdle, 0, 0, nullptr, 0);
        break;

      case audioMasterCurrentId:
        result = effect->uniqueID;
        break;

      case audioMasterUpdateDisplay:
      {
        auto vst = reinterpret_cast<Model*>(effect->resvd1);
        if (vst)
        {
          vst->reloadControls();
        }
        break;
      }

      case audioMasterAutomate:
      {
        auto vst = reinterpret_cast<Model*>(effect->resvd1);
        if (vst)
        {
          ossia::qt::run_async(vst, [=] {
            auto ctrl_it = vst->controls.find(index);
            if (ctrl_it != vst->controls.end())
            {
              ctrl_it->second->setValue(opt);
            }
            else
            {
              auto& ctx = score::IDocument::documentContext(*vst);
              CommandDispatcher<>{ctx.commandStack}.submit<CreateControl>(*vst, index, opt);
            }
          });
        }

        break;
      }
    }
  }

  switch (opcode)
  {
    case audioMasterProcessEvents:
      break;
    case audioMasterIOChanged:
      break;
    case audioMasterSizeWindow:
      result = 1;
      break;
    case audioMasterGetInputLatency:
    {
      static const auto& context = score::AppContext();
      static const auto& settings = context.settings<Audio::Settings::Model>();
      result = settings.getBufferSize() / double(settings.getRate());
      break;
    }
    case audioMasterGetOutputLatency:
    {
      static const auto& context = score::AppContext();
      static const auto& settings = context.settings<Audio::Settings::Model>();
      result = settings.getBufferSize() / double(settings.getRate());
      break;
    }
    case audioMasterVersion:
      result = kVstVersion;
      break;
    case audioMasterGetSampleRate:
    {
      static const auto& context = score::AppContext();
      static const auto& settings = context.settings<Audio::Settings::Model>();
      result = settings.getRate();
      break;
    }
    case audioMasterGetBlockSize:
    {
      static const auto& context = score::AppContext();
      static const auto& settings = context.settings<Audio::Settings::Model>();
      result = settings.getBufferSize();
      break;
    }
    case audioMasterGetCurrentProcessLevel:
    {
      static const auto& context = score::GUIAppContext();
      static const auto& plug = context.applicationPlugin<vst::ApplicationPlugin>();
      auto this_t = std::this_thread::get_id();
      if (this_t == plug.mainThreadId())
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
    case audioMasterCanDo:
    {
      static const std::set<std::string_view> supported{
          HostCanDos::canDoSendVstEvents,
          HostCanDos::canDoSendVstMidiEvent,
          HostCanDos::canDoSendVstTimeInfo,
          HostCanDos::canDoSendVstMidiEventFlagIsRealtime,
          HostCanDos::canDoSizeWindow,
          HostCanDos::canDoHasCockosViewAsConfig};
      if (supported.find(static_cast<const char*>(ptr)) != supported.end())
        result = 1;
      break;
    }
  }
  return result;
}

void Model::closePlugin()
{
  if (fx)
  {
    if (externalUI)
    {
      auto w = reinterpret_cast<Window*>(externalUI);
      delete w;
    }
    fx = nullptr;
  }
  auto old_inlets = score::clearAndDeleteLater(m_inlets);
  auto old_outlets = score::clearAndDeleteLater(m_outlets);
  metadata().setLabel("Dead VST");
}

AEffect* getPluginInstance(const QString& name)
{
  auto& app = score::GUIAppContext().applicationPlugin<vst::ApplicationPlugin>();

  auto info_it = ossia::find_if(app.vst_infos, [&](const vst::VSTInfo& i) {
    return i.prettyName == name;
  });
  if (info_it != app.vst_infos.end())
  {
    auto it = app.vst_modules.find(info_it->uniqueID);
    if (it != app.vst_modules.end())
    {
      if (!it->second)
        it->second = new vst::Module{info_it->path.toStdString()};

      if (auto m = it->second->getMain())
      {
        return m(vst_host_callback);
      }
    }
    else
    {
      auto plugin = new vst::Module{info_it->path.toStdString()};

      if (auto m = plugin->getMain())
      {
        if (auto p = (AEffect*)m(vst::vst_host_callback))
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

  auto info_it = ossia::find_if(app.vst_infos, [&](const vst::VSTInfo& i) {
    return i.uniqueID == id;
  });
  if (info_it != app.vst_infos.end())
  {
    auto it = app.vst_modules.find(info_it->uniqueID);
    if (it != app.vst_modules.end())
    {
      if (!it->second)
        it->second = new vst::Module{info_it->path.toStdString()};
      if (auto m = it->second->getMain())
      {
        return m(vst_host_callback);
      }
    }
    else
    {
      auto plugin = new vst::Module{info_it->path.toStdString()};

      if (auto m = plugin->getMain())
      {
        if (auto p = (AEffect*)m(vst::vst_host_callback))
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

void Model::initFx()
{
  fx = std::make_shared<AEffectWrapper>(getPluginInstance(m_effectId));
  if (!fx->fx)
  {
    fx = std::make_shared<AEffectWrapper>(getPluginInstance(metadata().getLabel()));
    if (!fx->fx)
    {
      qDebug() << "plugin was not created";
      fx.reset();
      return;
    }
  }

  fx->fx->resvd1 = reinterpret_cast<intptr_t>(this);

  auto& ctx = score::GUIAppContext();
  auto& media = ctx.settings<Audio::Settings::Model>();
  dispatch(effSetSampleRate, 0, media.getRate(), nullptr, media.getRate());
  dispatch(effSetBlockSize, 0, 4096, nullptr, 4096);
  dispatch(effOpen);

  auto& app = ctx.applicationPlugin<vst::ApplicationPlugin>();
  auto it = ossia::find_if(app.vst_infos, [=](auto& i) { return i.uniqueID == fx->fx->uniqueID; });
  SCORE_ASSERT(it != app.vst_infos.end());
  metadata().setLabel(it->prettyName);
}

void Model::create()
{
  SCORE_ASSERT(!fx);

  initFx();
  if (!fx)
    return;

  int inlet_i = 0;

  m_inlets.push_back(new Process::AudioInlet(Id<Process::Port>{inlet_i++}, this));
  if (fx->fx->flags & effFlagsIsSynth)
  {
    m_inlets.push_back(new Process::MidiInlet(Id<Process::Port>{inlet_i++}, this));
  }

  if (fx->fx->numParams < VST_DEFAULT_PARAM_NUMBER_CUTOFF || !(fx->fx->flags & VstAEffectFlags::effFlagsHasEditor))
  {
    for (int i = 0; i < fx->fx->numParams; i++)
    {
      on_addControl(i, fx->getParameter(i));
    }
  }

  auto out = new Process::AudioOutlet(Id<Process::Port>{}, this);
  out->setPropagate(true);
  m_outlets.push_back(out);
}

void Model::load()
{
  SCORE_ASSERT(!fx);
  initFx();
  if (!fx)
  {
    qDebug() << "not loading (no fx)";
    return;
  }

  const bool isSynth = fx->fx->flags & effFlagsIsSynth;
  for (std::size_t i = VST_FIRST_CONTROL_INDEX(isSynth); i < m_inlets.size(); i++)
  {
    auto inlet = safe_cast<ControlInlet*>(m_inlets[i]);
    int ctrl = inlet->fxNum;

    connect(inlet, &ControlInlet::valueChanged, this, [this, ctrl](float newval) {
      if (std::abs(newval - fx->getParameter(ctrl)) > 0.0001)
        fx->setParameter(ctrl, newval);
    });
    controls.insert({ctrl, inlet});
  }
}
}

#define SCORE_DATASTREAM_IDENTIFY_VST_CHUNK int32_t(0xABABABAB)
#define SCORE_DATASTREAM_IDENTIFY_VST_PARAMS int32_t(0x10101010)
template <>
void DataStreamReader::read(const vst::Model& eff)
{
  m_stream << eff.m_effectId;

  if (eff.fx)
  {
    if (eff.fx->fx->flags & effFlagsProgramChunks)
    {
      m_stream << SCORE_DATASTREAM_IDENTIFY_VST_CHUNK;
      void* ptr{};
      auto res = eff.fx->dispatch(effGetChunk, 0, 0, &ptr, 0.f);
      std::string encoded;
      if (ptr && res > 0)
      {
        encoded.assign((const char*)ptr, res);
      }
      m_stream << encoded;
    }
    else
    {
      m_stream << SCORE_DATASTREAM_IDENTIFY_VST_PARAMS;
      ossia::float_vector arr(eff.fx->fx->numParams);
      for (int i = 0; i < eff.fx->fx->numParams; i++)
        arr[i] = eff.fx->getParameter(i);
      m_stream << arr;
    }
  }

  readPorts(*this, eff.m_inlets, eff.m_outlets);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(vst::Model& eff)
{
  m_stream >> eff.m_effectId;
  int32_t kind = 0;
  m_stream >> kind;

  if (kind == SCORE_DATASTREAM_IDENTIFY_VST_CHUNK)
  {
    std::string chunk;
    m_stream >> chunk;
    if (!chunk.empty())
    {
      QPointer<vst::Model> ptr = &eff;
      QTimer::singleShot(1000, [chunk = std::move(chunk), ptr]() mutable {
        if (!ptr)
          return;
        auto& eff = *ptr;
        if (eff.fx)
        {
          if (eff.fx->fx->flags & effFlagsProgramChunks)
          {
            eff.fx->dispatch(effSetChunk, 0, chunk.size(), chunk.data(), 0.f);

            const bool isSynth = eff.fx->fx->flags & effFlagsIsSynth;
            for (std::size_t i = VST_FIRST_CONTROL_INDEX(isSynth); i < eff.inlets().size(); i++)
            {
              auto inlet = safe_cast<vst::ControlInlet*>(eff.inlets()[i]);
              inlet->setValue(eff.fx->getParameter(inlet->fxNum));
            }
          }
        }
      });
    }
  }
  else if (kind == SCORE_DATASTREAM_IDENTIFY_VST_PARAMS)
  {
    ossia::float_vector params;
    m_stream >> params;

    QPointer<vst::Model> ptr = &eff;
    QTimer::singleShot(1000, &eff, [params = std::move(params), ptr] {
      if (!ptr)
        return;
      auto& eff = *ptr;
      if (eff.fx)
      {
        for (std::size_t i = 0; i < params.size(); i++)
        {
          eff.fx->setParameter(i, params[i]);
        }
      }
    });
  }

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets, eff.m_outlets, &eff);

  eff.load();
  checkDelimiter();
}

template <>
void JSONReader::read(const vst::Model& eff)
{
  readPorts(*this, eff.m_inlets, eff.m_outlets);
  obj["EffectId"] = eff.m_effectId;

  if (eff.fx)
  {
    if (eff.fx->fx->flags & effFlagsProgramChunks)
    {
      void* ptr{};
      auto res = eff.fx->dispatch(effGetChunk, 0, 0, &ptr, 0.f);
      if (ptr && res > 0)
      {
        auto encoded = websocketpp::base64_encode((const unsigned char*)ptr, res);
        obj["Data"] = QString::fromStdString(encoded);
      }
    }
    else
    {
      stream.Key("Params");
      stream.StartArray();
      for (int i = 0; i < eff.fx->fx->numParams; i++)
        stream.Double(eff.fx->getParameter(i));
      stream.EndArray();
    }
  }
}

template <>
void JSONWriter::write(vst::Model& eff)
{
  auto it = base.FindMember("EffectId");
  if (it != base.MemberEnd())
  {
    eff.m_effectId = it->value.GetInt();
  }
  else
  {
    auto str = obj["Effect"].toString();

    auto& app = score::GUIAppContext().applicationPlugin<vst::ApplicationPlugin>();
    auto it = ossia::find_if(app.vst_infos, [&](const auto& i) { return i.path == str; });
    if (it != app.vst_infos.end())
    {
      eff.m_effectId = it->uniqueID;
    }
  }

  QPointer<vst::Model> ptr = &eff;
#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
  QTimer::singleShot(1000, &eff, [base_ptr = std::make_shared<rapidjson::Document>(clone(this->base)), ptr] {
    auto& base = *base_ptr;
#else
  QTimer::singleShot(1000, &eff, [base = clone(this->base), ptr] {
#endif
    if (!ptr)
      return;
    auto& eff = *ptr;
    if (eff.fx)
    {
      if (eff.fx->fx->flags & effFlagsProgramChunks)
      {
        auto it = base.FindMember("Data");
        if (it != base.MemberEnd())
        {
          auto b64 = websocketpp::base64_decode(JsonValue{it->value}.toStdString());
          eff.fx->dispatch(effSetChunk, 0, b64.size(), b64.data(), 0.f);

          const bool isSynth = eff.fx->fx->flags & effFlagsIsSynth;
          for (std::size_t i = VST_FIRST_CONTROL_INDEX(isSynth); i < eff.inlets().size(); i++)
          {
            auto inlet = safe_cast<vst::ControlInlet*>(eff.inlets()[i]);
            inlet->setValue(eff.fx->getParameter(inlet->fxNum));
          }
        }
      }
      else
      {
        auto it = base.FindMember("Params");
        if (it != base.MemberEnd())
        {
          const auto& arr = it->value.GetArray();
          for (std::size_t i = 0; i < arr.Size(); i++)
          {
            eff.fx->setParameter(i, arr[i].GetDouble());
          }
        }
      }
    }
  });

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets, eff.m_outlets, &eff);

  eff.load();
}

template <>
void DataStreamReader::read<vst::ControlInlet>(const vst::ControlInlet& p)
{
  m_stream << p.fxNum;
}
template <>
void DataStreamWriter::write<vst::ControlInlet>(vst::ControlInlet& p)
{
  m_stream >> p.fxNum;
}

template <>
void JSONReader::read<vst::ControlInlet>(const vst::ControlInlet& p)
{
  obj["FxNum"] = p.fxNum;
  obj["Value"] = p.value();
}
template <>
void JSONWriter::write<vst::ControlInlet>(vst::ControlInlet& p)
{
  p.fxNum = obj["FxNum"].toInt();
  p.setValue(obj["Value"].toDouble());
}
