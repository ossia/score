
#include "EffectModel.hpp"

#include "Node.hpp"

#include <Audio/Settings/Model.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Vst3/ApplicationPlugin.hpp>
#include <Vst3/Control.hpp>
#include <Vst3/DataStream.hpp>
#include <Vst3/Executor.hpp>
#include <Vst3/UI/Window.hpp>

#include <score/model/ComponentUtils.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/std/String.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/pod_vector.hpp>

#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <public.sdk/source/vst/hosting/module.h>

#include <QInputDialog>
#include <QTimer>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <cmath>
#include <pluginterfaces/vst/ivstprocesscontext.h>
#include <websocketpp/base64/base64.hpp>
#include <wobjectimpl.h>

#include <memory>
#include <set>

W_OBJECT_IMPL(vst3::Model)
namespace Process
{
template <>
QString EffectProcessFactory_T<vst3::Model>::customConstructionData() const
{ /*
  auto& app = score::GUIAppContext().applicationPlugin<Media::ApplicationPlugin>();
  QStringList vsts;
  vsts.reserve(app.vst_infos.size());
  QMap<QString, int32_t> ids;
  for (Media::ApplicationPlugin::vst_info& i : app.vst_infos)
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
    */
  return {};
}

template <>
Process::Descriptor
EffectProcessFactory_T<vst3::Model>::descriptor(QString d) const
{
  Process::Descriptor desc;

  auto& app = score::GUIAppContext().applicationPlugin<vst3::ApplicationPlugin>();
  auto uid = VST3::UID::fromString(d.toStdString());
  if(!uid)
    return desc;
  const auto& [plug, cls] = app.classInfo(*uid);

  desc.prettyName = QString::fromStdString(cls->name());
  desc.author = QString::fromStdString(cls->vendor());

  if (ossia::contains(cls->subCategories(), Steinberg::Vst::PlugType::kInstrument))
  {
    desc.category = Process::ProcessCategory::Synth;
  }
  else
  {
    desc.category = Process::ProcessCategory::AudioEffect;
  }
  return desc;
}
}

namespace vst3
{

Model::Model(
    TimeVal t,
    const QString& uid,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : ProcessModel{t, id, "VST", parent}
{
  if(auto id = VST3::UID::fromString(uid.toStdString()))
  {
    m_uid = *id;
    auto& vst_plug = score::GUIAppContext().applicationPlugin<vst3::ApplicationPlugin>();
    m_vstPath = vst_plug.pathForClass(m_uid);
  }
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
  return fx.hasUI;
}

void Model::removeControl(Steinberg::Vst::ParamID fxNum)
{
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

void Model::addControlFromEditor(
    Steinberg::Vst::ParamID id)
{
  auto it = m_paramToIndex.find(id);
  if(it == m_paramToIndex.end())
    return;

  Steinberg::Vst::ParameterInfo p;
  if(fx.controller->getParameterInfo(it->second, p) == Steinberg::kResultOk)
  {
    on_addControl(p);
  }
}

void Model::on_addControl(const Steinberg::Vst::ParameterInfo& v)
{
  if (controls.find(v.id) != controls.end())
  {
    return;
  }

  SCORE_ASSERT(controls.find(v.id) == controls.end());
  auto ctrl
      = new ControlInlet{Id<Process::Port>(getStrongId(inlets()).val()), this};
  ctrl->hidden = true;
  ctrl->fxNum = v.id;
  ctrl->setValue(v.defaultNormalizedValue);
  ctrl->setName(fromString(v.title));

  on_addControl_impl(ctrl);
}

void Model::removeControl(const Id<Process::Port>& id)
{
  auto it = ossia::find_if(
      m_inlets, [&](const auto& inl) { return inl->id() == id; });

  SCORE_ASSERT(it != m_inlets.end());
  auto ctrl = safe_cast<ControlInlet*>(*it);

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

QString Model::effect() const noexcept
{
  return QString::fromStdString(m_uid.toString());
}

void Model::init() { }

void Model::on_addControl_impl(ControlInlet* ctrl)
{
  m_inlets.push_back(ctrl);
  initControl(ctrl);
  controlAdded(ctrl->id());
}

void Model::initControl(ControlInlet* ctrl)
{
  ctrl->setValue(fx.controller->getParamNormalized(ctrl->fxNum));
  connect(
      ctrl,
      &vst3::ControlInlet::valueChanged,
      this,
      [this, i = ctrl->fxNum](float newval) {
        auto& c = *fx.controller;
        if (std::abs(newval - c.getParamNormalized(i)) > 0.0001)
        {
          c.setParamNormalized(i, newval);
        }
      });

  SCORE_ASSERT(controls.find(ctrl->fxNum) == controls.end());
  controls.insert({ctrl->fxNum, ctrl});
  SCORE_ASSERT(controls.find(ctrl->fxNum) != controls.end());
}

void Model::reloadControls()
{
  /*
  if (!fx)
    return;

  for (auto ctrl : controls)
  {
    ctrl.second->setValue(fx->getParameter(ctrl.first));
  }
  */
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
    fx.stop();
  }

  auto old_inlets = score::clearAndDeleteLater(m_inlets);
  auto old_outlets = score::clearAndDeleteLater(m_outlets);
  metadata().setLabel("Dead VST");
}

void Model::initFx()
{
  auto& ctx = score::IDocument::documentContext(*this);
  auto& p = ctx.app.applicationPlugin<vst3::ApplicationPlugin>();
  auto& media = ctx.app.settings<Audio::Settings::Model>();

  try
  {
    this->fx.load(
        *this,
        p,
        m_vstPath.toStdString(),
        m_uid,
        media.getRate(),
        4096);
  }
  catch (std::exception& e)
  {
    qDebug() << e.what();
    this->fx = {};
    return;
  }

  if(auto [plug, info] = p.classInfo(m_uid); info)
    metadata().setName(QString::fromStdString(info->name()));
  metadata().setLabel(metadata().getName());

  /// this->fx.controller->setComponentState(...);
}

struct BusActivationVisitor
{
  Plugin& fx;

  void audioIn(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    if (bus.flags & Steinberg::Vst::BusInfo::kDefaultActive)
    {
      fx.component->activateBus(
          Steinberg::Vst::kAudio, Steinberg::Vst::kInput, idx, true);
    }
  }
  void eventIn(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    if (bus.flags & Steinberg::Vst::BusInfo::kDefaultActive)
    {
      fx.component->activateBus(
          Steinberg::Vst::kEvent, Steinberg::Vst::kInput, idx, true);
    }
  }

  void audioOut(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    if (bus.flags & Steinberg::Vst::BusInfo::kDefaultActive)
    {
      fx.component->activateBus(
          Steinberg::Vst::kAudio, Steinberg::Vst::kOutput, idx, true);
    }
  }

  void eventOut(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    if (bus.flags & Steinberg::Vst::BusInfo::kDefaultActive)
    {
      fx.component->activateBus(
          Steinberg::Vst::kEvent, Steinberg::Vst::kOutput, idx, true);
    }
  }
};

struct PortCreationVisitor
{
  Model& model;
  Plugin& fx;

  int inlet_i = 0;
  int outlet_i = 0;

  void audioIn(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    BusActivationVisitor{fx}.audioIn(bus, idx);

    auto port = new Process::AudioInlet(Id<Process::Port>{inlet_i++}, &model);
    port->setName(fromString(bus.name));
    model.m_inlets.push_back(port);
  }
  void eventIn(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    BusActivationVisitor{fx}.eventIn(bus, idx);

    auto port = new Process::MidiInlet(Id<Process::Port>{inlet_i++}, &model);
    port->setName(fromString(bus.name));
    model.m_inlets.push_back(port);
  }

  void audioOut(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    BusActivationVisitor{fx}.audioOut(bus, idx);

    auto port
        = new Process::AudioOutlet(Id<Process::Port>{outlet_i++}, &model);
    port->setName(fromString(bus.name));
    model.m_outlets.push_back(port);

    if (idx == 0)
      port->setPropagate(true);
  }

  void eventOut(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    BusActivationVisitor{fx}.eventOut(bus, idx);

    auto port = new Process::MidiOutlet(Id<Process::Port>{outlet_i++}, &model);
    port->setName(fromString(bus.name));
    model.m_outlets.push_back(port);
  }
};

void Model::mapAllControls(int numParams)
{
  Steinberg::Vst::ParameterInfo p;
  for (int i = 0; i < numParams; i++)
  {
    if (fx.controller->getParameterInfo(i, p) == Steinberg::kResultOk)
    {
      m_paramToIndex[p.id] = i;
    }
  }
}

void Model::create()
{
  SCORE_ASSERT(!fx);

  initFx();
  if (!fx)
    return;

  forEachBus(PortCreationVisitor{*this, fx}, *fx.component);

  if (fx.controller)
  {
    fx.loadProcessorStateToController();

    const int numParams = fx.controller->getParameterCount();
    if (numParams < VST_DEFAULT_PARAM_NUMBER_CUTOFF || !fx.view)
    {
      Steinberg::Vst::ParameterInfo p;
      for (int i = 0; i < numParams; i++)
      {
        if (fx.controller->getParameterInfo(i, p) == Steinberg::kResultOk)
        {
          m_paramToIndex[p.id] = i;
          on_addControl(p);
        }
      }
    }
    else
    {
      mapAllControls(numParams);
    }
  }
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

  forEachBus(BusActivationVisitor{fx}, *fx.component);

  writeState();

  // fx.loadProcessorStateToController();

  mapAllControls(fx.controller->getParameterCount());

  for (auto* inlet : this->m_inlets)
  {
    if (auto ctl = qobject_cast<ControlInlet*>(inlet))
    {
      initControl(ctl);
    }
  }
}

QByteArray Model::readProcessorState() const
{
  if (fx.component)
  {
    QByteArray vstData;
    QDataStream str{&vstData, QIODevice::ReadWrite};
    Vst3DataStream stream{str};
    fx.component->getState(&stream);
    return vstData;
  }
  else
  {
    return m_savedProcessorState;
  }
}

QByteArray Model::readControllerState() const
{
  if (fx.controller)
  {
    QByteArray vstData;
    QDataStream str{&vstData, QIODevice::ReadWrite};
    Vst3DataStream stream{str};
    fx.controller->getState(&stream);
    return vstData;
  }
  else
  {
    return m_savedControllerState;
  }
}

void Model::writeState()
{
  if (fx.component)
  {
    // First reload the processor state into the processor.
    {
      QDataStream str{m_savedProcessorState};

      Vst3DataStream stream{str};
      fx.component->setState(&stream);

      // Then into the controller
      if (fx.controller)
      {
        QDataStream str{m_savedProcessorState};
        Vst3DataStream stream{str};
        fx.controller->setComponentState(&stream);
      }
      m_savedProcessorState = {};
    }

    // Then reload the controller-specific data
    if (fx.controller)
    {
      if(!m_savedControllerState.isEmpty())
      {
        QDataStream str{m_savedControllerState};

        Vst3DataStream stream{str};

        fx.controller->setState(&stream);

        m_savedControllerState = {};
      }
    }
  }
}


void Model::loadPreset(const Process::Preset& preset)
{
  const rapidjson::Document doc = readJson(preset.data);
  if(!doc.IsObject())
    return;
  auto obj = doc.GetObject();

  if(auto it = obj.FindMember("Processor"); it != obj.MemberEnd())
    m_savedProcessorState = QByteArray::fromBase64(JsonValue{it->value}.toByteArray());
  if(auto it = obj.FindMember("Controller"); it != obj.MemberEnd())
    m_savedControllerState = QByteArray::fromBase64(JsonValue{it->value}.toByteArray());

  writeState();

  for (auto* inlet : this->m_inlets)
  {
    if (auto ctl = qobject_cast<ControlInlet*>(inlet))
    {
      ctl->setValue(fx.controller->getParamNormalized(ctl->fxNum));
    }
  }
}

Process::Preset Model::savePreset() const noexcept
{
  Process::Preset p;
  p.name = this->metadata().getName();
  p.key.key = this->concreteKey();
  p.key.effect = this->effect();

  JSONReader r;
  r.stream.StartObject();
  r.obj["Processor"] = readProcessorState().toBase64();
  r.obj["Controller"] = readControllerState().toBase64();
  r.stream.EndObject();
  p.data = r.toByteArray();
  return p;
}

}

struct InputCopier
{
  Steinberg::Vst::IComponent* component{};
  Steinberg::Vst::IAudioProcessor* processor{};
  Steinberg::Vst::IEditController* controller{};

  Steinberg::Vst::ProcessData m_vstData;

  vst3::param_changes m_inputChanges;
  vst3::param_changes m_outputChanges;

  InputCopier(const vst3::Plugin& p)
      : component{p.component}
      , processor{p.processor}
      , controller{p.controller}
  {
    m_vstData.processMode = Steinberg::Vst::ProcessModes::kRealtime;
    m_vstData.numInputs = 0;
    m_vstData.numOutputs = 0;

    m_vstData.inputs = nullptr;
    m_vstData.outputs = nullptr;
    m_vstData.inputEvents = nullptr;
    m_vstData.outputEvents = nullptr;

    m_vstData.inputParameterChanges = &m_inputChanges;
    m_vstData.outputParameterChanges = &m_outputChanges;

    m_vstData.processContext = nullptr;

    for (int i = 0, N = controller->getParameterCount(); i < N; i++)
    {
      Steinberg::Vst::ParameterInfo inf;
      controller->getParameterInfo(i, inf);
      auto& queue = m_inputChanges.queues.emplace_back(inf.id);
      Steinberg::int32 x;
      queue.lastValue = controller->getParamNormalized(inf.id);
      queue.addPoint(0, queue.lastValue, x);
    }

    processor->process(m_vstData);
  }
};

void syncVST3ControllerToProcessor(const vst3::Model& eff)
{
  // The main "data" to save is the one that is part of the Component.
  // Problem: this is the part that executes in the audio thread.
  // So if we aren't playing, the updates aren't being transmitted to it...
  // And in that case the processor's copy of the controls isn't updated, and we save old values.
  // Thus we have to sync the processor with the controller in that case.

  if (auto comp = score::findComponent<vst3::Executor>(eff.components()))
    return;

  // TODO do something more fine-grained.
  // Or potentially send a request to the plug-in, and wait for a signal that it executed.

  InputCopier{eff.fx};
}

template <>
void DataStreamReader::read(const vst3::Model& eff)
{
  syncVST3ControllerToProcessor(eff);

  m_stream << QString::fromStdString(eff.m_uid.toString()) << eff.readProcessorState()
           << eff.readControllerState();
  readPorts(*this, eff.m_inlets, eff.m_outlets);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(vst3::Model& eff)
{
  QString uid;
  m_stream >> uid >> eff.m_savedProcessorState >> eff.m_savedControllerState;

  if(auto id = VST3::UID::fromString(uid.toStdString()))
    eff.m_uid = *id;
  auto& vst_plug = this->components.applicationPlugin<vst3::ApplicationPlugin>();
  eff.m_vstPath = vst_plug.pathForClass(eff.m_uid);

  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      eff.m_inlets,
      eff.m_outlets,
      &eff);

  eff.load();
  checkDelimiter();
}

template <>
void JSONReader::read(const vst3::Model& eff)
{
  syncVST3ControllerToProcessor(eff);

  readPorts(*this, eff.m_inlets, eff.m_outlets);
  obj["UID"] = QString::fromStdString(eff.m_uid.toString());
  obj["State"] = eff.readProcessorState().toBase64();
  obj["UIState"] = eff.readControllerState().toBase64();
}

template <>
void JSONWriter::write(vst3::Model& eff)
{
  auto& vst_plug = this->components.applicationPlugin<vst3::ApplicationPlugin>();
  if(auto id = obj.tryGet("UID"))
  {
    if(auto uid = VST3::UID::fromString(id->toStdString()))
      eff.m_uid = *uid;
    eff.m_vstPath = vst_plug.pathForClass(eff.m_uid);
  }
  else
  {
    // Old format
    QString m_vstPath, m_className;
    m_vstPath <<= obj["VstPath"];
    m_className <<= obj["ClassName"];
    if(auto id = vst_plug.uidForPathAndClassName(m_vstPath, m_className))
    {
      eff.m_uid = *id;
      // Note: we use a different path here because
      // the VST may be in a different path that the one saved.
      eff.m_vstPath = vst_plug.pathForClass(eff.m_uid);
    }
  }

  {
    {
      QByteArray b;
      b <<= obj["State"];
      eff.m_savedProcessorState = QByteArray::fromBase64(b);
    }

    {
      QByteArray b;
      b <<= obj["UIState"];
      eff.m_savedControllerState = QByteArray::fromBase64(b);
    }
  }

  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      eff.m_inlets,
      eff.m_outlets,
      &eff);

  eff.load();
}

template <>
void DataStreamReader::read<vst3::ControlInlet>(const vst3::ControlInlet& p)
{
  m_stream << (uint32_t)p.fxNum << p.m_value;
}
template <>
void DataStreamWriter::write<vst3::ControlInlet>(vst3::ControlInlet& p)
{
  m_stream >> (uint32_t&)p.fxNum >> p.m_value;
}

template <>
void JSONReader::read<vst3::ControlInlet>(const vst3::ControlInlet& p)
{
  obj["FxNum"] = (uint32_t)p.fxNum;
  obj["Value"] = p.value();
}
template <>
void JSONWriter::write<vst3::ControlInlet>(vst3::ControlInlet& p)
{
  p.fxNum = obj["FxNum"].toInt();
  p.setValue(obj["Value"].toDouble());
}
