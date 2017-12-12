#include "VSTEffectModel.hpp"

#include <QUrl>
#include <QFile>
#include <QFileInfo>

#include <ModernMIDI/midi_message.h>
#include <set>
#include <iostream>
#include <memory>
#include <cmath>
#include <unordered_map>
#include <ossia/detail/math.hpp>
#include <score/tools/Todo.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/network/domain/domain.hpp>
#include <Media/ApplicationPlugin.hpp>
#include <aeffectx.h>
#include <QTimer>
#include <Process/Dataflow/Port.hpp>
#include <websocketpp/base64/base64.hpp>
void show_vst2_editor(AEffect& effect, ERect rect);
void hide_vst2_editor(AEffect& effect);

namespace Media
{
namespace VST
{
// Taken from VST API
struct HostCanDos
{
    static const constexpr auto canDoSendVstEvents = "sendVstEvents"; ///< Host supports send of Vst events to plug-in
    static const constexpr auto canDoSendVstMidiEvent = "sendVstMidiEvent"; ///< Host supports send of MIDI events to plug-in
    static const constexpr auto canDoSendVstTimeInfo = "sendVstTimeInfo"; ///< Host supports send of VstTimeInfo to plug-in
    static const constexpr auto canDoReceiveVstEvents = "receiveVstEvents"; ///< Host can receive Vst events from plug-in
    static const constexpr auto canDoReceiveVstMidiEvent = "receiveVstMidiEvent"; ///< Host can receive MIDI events from plug-in
    static const constexpr auto canDoReportConnectionChanges = "reportConnectionChanges"; ///< Host will indicates the plug-in when something change in plug-in�s routing/connections with #suspend/#resume/#setSpeakerArrangement
    static const constexpr auto canDoAcceptIOChanges = "acceptIOChanges"; ///< Host supports #ioChanged ()
    static const constexpr auto canDoSizeWindow = "sizeWindow"; ///< used by VSTGUI
    static const constexpr auto canDoOffline = "offline"; ///< Host supports offline feature
    static const constexpr auto canDoOpenFileSelector = "openFileSelector"; ///< Host supports function #openFileSelector ()
    static const constexpr auto canDoCloseFileSelector = "closeFileSelector"; ///< Host supports function #closeFileSelector ()
    static const constexpr auto canDoStartStopProcess = "startStopProcess"; ///< Host supports functions #startProcess () and #stopProcess ()
    static const constexpr auto canDoShellCategory = "shellCategory"; ///< 'shell' handling via uniqueID. If supported by the Host and the Plug-in has the category #kPlugCategShell
    static const constexpr auto canDoSendVstMidiEventFlagIsRealtime = "sendVstMidiEventFlagIsRealtime"; ///< Host supports flags for #VstMidiEvent


    static const constexpr auto canDoHasCockosViewAsConfig = "hasCockosViewAsConfig"; ///< Host supports flags for #VstMidiEvent

};

VSTEffectModel::VSTEffectModel(
    const QString& path,
    const Id<EffectModel>& id,
    QObject* parent):
  EffectModel{id, parent},
  m_effectPath{path}
{
  reload();
}

VSTEffectModel::VSTEffectModel(
    const VSTEffectModel& source,
    const Id<EffectModel>& id,
    QObject* parent):
  EffectModel{id, parent},
  m_effectPath{source.effect()}
{
  reload();
}
VSTEffectModel::~VSTEffectModel()
{
  closePlugin();
}

void VSTEffectModel::readPlugin()
{
}

static auto HostCallback (AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
{
  VstIntPtr result = 0;

  switch (opcode)
  {
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
      auto inlet = static_cast<Process::ControlInlet*>(vst->inlets()[1 + index]);
      inlet->setValue(opt);
      inlet->setUiVisible(true);

      break;
    }
    case audioMasterGetAutomationState:
      result = kVstAutomationOff;
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

    case audioMasterCanDo:
    {
      static const std::set<std::string_view> supported{
        HostCanDos::canDoSendVstEvents,
        HostCanDos::canDoSendVstMidiEvent,
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
    hideUI();
    dispatch(effStopProcess);
    dispatch(effMainsChanged, 0, 0);
    dispatch(effClose);
    fx = nullptr;
  }
  qDeleteAll(m_inlets);
  qDeleteAll(m_outlets);
  m_inlets.clear();
  m_outlets.clear();
  metadata().setLabel("Dead VST");
}

void VSTEffectModel::showUI()
{
  ERect* vstRect{};

  dispatch(effEditGetRect, 0, 0, &vstRect, 0.0f);

  int w{};
  int h{};
  if(vstRect)
  {
    w = vstRect->right - vstRect->left;
    h = vstRect->bottom - vstRect->top;
  }

  if(w <= 1)
    w = 640;
  if(h <= 1)
    h = 480;

  show_vst2_editor(*fx, *vstRect);
}

void VSTEffectModel::hideUI()
{
  hide_vst2_editor(*fx);
}

void VSTEffectModel::reload()
{
  closePlugin();

  auto path = m_effectPath.toStdString();

  VSTModule* plugin;
  auto& app = score::GUIAppContext().applicationPlugin<Media::ApplicationPlugin>();
  auto it = app.vst_modules.find(path);
  if(it == app.vst_modules.end())
  {
    if(path.empty())
      return;

    bool isFile = QFile(QUrl(m_effectPath).toString(QUrl::PreferLocalFile)).exists();
    if(!isFile)
    {
      qDebug() << "Invalid path: " << m_effectPath;
      return;
    }

    try {
      plugin = new VSTModule{path};
      app.vst_modules.insert({std::move(path), plugin});
    } catch(const std::runtime_error& e) {
      qDebug() << e.what();
      return;
    }
  }
  else
  {
    plugin = it->second;
  }

  if(!plugin)
    return;

  auto main = plugin->getMain();
  if(!main) {
    qDebug() << "plugin has no main";
    return;
  }

  fx = main(HostCallback);
  if(!fx)
  {
    qDebug() << "plugin was not created";
    return;
  }

  fx->resvd1 = reinterpret_cast<VstIntPtr>(this);

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

  int inlet_i = 0;
  if(fx->flags & effFlagsIsSynth)
  {
    m_inlets.push_back(new Process::Inlet(Id<Process::Port>(++inlet_i), this));
    m_inlets[0]->type = Process::PortType::Midi;
  }
  else
  {
    m_inlets.push_back(new Process::Inlet(Id<Process::Port>(++inlet_i), this));
    m_inlets[0]->type = Process::PortType::Audio;
  }

  for(int i = 0; i < fx->numParams; i++)
  {
    auto p = new Process::ControlInlet{Id<Process::Port>{++inlet_i}, this};

    // Metadata
    {
      auto get_string = [=] (auto req, int i) {
        char paramName[256] = {0};
        dispatch(req, i, 0, paramName);
        return QString::fromUtf8(paramName);
      };
      auto name = get_string(effGetParamName, i);
      auto label = get_string(effGetParamLabel, i);
      // auto display = get_string(effGetParamDisplay, i);

      // Get the name
      if(!label.isEmpty())
        p->setCustomData(label);
      else if(!name.isEmpty())
        p->setCustomData(name);
      else
        p->setCustomData("Parameter");
    }

    // Value
    {
      auto val = fx->getParameter(fx, i);
      p->setDomain(ossia::make_domain(0.f, 1.f));
      p->setValue(val);
      p->hidden = true;
      if(fx->numParams < 10)
      {
        p->setUiVisible(true);
      }

      connect(p, &Process::ControlInlet::valueChanged,
              this, [=] (const ossia::value& v){
        auto newval =  ossia::convert<float>(v);
        if(std::abs(newval - fx->getParameter(fx, i)) > 0.0001)
          fx->setParameter(fx, i, newval);
      });
    }

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

    m_inlets.push_back(p);
  }

  m_outlets.push_back(new Process::Outlet(Id<Process::Port>(0), this));
  m_outlets[0]->type = Process::PortType::Audio;
  m_outlets[0]->setPropagate(true);
}
}
}

template <>
void DataStreamReader::read(
    const Media::VST::VSTEffectModel& eff)
{
  m_stream << eff.effect();
  // TODO save & reload program parameters
  insertDelimiter();
}

template <>
void DataStreamWriter::write(
    Media::VST::VSTEffectModel& eff)
{
  m_stream >> eff.m_effectPath;
  eff.reload();
  checkDelimiter();
}

template <>
void JSONObjectReader::read(
    const Media::VST::VSTEffectModel& eff)
{
  obj["Effect"] = eff.effect();
  if(eff.fx)
  {
    if(eff.fx->flags & effFlagsProgramChunks)
    {
      void* ptr{};
      auto res = eff.fx->dispatcher(eff.fx, effGetChunk, 0, 0, &ptr, 0.f);
      if(ptr && res > 0)
      {
        auto encoded = websocketpp::base64_encode((const unsigned char*)ptr, res);
        obj["Data"] = QString::fromStdString(encoded);
      }
    }
    else
    {
      QJsonArray arr;
      for(int i = 0; i < eff.fx->numParams; i++)
        arr.push_back(eff.fx->getParameter(eff.fx, i));
      obj["Params"] = std::move(arr);
    }
  }
}

template <>
void JSONObjectWriter::write(
    Media::VST::VSTEffectModel& eff)
{
  eff.m_effectPath = obj["Effect"].toString();
  eff.reload();
  QTimer::singleShot(1000, [obj=this->obj,&eff] {
    if(eff.fx)
    {
      if(eff.fx->flags & effFlagsProgramChunks)
      {
        auto it = obj.find("Data");
        if(it != obj.end())
        {
          auto b64 = websocketpp::base64_decode(it->toString().toStdString());
          eff.fx->dispatcher(eff.fx, effSetChunk, 0, b64.size(), b64.data(), 0.f);

          for(int i = 1; i < eff.inlets().size(); i++)
          {
            static_cast<Process::ControlInlet*>(eff.inlets()[i])->setValue(
                  eff.fx->getParameter(eff.fx, i-1));
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
            eff.fx->setParameter(eff.fx, i, arr[i].toDouble());
          }
        }
      }
    }});
}
