#include "VSTEffectModel.hpp"
#include "VSTWidgets.hpp"

#include <Media/Effect/VST/VSTLoader.hpp>
#include <ModernMIDI/midi_message.h>
#include <ossia/detail/math.hpp>
#include <score/tools/Todo.hpp>
#include <Media/ApplicationPlugin.hpp>
#include <QTimer>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <websocketpp/base64/base64.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Media/Effect/VST/VSTControl.hpp>
#include <Media/Commands/VSTCommands.hpp>

#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <set>
#include <iostream>
#include <memory>
#include <cmath>
#include <unordered_map>

namespace Media::VST
{
VSTEffectModel::VSTEffectModel(
    TimeVal t,
    const QString& path,
    const Id<Process::ProcessModel>& id,
    QObject* parent):
  ProcessModel{t, id, "VST", parent},
  m_effectPath{path}
{
  init();
  create();
}

VSTEffectModel::~VSTEffectModel()
{
  closePlugin();
}

QString VSTEffectModel::prettyName() const
{
  return metadata().getLabel();
}

void VSTEffectModel::removeControl(int fxNum)
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
  emit controlRemoved(*ctrl);
  delete ctrl;
}

void VSTEffectModel::removeControl(const Id<Process::Port>& id)
{
  auto it = ossia::find_if(m_inlets, [&] (const auto& inl) {
    return inl->id() == id;
  });

  SCORE_ASSERT(it != m_inlets.end());
  auto ctrl = safe_cast<VSTControlInlet*>(*it);

  controls.erase(ctrl->fxNum);
  m_inlets.erase(it);

  emit controlRemoved(*ctrl);
  delete ctrl;
}

VSTControlInlet* VSTEffectModel::getControl(const Id<Process::Port>& p)
{
  for(auto e : m_inlets)
    if(e->id() == p)
      return static_cast<VSTControlInlet*>(e);
  return nullptr;
}

void VSTEffectModel::init()
{
  connect(this, &VSTEffectModel::addControl,
          this, &VSTEffectModel::on_addControl);
}

void VSTEffectModel::on_addControl(int i, float v)
{
  auto ctrl = new VSTControlInlet{
              Id<Process::Port>(getStrongId(inlets()).val()), this};
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
    if(!label.isEmpty())
      str += "(" + label + ")";

    ctrl->setCustomData(name);
  }

  on_addControl_impl(ctrl);
}

void VSTEffectModel::on_addControl_impl(VSTControlInlet* ctrl)
{
  connect(ctrl, &VSTControlInlet::valueChanged,
          this, [this,i=ctrl->fxNum] (float newval) {
    if(std::abs(newval - fx->getParameter(i)) > 0.0001)
      fx->setParameter(i, newval);
  });

  {
    /*
    VstParameterProperties props;
    auto res = dispatch(effGetParameterProperties, i, 0, &props);
    if(res == 1)
    {
      // apparently there's exactly 0 plug-ins supporting this
      qDebug() << props.label << props.minInteger << props.maxInteger << props.smallStepFloat << props.stepFloat;
    }
    */
  }
  m_inlets.push_back(ctrl);
  controls.insert({ctrl->fxNum, ctrl});
  emit controlAdded(ctrl->id());
}

QString VSTEffectModel::getString(AEffectOpcodes op, int param)
{
  char paramName[512] = {0};
  dispatch(op, param, 0, paramName);
  return QString::fromUtf8(paramName);
}

static auto HostCallback (AEffect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr, float opt)
{
  intptr_t result = 0;

  switch (opcode)
  {
    case audioMasterGetTime:
    {
      auto vst = reinterpret_cast<VSTEffectModel*>(effect->resvd1);
      if(vst)
      {
        result = reinterpret_cast<intptr_t>(&vst->fx->info);
      }
      break;
    }
    case audioMasterProcessEvents:
      break;
    case audioMasterIOChanged:
      break;
    case audioMasterSizeWindow:
      break;
    case audioMasterGetInputLatency:
      break;
    case audioMasterGetOutputLatency:
      break;
    case audioMasterIdle:
      effect->dispatcher(effect, effEditIdle, 0, 0, nullptr, 0);
      break;
    case audioMasterVersion :
      result = kVstVersion;
      break;

    case audioMasterCurrentId:
      result = effect->uniqueID;
      break;

    case audioMasterGetSampleRate:
      break;

    case audioMasterGetBlockSize:
      break;

    case audioMasterGetCurrentProcessLevel:
      result = kVstProcessLevelUnknown;
      break;

    case audioMasterAutomate:
    {
      auto vst = reinterpret_cast<VSTEffectModel*>(effect->resvd1);
      if(vst)
      {
        auto ctrl_it = vst->controls.find(index);
        if(ctrl_it != vst->controls.end())
        {
          ctrl_it->second->setValue(opt);
        }
        else
        {
          QMetaObject::invokeMethod(vst, [=] {
            auto& ctx = score::IDocument::documentContext(*vst);
            CommandDispatcher<> {ctx.commandStack}.submitCommand<CreateVSTControl>(*vst, index, opt);
          }, Qt::QueuedConnection);
        }
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

    case audioMasterUpdateDisplay:
      // TODO update all values
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
            HostCanDos::canDoHasCockosViewAsConfig
      };
      if(supported.find(static_cast<const char*>(ptr)) != supported.end())
        result = 1;
      break;
    }
  }
  return result;
}
void VSTEffectModel::closePlugin()
{
  if(fx)
  {
    if(ui)
    {
      auto w = reinterpret_cast<VSTWindow*>(ui);
      delete w; //hideUI();
    }
    fx = nullptr;
  }
  qDeleteAll(m_inlets);
  qDeleteAll(m_outlets);
  m_inlets.clear();
  m_outlets.clear();
  metadata().setLabel("Dead VST");
}

VSTModule* getPlugin(QString path)
{
  auto path_std = path.toStdString();
  VSTModule* plugin;
  auto& app = score::GUIAppContext().applicationPlugin<Media::ApplicationPlugin>();
  auto it = app.vst_modules.find(path_std);
  if(it == app.vst_modules.end())
  {
    if(path.isEmpty())
      return nullptr;

    bool isFile = QFile(QUrl(path).toString(QUrl::PreferLocalFile)).exists();
    if(!isFile)
    {
      qDebug() << "Invalid path: " << path;
      return nullptr;
    }

    try {
      plugin = new VSTModule{path_std};
      app.vst_modules.insert({std::move(path_std), plugin});
    } catch(const std::runtime_error& e) {
      qDebug() << e.what();
      return nullptr;
    }
  }
  else
  {
    plugin = it->second;
  }

  return plugin;
}

void VSTEffectModel::initFx(VSTModule& plugin)
{
  auto main = plugin.getMain();
  if(!main) {
    qDebug() << "plugin has no main";
    return;
  }

  fx = std::make_shared<AEffectWrapper>(main(HostCallback));
  if(!fx->fx)
  {
    qDebug() << "plugin was not created";
    fx.reset();
    return;
  }

  fx->fx->resvd1 = reinterpret_cast<intptr_t>(this);

  dispatch(effOpen);

  {
    char buf[256] = {0};
    dispatch(effGetProductString, 0, 0, buf);
    QString s = buf;
    if(!s.isEmpty())
      metadata().setLabel(s);
    else
      metadata().setLabel(QFileInfo(m_effectPath).baseName());
  }
}

void VSTEffectModel::create()
{
  SCORE_ASSERT(!fx);
  VSTModule* plugin = getPlugin(m_effectPath);
  if(!plugin)
    return;

  initFx(*plugin);
  if(!fx)
    return;

  int inlet_i = 0;
  if(fx->fx->flags & effFlagsIsSynth)
  {
    m_inlets.push_back(new Process::Inlet(Id<Process::Port>{inlet_i++}, this));
    m_inlets[0]->type = Process::PortType::Midi;
  }
  else
  {
    m_inlets.push_back(new Process::Inlet(Id<Process::Port>{inlet_i++}, this));
    m_inlets[0]->type = Process::PortType::Audio;
  }

  // Tempo
  {
    auto tempo = new Process::ControlInlet(Id<Process::Port>{inlet_i++}, this);
    m_inlets.push_back(tempo);
    tempo->type = Process::PortType::Message;
    tempo->setCustomData("Tempo");
    tempo->setValue(120.);
    tempo->setDomain(ossia::make_domain(20., 200.));
  }
  {
    // Signature
    auto sig = new Process::ControlInlet(Id<Process::Port>{inlet_i++}, this);
    m_inlets.push_back(sig);
    sig->type = Process::PortType::Message;
    sig->setCustomData("Time signature");
    sig->setValue("4/4");
  }

  if(fx->fx->numParams < 10)
  {
    for(int i = 0; i < 10; i++)
    {
      on_addControl(i, fx->getParameter(i));
    }
  }

  m_outlets.push_back(new Process::Outlet(Id<Process::Port>{}, this));
  m_outlets[0]->type = Process::PortType::Audio;
  m_outlets[0]->setPropagate(true);
}

void VSTEffectModel::load()
{
  SCORE_ASSERT(!fx);
  VSTModule* plugin = getPlugin(m_effectPath);
  if(!plugin)
    return;

  initFx(*plugin);
  if(!fx)
    return;

  for(std::size_t i = 3; i < m_inlets.size(); i++)
  {
    auto inlet = safe_cast<VSTControlInlet*>(m_inlets[i]);
    int ctrl = inlet->fxNum;
    connect(inlet, &VSTControlInlet::valueChanged,
            this, [this,ctrl] (float newval) {
      if(std::abs(newval - fx->getParameter(ctrl)) > 0.0001)
        fx->setParameter(ctrl, newval);
    });
    controls.insert({ctrl, inlet});
  }
}

}

template <>
void DataStreamReader::read(
    const Media::VST::VSTEffectModel& eff)
{
  readPorts(*this, eff.m_inlets, eff.m_outlets);
  m_stream << eff.effect();

  // TODO save & reload program parameters
  insertDelimiter();
}

template <>
void DataStreamWriter::write(
    Media::VST::VSTEffectModel& eff)
{
  writePorts(*this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets, eff.m_outlets, &eff);

  m_stream >> eff.m_effectPath;
  // TODO save & reload program parameters
  eff.load();
  checkDelimiter();
}

template <>
void JSONObjectReader::read(
    const Media::VST::VSTEffectModel& eff)
{
  readPorts(obj, eff.m_inlets, eff.m_outlets);
  obj["Effect"] = eff.effect();

  if(eff.fx)
  {
    if(eff.fx->fx->flags & effFlagsProgramChunks)
    {
      void* ptr{};
      auto res = eff.fx->dispatch(effGetChunk, 0, 0, &ptr, 0.f);
      if(ptr && res > 0)
      {
        auto encoded = websocketpp::base64_encode((const unsigned char*)ptr, res);
        obj["Data"] = QString::fromStdString(encoded);
      }
    }
    else
    {
      QJsonArray arr;
      for(int i = 0; i < eff.fx->fx->numParams; i++)
        arr.push_back(eff.fx->getParameter(i));
      obj["Params"] = std::move(arr);
    }
  }
}

template <>
void JSONObjectWriter::write(
    Media::VST::VSTEffectModel& eff)
{
  writePorts(obj, components.interfaces<Process::PortFactoryList>(), eff.m_inlets, eff.m_outlets, &eff);

  eff.m_effectPath = obj["Effect"].toString();
  eff.load();
  QPointer<Media::VST::VSTEffectModel> ptr = &eff;
  QTimer::singleShot(1000, [obj=this->obj,ptr] {
    if(!ptr)
      return;
    auto& eff = *ptr;
    if(eff.fx)
    {
      if(eff.fx->fx->flags & effFlagsProgramChunks)
      {
        auto it = obj.find("Data");
        if(it != obj.end())
        {
          auto b64 = websocketpp::base64_decode(it->toString().toStdString());
          eff.fx->dispatch(effSetChunk, 0, b64.size(), b64.data(), 0.f);

          for(std::size_t i = 3; i < eff.inlets().size(); i++)
          {
            auto inlet = safe_cast<Media::VST::VSTControlInlet*>(eff.inlets()[i]);
            inlet->setValue(eff.fx->getParameter(inlet->fxNum));
          }
        }
      }
      else
      {
        auto it = obj.find("Params");
        if(it != obj.end())
        {
          QJsonArray arr = it->toArray();
          for(int i = 0; i < arr.size(); i++)
          {
            eff.fx->setParameter(i, arr[i].toDouble());
          }
        }
      }
    }});
}



template<>
void DataStreamReader::read<Media::VST::VSTControlInlet>(const Media::VST::VSTControlInlet& p)
{
  m_stream << p.fxNum;
}
template<>
void DataStreamWriter::write<Media::VST::VSTControlInlet>(Media::VST::VSTControlInlet& p)
{
  m_stream >> p.fxNum;
}

template<>
void JSONObjectReader::read<Media::VST::VSTControlInlet>(const Media::VST::VSTControlInlet& p)
{
  obj["FxNum"] = p.fxNum;
}
template<>
void JSONObjectWriter::write<Media::VST::VSTControlInlet>(Media::VST::VSTControlInlet& p)
{
  p.fxNum = obj["FxNum"].toInt();
}
