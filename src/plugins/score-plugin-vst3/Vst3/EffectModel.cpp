
#include "EffectModel.hpp"
#include "Node.hpp"

#include <Vst3/ApplicationPlugin.hpp>
#include <Vst3/Control.hpp>
#include <Vst3/Executor.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/tools/std/String.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/model/ComponentUtils.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/pod_vector.hpp>

#include <QInputDialog>
#include <QTimer>

#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/Model.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Vst3/UI/Window.hpp>
#include <Vst3/DataStream.hpp>
#include <cmath>
#include <public.sdk/source/vst/hosting/module.h>
#include <websocketpp/base64/base64.hpp>
#include <wobjectimpl.h>
#include <pluginterfaces/vst/ivstprocesscontext.h>

#include <memory>
#include <set>
W_OBJECT_IMPL(vst3::Model)
namespace Process
{
template <>
QString EffectProcessFactory_T<vst3::Model>::customConstructionData() const
{/*
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
Process::Descriptor EffectProcessFactory_T<vst3::Model>::descriptor(QString d) const
{
  Process::Descriptor desc;
  /*
  auto& app = score::GUIAppContext().applicationPlugin<Media::ApplicationPlugin>();

  auto it = ossia::find_if(app.vst_infos, [=](const Media::ApplicationPlugin::vst_info& vst) {
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
  }*/
  return desc;
}
}

namespace vst3
{


Model::Model(
    TimeVal t,
    const QString& path,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : ProcessModel{t, id, "VST", parent}
{
  auto identifier = splitWithoutEmptyParts(path, "/::/");
  if(identifier.size() == 2)
  {
    m_vstPath = identifier[0];
    m_className = identifier[1];
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

void Model::on_addControl(const Steinberg::Vst::ParameterInfo& v)
{
  if(controls.find(v.id) != controls.end())
  {
    return;
  }

  SCORE_ASSERT(controls.find(v.id) == controls.end());
  auto ctrl = new ControlInlet{Id<Process::Port>(getStrongId(inlets()).val()), this};
  ctrl->hidden = true;
  ctrl->fxNum = v.id;
  ctrl->setValue(v.defaultNormalizedValue);
  ctrl->setCustomData(fromString(v.title));

  on_addControl_impl(ctrl);
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
  for (auto e : m_inlets)
    if (e->id() == p)
      return static_cast<ControlInlet*>(e);
  return nullptr;
}

void Model::init()
{
}

void Model::on_addControl_impl(ControlInlet* ctrl)
{
  m_inlets.push_back(ctrl);
  initControl(ctrl);
  controlAdded(ctrl->id());
}

void Model::initControl(ControlInlet* ctrl)
{
  ctrl->setValue(fx.controller->getParamNormalized(ctrl->fxNum));
  connect(ctrl, &vst3::ControlInlet::valueChanged,
          this, [this, i = ctrl->fxNum](float newval) {
    auto& c = *fx.controller;
    if (std::abs(newval - c.getParamNormalized(i)) > 0.0001)
      c.setParamNormalized(i, newval);
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

  try {
    this->fx.load(*this, p, m_vstPath.toStdString(), m_className.toStdString(), media.getRate(), 4096);
  } catch(std::exception& e) {
    qDebug() << e.what();
    this->fx = {};
    return;
  }

  metadata().setLabel(m_className);


  /// this->fx.controller->setComponentState(...);
}


struct BusActivationVisitor {
  Plugin& fx;

  void audioIn(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    if (bus.flags & Steinberg::Vst::BusInfo::kDefaultActive)
    {
      fx.component->activateBus(Steinberg::Vst::kAudio, Steinberg::Vst::kInput, idx, true);
    }
  }
  void eventIn(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    if (bus.flags & Steinberg::Vst::BusInfo::kDefaultActive)
    {
      fx.component->activateBus(Steinberg::Vst::kEvent, Steinberg::Vst::kInput, idx, true);
    }
  }

  void audioOut(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    if (bus.flags & Steinberg::Vst::BusInfo::kDefaultActive)
    {
      fx.component->activateBus(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput, idx, true);
    }
  }

  void eventOut(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    if (bus.flags & Steinberg::Vst::BusInfo::kDefaultActive)
    {
      fx.component->activateBus(Steinberg::Vst::kEvent, Steinberg::Vst::kOutput, idx, true);
    }
  }
};

struct PortCreationVisitor {
  Model& model;
  Plugin& fx;

  int inlet_i = 0;
  int outlet_i = 0;

  void audioIn(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    BusActivationVisitor{fx}.audioIn(bus, idx);

    auto port = new Process::AudioInlet(Id<Process::Port>{inlet_i++}, &model);
    port->setCustomData(fromString(bus.name));
    model.m_inlets.push_back(port);
  }
  void eventIn(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    BusActivationVisitor{fx}.eventIn(bus, idx);

    auto port = new Process::MidiInlet(Id<Process::Port>{inlet_i++}, &model);
    port->setCustomData(fromString(bus.name));
    model.m_inlets.push_back(port);
  }

  void audioOut(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    BusActivationVisitor{fx}.audioOut(bus, idx);

    auto port = new Process::AudioOutlet(Id<Process::Port>{outlet_i++}, &model);
    port->setCustomData(fromString(bus.name));
    model.m_outlets.push_back(port);

    if(idx == 0)
      port->setPropagate(true);
  }

  void eventOut(const Steinberg::Vst::BusInfo& bus, int idx)
  {
    BusActivationVisitor{fx}.eventOut(bus, idx);

    auto port = new Process::MidiOutlet(Id<Process::Port>{outlet_i++}, &model);
    port->setCustomData(fromString(bus.name));
    model.m_outlets.push_back(port);
  }
};

void Model::create()
{
  SCORE_ASSERT(!fx);

  initFx();
  if (!fx)
    return;

  forEachBus(PortCreationVisitor{*this, fx}, *fx.component);

  if(fx.controller)
  {
    fx.loadProcessorStateToController();

    const int numParams = fx.controller->getParameterCount();
    if(numParams < VST_DEFAULT_PARAM_NUMBER_CUTOFF || !fx.view)
    {
      Steinberg::Vst::ParameterInfo p;
      for(int i = 0; i < numParams; i++) {
        if(fx.controller->getParameterInfo(i, p) == Steinberg::kResultOk)
        {
          on_addControl(p);
        }
      }
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

  for(auto*inlet : this->m_inlets)
  {
    if(auto ctl = qobject_cast<ControlInlet*>(inlet))
    {
      initControl(ctl);
    }
  }
}

QByteArray Model::readProcessorState() const
{
  if(fx.component)
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
  if(fx.controller)
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
  if(fx.component)
  {
    // First reload the processor state into the processor.
    {
      QDataStream str{m_savedProcessorState};

      Vst3DataStream stream{str};
      fx.component->setState(&stream);

      // Then into the controller
      if(fx.controller)
      {
        QDataStream str{m_savedProcessorState};
        Vst3DataStream stream{str};
        fx.controller->setComponentState(&stream);

      }
      m_savedProcessorState = {};
    }

    // Then reload the controller-specific data
    if(fx.controller)
    {
      {
        QDataStream str{m_savedControllerState};

        Vst3DataStream stream{str};

        m_savedControllerState = {};
      }
    }
  }
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

    for(int i = 0, N = controller->getParameterCount(); i < N; i++)
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

  if(auto comp = score::findComponent<vst3::Executor>(eff.components()))
    return;

  // TODO do something more fine-grained.
  // Or potentially send a request to the plug-in, and wait for a signal that it executed.

  InputCopier{eff.fx};
}

template <>
void DataStreamReader::read(const vst3::Model& eff)
{
  syncVST3ControllerToProcessor(eff);

  m_stream << eff.m_vstPath << eff.m_className << eff.readProcessorState() << eff.readControllerState();
  readPorts(*this, eff.m_inlets, eff.m_outlets);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(vst3::Model& eff)
{
  m_stream >> eff.m_vstPath >> eff.m_className >> eff.m_savedProcessorState >> eff.m_savedControllerState;

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets, eff.m_outlets, &eff);

  eff.load();
  checkDelimiter();
}

template <>
void JSONReader::read(const vst3::Model& eff)
{
  syncVST3ControllerToProcessor(eff);

  readPorts(*this, eff.m_inlets, eff.m_outlets);
  obj["VstPath"] = eff.m_vstPath;
  obj["ClassName"] = eff.m_className;
  obj["State"] = eff.readProcessorState().toBase64();
  obj["UIState"] = eff.readControllerState().toBase64();
}

template <>
void JSONWriter::write(vst3::Model& eff)
{
  eff.m_vstPath <<= obj["VstPath"];
  eff.m_className <<= obj["ClassName"];

  {
    QByteArray b;

    b <<= obj["State"];
    eff.m_savedProcessorState = QByteArray::fromBase64(b);

    b <<= obj["UIState"];
    eff.m_savedControllerState = QByteArray::fromBase64(b);
  }

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(),
        eff.m_inlets, eff.m_outlets, &eff);

  eff.load();
}

template <>
void DataStreamReader::read<vst3::ControlInlet>(const vst3::ControlInlet& p)
{
  m_stream << (uint32_t)p.fxNum;
}
template <>
void DataStreamWriter::write<vst3::ControlInlet>(vst3::ControlInlet& p)
{
  m_stream >> (uint32_t&)p.fxNum;
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
