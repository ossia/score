#include "PdProcess.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/File.hpp>
#include <ossia/detail/flat_map.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/base/parameter_data.hpp>

#include <QFile>
#include <QRegularExpression>
#include <QProcess>
#include <QDirIterator>
#include <QSettings>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Pd::ProcessModel)
namespace Pd
{

PatchSpec::Control parseControlSpec(QString var)
{
  var = var.replace("\n", " ");
  QStringList splitted = var.split(" ");
  splitted.removeAll(QString{});

  PatchSpec::Control ctl;
  ctl.name = splitted.front();
  ctl.remote = var;

  std::optional<ossia::value> min_domain{}, max_domain{};
  enum { unknown, type, range, min, max, unit, widget, defaultv } next_is{};
  for(int i = 1; i < splitted.size(); i++)
  {
    switch(next_is) {
      case unknown:
        if(splitted[i] == "@type")
        { next_is = type; break; }
        if(splitted[i] == "@range")
        { next_is = range; break; }
        if(splitted[i] == "@min")
        { next_is = min; break; }
        if(splitted[i] == "@max")
        { next_is = max; break; }
        if(splitted[i] == "@unit")
        { next_is = unit; break; }
        if(splitted[i] == "@widget")
        { next_is = widget; break; }
        if(splitted[i] == "@default")
        { next_is = defaultv; break; }
        break;
      case type:
        ctl.type = splitted[i];
        next_is = unknown;
        break;
      case range:
        if(i < splitted.size() - 1)
        {
          min_domain = splitted[i].toDouble();
          max_domain = splitted[i+1].toDouble();
          i++;
        }
        next_is = unknown;
        break;
      case min:
        min_domain = splitted[i].toDouble();
        next_is = unknown;
        break;
      case max:
        max_domain = splitted[i].toDouble();
        next_is = unknown;
        break;
      case unit:
        ctl.unit = splitted[i];
        next_is = unknown;
        break;
      case widget:
        ctl.widget = splitted[i];
        next_is = unknown;
        break;
      case defaultv:
        ctl.defaultv = splitted[i].toFloat();
        next_is = unknown;
        break;
    }
  }

  if(min_domain && max_domain)
    ctl.domain = ossia::make_domain(*min_domain, *max_domain);

  return ctl;
}

auto initFuncMap()
{
  using InletFunc = std::function<Process::Inlet*(const PatchSpec::Control& , const Id<Process::Port>& , QObject*)>;
  ossia::flat_map<QString, InletFunc> widgetFuncMap{
    {"floatslider", [] (const PatchSpec::Control& ctl, const Id<Process::Port>& id, QObject* parent) -> Process::Inlet* {
        const auto [dom_min, dom_max] = ossia::get_float_minmax(ctl.domain);
        float min{dom_min ? *dom_min : 0.f};
        float max{dom_max ? *dom_max : 1.f};
        float init{0.f};
        return new Process::FloatSlider{min, max, init, ctl.name, id, parent};
      }
    },
    {"logfloatslider", [] (const PatchSpec::Control& ctl, const Id<Process::Port>& id, QObject* parent) -> Process::Inlet* {
        const auto [dom_min, dom_max] = ossia::get_float_minmax(ctl.domain);
        float min{dom_min ? *dom_min : 0.f};
        float max{dom_max ? *dom_max : 1.f};
        float init{0.f};
        return new Process::LogFloatSlider{min, max, init, ctl.name, id, parent};
      }
    },
    {"intslider", [] (const PatchSpec::Control& ctl, const Id<Process::Port>& id, QObject* parent) -> Process::Inlet* {
        const auto [dom_min, dom_max] = ossia::get_float_minmax(ctl.domain);
        int min{dom_min ? int(*dom_min) : 0};
        int max{dom_max ? int(*dom_max) : 127};
        int init{0};
        return new Process::IntSlider{min, max, init, ctl.name, id, parent};
      }
    },
    {"intspinbox", [] (const PatchSpec::Control& ctl, const Id<Process::Port>& id, QObject* parent) -> Process::Inlet* {
        const auto [dom_min, dom_max] = ossia::get_float_minmax(ctl.domain);
        int min{dom_min ? int(*dom_min) : 0};
        int max{dom_max ? int(*dom_max) : 127};
        int init{0};
        return new Process::IntSpinBox{min, max, init, ctl.name, id, parent};
      }
    },
    {"toggle", [] (const PatchSpec::Control& ctl, const Id<Process::Port>& id, QObject* parent) -> Process::Inlet* {
         return new Process::Toggle{false, ctl.name, id, parent};
      }
    },
    {"lineedit", [] (const PatchSpec::Control& ctl, const Id<Process::Port>& id, QObject* parent) -> Process::Inlet* {
        return new Process::LineEdit{{}, ctl.name, id, parent};
      }
    },
    {"combobox", [] (const PatchSpec::Control& ctl, const Id<Process::Port>& id, QObject* parent) -> Process::Inlet* {
        std::vector<std::pair<QString, ossia::value>> choices;
        ossia::value init;
         return new Process::ComboBox{choices, init, ctl.name, id, parent};
      }
    },
    {"button", [] (const PatchSpec::Control& ctl, const Id<Process::Port>& id, QObject* parent) -> Process::Inlet* {
         return new Process::Button{ctl.name, id, parent};
      }
    },
    {"hsvslider", [] (const PatchSpec::Control& ctl, const Id<Process::Port>& id, QObject* parent) -> Process::Inlet* {
        return new Process::HSVSlider{ossia::vec4f{}, ctl.name, id, parent};
      }
    },
    {"xyslider", [] (const PatchSpec::Control& ctl, const Id<Process::Port>& id, QObject* parent) -> Process::Inlet* {
        return new Process::XYSlider{ossia::vec2f{}, ctl.name, id, parent};
      }
    },
    {"multislider", [] (const PatchSpec::Control& ctl, const Id<Process::Port>& id, QObject* parent) -> Process::Inlet* {
        return new Process::MultiSlider{std::vector<ossia::value>{}, ctl.name, id, parent};
      }
    }
  };
  return widgetFuncMap;
}
Process::Inlet* makeInletFromSpec(const PatchSpec::Control& ctl, const Id<Process::Port>& id, QObject* parent)
{
  static const auto& widgetFuncMap = initFuncMap();
  Process::Inlet* inl{};
  auto param = ossia::default_parameter_for_type(ctl.unit.toStdString());
  if(!param)
    param = ossia::default_parameter_for_type(ctl.type.toStdString());
  if(auto it = widgetFuncMap.find(ctl.widget); it != widgetFuncMap.end())
  {
    inl = it->second(ctl, id, parent);
  }
  else if(param)
  {
    if(param->unit)
    {
      auto dataspace = ossia::get_dataspace_text(param->unit);
      if(dataspace == "color")
        inl = widgetFuncMap.at("hsvslider")(ctl, id, parent);
      else if(dataspace == "position")
        inl = widgetFuncMap.at("xyslider")(ctl, id, parent);
    }
    else
    {
      switch(ossia::underlying_type(param->type))
      {
        case ossia::val_type::FLOAT:
          inl = widgetFuncMap.at("floatslider")(ctl, id, parent);
          break;
        case ossia::val_type::INT:
          inl = widgetFuncMap.at("intslider")(ctl, id, parent);
          break;
        case ossia::val_type::CHAR:
          inl = widgetFuncMap.at("intslider")(ctl, id, parent);
          break;
        case ossia::val_type::BOOL:
          inl = widgetFuncMap.at("toggle")(ctl, id, parent);
          break;
        case ossia::val_type::IMPULSE:
          inl = widgetFuncMap.at("button")(ctl, id, parent);
          break;
        case ossia::val_type::VEC2F:
        case ossia::val_type::VEC3F:
        case ossia::val_type::VEC4F:
        case ossia::val_type::LIST:
          inl = widgetFuncMap.at("multislider")(ctl, id, parent);
          break;
        case ossia::val_type::STRING:
          inl = widgetFuncMap.at("lineedit")(ctl, id, parent);
          break;
        case ossia::val_type::NONE:
          break;
      }
    }
  }

  if(!inl)
  {
    inl = new Process::ValueInlet{id, parent};
  }
  inl->setCustomData(ctl.name);
  return inl;
}
static bool checkIfBinaryIsInPath(const QString& binary)
{
#if !defined(_WIN32)
  QProcess findProcess;
  findProcess.start("which", {binary});
  findProcess.setReadChannel(QProcess::ProcessChannel::StandardOutput);

  if(!findProcess.waitForFinished())
    return {};

  QFileInfo check_file(findProcess.readAll().trimmed());
  return check_file.exists() && check_file.isFile();
#endif
  return false;
}

#if defined(_WIN32)
static QString readKeyFromRegistry(const QString& path, const QString& key)
{
  QSettings settings(path, QSettings::Registry64Format);
  return settings.value(key).toString();
}
#endif
const QString& locatePdBinary() noexcept
{
  static const QString pdbinary = [] () -> QString {
#if __APPLE__
    {
      const auto& applist = QDir{"/Applications"}.entryList();
      if(applist.contains("Pd-l2ork"))
      {
        QString pd_path = "/Applications/Pd-l2ork.app/Contents/MacOS/nwjs";
        if(QFile::exists(pd_path))
          return pd_path;
      }

      for(const auto& app : applist)
      {
        if(app.startsWith("Pd-"))
        {
          QString pd_path = "/Applications/" + app + "/Contents/MacOS/Pd";
          if(QFile::exists(pd_path))
            return pd_path;
        }
      }
    }
#endif

#if _WIN32
    if(QFile::exists("c:\\Program Files\\Purr Data\\bin\\pd.exe"))
      return "c:\\Program Files\\Purr Data\\bin\\pd.exe";
    else if(QFile::exists("c:\\Program Files (x86)\\Purr Data\\bin\\pd.exe"))
      return "c:\\Program Files (x86)\\Purr Data\\bin\\pd.exe";

    if(QFile::exists("c:\\Program Files\\Pd\\bin\\pd.exe"))
      return "c:\\Program Files\\Pd\\bin\\pd.exe";
    else if(QFile::exists("c:\\Program Files (x86)\\Pd\\bin\\pd.exe"))
      return "c:\\Program Files (x86)\\Pd\\bin\\pd.exe";
    else if(QString k = readKeyFromRegistry("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\pd.exe", "64"); !k.isEmpty())
      return k + "\\bin";
    else if(QString k = readKeyFromRegistry("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\pd.exe", "32"); !k.isEmpty())
      return k + "\\bin";
    else if(QString k = readKeyFromRegistry("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\pd.exe", "64"); !k.isEmpty())
      return k + "\\bin";
    else if(QString k = readKeyFromRegistry("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\pd.exe", "32"); !k.isEmpty())
      return k + "\\bin";
#else
    if(QFile::exists("/usr/bin/purr-data"))
      return "/usr/bin/purr-data";
    else if(QFile::exists("/usr/local/bin/purr-data"))
      return "/usr/local/bin/purr-data";

    else if(QFile::exists("/usr/bin/pd"))
      return "/usr/bin/pd";
    else if(QFile::exists("/usr/local/bin/pd"))
      return "/usr/local/bin/pd";
#endif

    if(checkIfBinaryIsInPath("purr-data"))
      return "purr-data";
    if(checkIfBinaryIsInPath("pd"))
      return "pd";

    return {};
  }();
  return pdbinary;
}

ProcessModel::ProcessModel(
    const TimeVal& duration,
      const QString& pdpatch,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration,
                            id,
                            Metadata<ObjectKey_k, ProcessModel>::get(),
                            parent}
{
  metadata().setInstanceName(*this);
  setScript(pdpatch);
}

bool ProcessModel::hasExternalUI() const noexcept
{
  const QString& pathToPd = locatePdBinary();
  return !pathToPd.isEmpty();
}

ProcessModel::~ProcessModel() {}

int ProcessModel::audioInputs() const
{
  return m_audioInputs;
}

int ProcessModel::audioOutputs() const
{
  return m_audioOutputs;
}

bool ProcessModel::midiInput() const
{
  return m_midiInput;
}

bool ProcessModel::midiOutput() const
{
  return m_midiOutput;
}

void ProcessModel::setAudioInputs(int audioInputs)
{
  if (m_audioInputs == audioInputs)
    return;

  m_audioInputs = audioInputs;
  audioInputsChanged(m_audioInputs);
}

void ProcessModel::setAudioOutputs(int audioOutputs)
{
  if (m_audioOutputs == audioOutputs)
    return;

  m_audioOutputs = audioOutputs;
  audioOutputsChanged(m_audioOutputs);
}

void ProcessModel::setMidiInput(bool midiInput)
{
  if (m_midiInput == midiInput)
    return;

  m_midiInput = midiInput;
  midiInputChanged(m_midiInput);
}

void ProcessModel::setMidiOutput(bool midiOutput)
{
  if (m_midiOutput == midiOutput)
    return;

  m_midiOutput = midiOutput;
  midiOutputChanged(m_midiOutput);
}

void ProcessModel::setScript(const QString& script)
{
  m_script = score::locateFilePath(
        script, score::IDocument::documentContext(*this));
  QFile f(m_script);
  if (f.open(QIODevice::ReadOnly))
  {
    m_spec.receives.clear();
    m_spec.sends.clear();
    setMidiInput(false);
    setMidiOutput(false);

    auto old_inlets = score::clearAndDeleteLater(m_inlets);
    auto old_outlets = score::clearAndDeleteLater(m_outlets);

    int i = 0;
    auto get_next_id = [&] {
      i++;
      return Id<Process::Port>(i);
    };

    QString patch = score::readFileAsQString(f);
    {
      static const QRegularExpression adc_regex{"adc~"};
      auto m = adc_regex.match(patch);
      if (m.hasMatch())
      {
        auto p = new Process::AudioInlet{get_next_id(), this};
        p->setCustomData("Audio In");
        setAudioInputs(2);
        m_inlets.push_back(p);
      }
    }

    {
      static const QRegularExpression dac_regex{"dac~"};
      auto m = dac_regex.match(patch);
      if (m.hasMatch())
      {
        auto p = new Process::AudioOutlet{get_next_id(), this};
        p->setPropagate(true);
        p->setCustomData("Audio Out");
        setAudioOutputs(2);
        m_outlets.push_back(p);
      }
    }

    {
      static const QRegularExpression midi_regex{"(midiin|notein|controlin)"};
      auto m = midi_regex.match(patch);
      if (m.hasMatch())
      {
        auto p = new Process::MidiInlet{get_next_id(), this};
        p->setCustomData("Midi In");
        m_inlets.push_back(p);

        setMidiInput(true);
      }
    }

    {
      static const QRegularExpression midi_regex{
        "(midiiout|noteout|controlout)"};
      auto m = midi_regex.match(patch);
      if (m.hasMatch())
      {
        auto p = new Process::MidiOutlet{get_next_id(), this};
        p->setCustomData("Midi Out");
        m_outlets.push_back(p);

        setMidiOutput(true);
      }
    }

    {
      static const QRegularExpression recv_regex{R"_((r|receive) \\\$0-(.*?);)_", QRegularExpression::DotMatchesEverythingOption};
      auto it = recv_regex.globalMatch(patch);
      while (it.hasNext())
      {
        const auto& m = it.next();
        if (m.hasMatch())
        {
          if(const auto var = m.captured(2); !var.isEmpty())
          {
            PatchSpec::Control ctl = parseControlSpec(var);

            auto p = makeInletFromSpec(ctl, get_next_id(), this);
            m_inlets.push_back(p);

            m_spec.receives.push_back(ctl);
          }
        }
      }
    }

    {
      static const QRegularExpression send_regex{R"_((s|send) \\\$0-(.*?);)_", QRegularExpression::DotMatchesEverythingOption};
      auto it = send_regex.globalMatch(patch);
      while (it.hasNext())
      {
        const auto& m = it.next();
        if (m.hasMatch())
        {
          if(const auto var = m.captured(2); !var.isEmpty())
          {
            PatchSpec::Control ctl = parseControlSpec(var);

            Process::Outlet* p{};
            p = new Process::ValueOutlet{get_next_id(), this};
            p->setCustomData(ctl.name);
            m_outlets.push_back(p);

            m_spec.sends.push_back(ctl);
          }
        }
      }
    }
    inletsChanged();
    outletsChanged();
  }

  scriptChanged(script);
}

const QString& ProcessModel::script() const
{
  return m_script;
}
}

template <>
void DataStreamReader::read(const Pd::ProcessModel& proc)
{
  insertDelimiter();

  m_stream << proc.m_script << proc.m_audioInputs << proc.m_audioOutputs
           << proc.m_midiInput << proc.m_midiOutput;

  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Pd::ProcessModel& proc)
{
  checkDelimiter();

  m_stream >> proc.m_script >> proc.m_audioInputs >> proc.m_audioOutputs
      >> proc.m_midiInput >> proc.m_midiOutput;

  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);

  checkDelimiter();
}

template <>
void JSONReader::read(const Pd::ProcessModel& proc)
{
  obj["Script"] = proc.script();
  obj["AudioInputs"] = proc.audioInputs();
  obj["AudioOutputs"] = proc.audioOutputs();
  obj["MidiInput"] = proc.midiInput();
  obj["MidiOutput"] = proc.midiOutput();

  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Pd::ProcessModel& proc)
{
  proc.m_script = obj["Script"].toString();
  proc.m_audioInputs = obj["AudioInputs"].toInt();
  proc.m_audioOutputs = obj["AudioOutputs"].toInt();
  proc.m_midiInput = obj["MidiInput"].toBool();
  proc.m_midiOutput = obj["MidiOutput"].toBool();

  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);
}
