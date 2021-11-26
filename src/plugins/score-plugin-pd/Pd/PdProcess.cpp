#include "PdProcess.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/PresetHelpers.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/File.hpp>

#include <Audio/Settings/Model.hpp>
#include <ossia/detail/small_flat_map.hpp>
#include <ossia/network/base/parameter_data.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>

#include <wobjectimpl.h>
#include <z_libpd.h>
W_OBJECT_IMPL(Pd::ProcessModel)
namespace Pd
{
Instance::Instance()
{
  instance = pdinstance_new();
}

Instance::~Instance()
{
  pdinstance_free(instance);
}

auto initTypeMap()
{
  ossia::flat_map<QString, ossia::val_type> widgetTypeMap{
      {"floatslider", ossia::val_type::FLOAT},
      {"logfloatslider", ossia::val_type::FLOAT},
      {"intslider",ossia::val_type::INT},
      {"intspinbox",ossia::val_type::INT},
      {"toggle",ossia::val_type::BOOL},
      {"lineedit",ossia::val_type::STRING},
      {"combobox",ossia::val_type::STRING},
      {"enum",ossia::val_type::STRING},
      {"button", ossia::val_type::IMPULSE},
      {"hsvslider", ossia::val_type::VEC4F},
      {"xyslider", ossia::val_type::VEC2F},
      {"multislider", ossia::val_type::LIST}};
  return widgetTypeMap;
}
enum pd_thing_to_parse
{
  unknown,
  type,
  range,
  min,
  max,
  unit,
  widget,
  defaultv
};

void parseControlTypeAttributes(PatchSpec::Control& ctl, const QStringList& args)
{
  pd_thing_to_parse next_is{};
  // First look for the type
    for (int i = 1; i < args.size(); i++)
  {
    switch (next_is)
    {
      default:
      case unknown:
        if (args[i] == "@type")
        {
          next_is = type;
          break;
        }
        if (args[i] == "@unit")
        {
          next_is = unit;
          break;
        }
        if (args[i] == "@widget")
        {
          next_is = widget;
          break;
        }
        break;
      case type:
        ctl.type = args[i];
        next_is = unknown;
        break;
      case unit:
        ctl.unit = args[i];
        next_is = unknown;
        break;
      case widget:
        ctl.widget = args[i];
        next_is = unknown;
        break;
    }
  }

  static const auto& widgetFuncMap = initTypeMap();
  if (auto it = widgetFuncMap.find(ctl.widget); it != widgetFuncMap.end())
  {
    ctl.deduced_type = it->second;
  }
  else
  {
    if(auto param = ossia::default_parameter_for_type(ctl.unit.toStdString()))
      ctl.deduced_type = ossia::underlying_type(param->type);
    else if(auto param = ossia::default_parameter_for_type(ctl.type.toStdString()))
      ctl.deduced_type = ossia::underlying_type(param->type);
    else
      ctl.deduced_type = ossia::val_type::FLOAT;
  }
}

std::optional<ossia::value> parseControlValue(PatchSpec::Control& ctl, const QStringList& args, int& i)
{
  switch(*ctl.deduced_type)
  {
    case ossia::val_type::NONE:
      return std::nullopt;
    case ossia::val_type::BOOL:
    {
      auto str = args[i].toLower();
      return bool(str.startsWith('t') || str.startsWith('y') || str == "1");
    }
    case ossia::val_type::IMPULSE:
      return ossia::impulse{};
    case ossia::val_type::FLOAT:
    {
      bool ok{true};
      double v = args[i].toDouble(&ok);
      if(ok)
        return float(v);
      break;
    }
    case ossia::val_type::INT:
    {
      bool ok{true};
      int v = args[i].toInt(&ok);
      if(ok)
        return int(v);
      break;
    }
    case ossia::val_type::CHAR:
    {
      bool ok{true};
      int v = args[i].toInt(&ok);
      if(ok)
        return int(v);
      break;
    }
    case ossia::val_type::VEC2F:
      SCORE_TODO;
      break;
    case ossia::val_type::VEC3F:
      SCORE_TODO;
      break;
    case ossia::val_type::VEC4F:
      SCORE_TODO;
      break;
    case ossia::val_type::LIST:
      SCORE_TODO;
      break;
    case ossia::val_type::STRING:
      return args[i].toStdString();
  }
  return std::nullopt;
}

void parseControlDataRange(PatchSpec::Control& ctl, const QStringList& args, int& i, std::optional<ossia::value>& min_domain, std::optional<ossia::value>& max_domain, std::optional<ossia::domain>& domain)
{
  switch(*ctl.deduced_type)
  {
    case ossia::val_type::NONE:
    case ossia::val_type::BOOL:
    case ossia::val_type::IMPULSE:
      break;
    case ossia::val_type::FLOAT:
    {
      if (i < args.size() - 1)
      {
        bool ok{true};
        min_domain = args[i].toDouble(&ok);
        if(!ok) min_domain = std::nullopt;

        i++;
        ok = true;
        max_domain = args[i].toDouble(&ok);
        if(!ok) max_domain = std::nullopt;
      }
      break;
    }
    case ossia::val_type::INT:
    {
      if (i < args.size() - 1)
      {
        bool ok{true};
        min_domain = args[i].toInt(&ok);
        if(!ok) min_domain = std::nullopt;

        i++;
        ok = true;
        max_domain = args[i].toInt(&ok);
        if(!ok) max_domain = std::nullopt;
      }
      break;
    }
    case ossia::val_type::CHAR:
    {
      if (i < args.size() - 1)
      {
        bool ok{true};
        min_domain = args[i].toInt(&ok);
        if(!ok) min_domain = std::nullopt;

        i++;
        ok = true;
        max_domain = args[i].toInt(&ok);
        if(!ok) max_domain = std::nullopt;
      }
      break;
    }
    case ossia::val_type::VEC2F:
      SCORE_TODO;
      break;
    case ossia::val_type::VEC3F:
      SCORE_TODO;
      break;
    case ossia::val_type::VEC4F:
      SCORE_TODO;
      break;
    case ossia::val_type::LIST:
      SCORE_TODO;
      break;
    case ossia::val_type::STRING:
    {
      std::vector<std::string> vec;
      while(i < args.size() && args[i][0] != '@')
      {
        vec.push_back(args[i].toStdString());
        i++;
      }
      if(!vec.empty())
      {
        domain = ossia::domain_base<std::string>{std::move(vec)};
      }
      break;
    }
  }
}
void parseControlDataAttributes(PatchSpec::Control& ctl, const QStringList& args)
{
  pd_thing_to_parse next_is{};
  std::optional<ossia::value> min_domain{}, max_domain{};
  std::optional<ossia::domain> domain;
  for (int i = 1; i < args.size(); i++)
  {
    switch (next_is)
    {
      default:
      case unknown:
        if (args[i] == "@range")
        {
          next_is = range;
          break;
        }
        if (args[i] == "@min")
        {
          next_is = min;
          break;
        }
        if (args[i] == "@max")
        {
          next_is = max;
          break;
        }
        if (args[i] == "@default")
        {
          next_is = defaultv;
          break;
        }
        break;
      case range:
        parseControlDataRange(ctl, args, i, min_domain, max_domain, domain);
        next_is = unknown;
        break;
      case min:
        min_domain = args[i].toDouble();
        next_is = unknown;
        break;
      case max:
        max_domain = args[i].toDouble();
        next_is = unknown;
        break;
      case defaultv:
        ctl.defaultv = args[i].toFloat();
        next_is = unknown;
        break;
    }
  }

  if (domain)
    ctl.domain = std::move(*domain);
  else if (min_domain && max_domain)
    ctl.domain = ossia::make_domain(*min_domain, *max_domain);
}

PatchSpec::Control parseControlSpec(QString var)
{
  var = var.replace("\n", " ");
  QStringList splitted = var.split(" ");
  splitted.removeAll(QString{});

  PatchSpec::Control ctl;
  ctl.name = splitted.front();
  ctl.remote = var;

  parseControlTypeAttributes(ctl, splitted);
  parseControlDataAttributes(ctl, splitted);

  return ctl;
}

const auto& initFuncMap()
{
  using InletFunc = std::function<Process::Inlet*(
      const PatchSpec::Control&, const Id<Process::Port>&, QObject*)>;
  static ossia::static_flat_map<QString, InletFunc, 32> widgetFuncMap{
      {"floatslider",
       [](const PatchSpec::Control& ctl,
          const Id<Process::Port>& id,
          QObject* parent) -> Process::Inlet* {
         const auto [dom_min, dom_max] = ossia::get_float_minmax(ctl.domain);
         float min{dom_min ? *dom_min : 0.f};
         float max{dom_max ? *dom_max : 1.f};
         float init{ossia::convert<float>(ctl.defaultv)};
         return new Process::FloatSlider{min, max, init, ctl.name, id, parent};
       }},
      {"logfloatslider",
       [](const PatchSpec::Control& ctl,
          const Id<Process::Port>& id,
          QObject* parent) -> Process::Inlet* {
         const auto [dom_min, dom_max] = ossia::get_float_minmax(ctl.domain);
         float min{dom_min ? *dom_min : 0.f};
         float max{dom_max ? *dom_max : 1.f};
         float init{ossia::convert<float>(ctl.defaultv)};
         return new Process::LogFloatSlider{
             min, max, init, ctl.name, id, parent};
       }},
      {"intslider",
       [](const PatchSpec::Control& ctl,
          const Id<Process::Port>& id,
          QObject* parent) -> Process::Inlet* {
         const auto [dom_min, dom_max] = ossia::get_float_minmax(ctl.domain);
         int min{dom_min ? int(*dom_min) : 0};
         int max{dom_max ? int(*dom_max) : 127};
         int init{ossia::convert<int>(ctl.defaultv)};
         return new Process::IntSlider{min, max, init, ctl.name, id, parent};
       }},
      {"intspinbox",
       [](const PatchSpec::Control& ctl,
          const Id<Process::Port>& id,
          QObject* parent) -> Process::Inlet* {
         const auto [dom_min, dom_max] = ossia::get_float_minmax(ctl.domain);
         int min{dom_min ? int(*dom_min) : 0};
         int max{dom_max ? int(*dom_max) : 127};
         int init{ossia::convert<int>(ctl.defaultv)};
         return new Process::IntSpinBox{min, max, init, ctl.name, id, parent};
       }},
      {"toggle",
       [](const PatchSpec::Control& ctl,
          const Id<Process::Port>& id,
          QObject* parent) -> Process::Inlet* {
         return new Process::Toggle{
             ossia::convert<bool>(ctl.defaultv), ctl.name, id, parent};
       }},
      {"lineedit",
       [](const PatchSpec::Control& ctl,
          const Id<Process::Port>& id,
          QObject* parent) -> Process::Inlet* {
         const std::string& init = ossia::convert<std::string>(ctl.defaultv);
         return new Process::LineEdit{
             QString::fromUtf8(init.c_str(), init.size()),
             ctl.name,
             id,
             parent};
       }},
      {"combobox",
       [](const PatchSpec::Control& ctl,
          const Id<Process::Port>& id,
          QObject* parent) -> Process::Inlet* {
         std::vector<std::string> choices;
         if(auto dom = ctl.domain.v.target<ossia::domain_base<std::string>>())
           choices = dom->values;
         std::string defaultv;
         if(auto v = ctl.defaultv.target<std::string>())
           defaultv = *v;

         return new Process::Enum{
             choices, {}, defaultv, ctl.name, id, parent};
       }},
      {"button",
       [](const PatchSpec::Control& ctl,
          const Id<Process::Port>& id,
          QObject* parent) -> Process::Inlet* {
         return new Process::Button{ctl.name, id, parent};
       }},
      {"hsvslider",
       [](const PatchSpec::Control& ctl,
          const Id<Process::Port>& id,
          QObject* parent) -> Process::Inlet* {
         return new Process::HSVSlider{ossia::vec4f{}, ctl.name, id, parent};
       }},
      {"xyslider",
       [](const PatchSpec::Control& ctl,
          const Id<Process::Port>& id,
          QObject* parent) -> Process::Inlet* {
         return new Process::XYSlider{ossia::vec2f{}, ctl.name, id, parent};
       }},
      {"multislider",
       [](const PatchSpec::Control& ctl,
          const Id<Process::Port>& id,
          QObject* parent) -> Process::Inlet* {
         return new Process::MultiSlider{
             std::vector<ossia::value>{}, ctl.name, id, parent};
       }}};
  widgetFuncMap["colorchooser"] = widgetFuncMap["hsvslider"];
  widgetFuncMap["enum"] = widgetFuncMap["combobox"];
  widgetFuncMap["text"] = widgetFuncMap["lineedit"];
  widgetFuncMap["checkbox"] = widgetFuncMap["toggle"];
  widgetFuncMap["bang"] = widgetFuncMap["button"];
  widgetFuncMap["pulse"] = widgetFuncMap["button"];
  widgetFuncMap["impulse"] = widgetFuncMap["button"];
  return widgetFuncMap;
}
Process::Inlet* makeInletFromSpec(
    const PatchSpec::Control& ctl,
    const Id<Process::Port>& id,
    QObject* parent)
{
  static const auto& widgetFuncMap = initFuncMap();
  Process::Inlet* inl{};
  if (auto it = widgetFuncMap.find(ctl.widget); it != widgetFuncMap.end())
  {
    inl = it->second(ctl, id, parent);
  }
  else
  {
    auto param = ossia::default_parameter_for_type(ctl.unit.toStdString());
    if (!param)
      param = ossia::default_parameter_for_type(ctl.type.toStdString());
    if (param)
    {
      if (param->unit)
      {
        auto dataspace = ossia::get_dataspace_text(param->unit);
        if (dataspace == "color")
          inl = widgetFuncMap.at("hsvslider")(ctl, id, parent);
        else if (dataspace == "position")
          inl = widgetFuncMap.at("xyslider")(ctl, id, parent);
      }
      else
      {
        switch (ossia::underlying_type(param->type))
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
  }

  if (!inl)
  {
    inl = new Process::ValueInlet{id, parent};
  }
  inl->setName(ctl.name);
  return inl;
}
static bool checkIfBinaryIsInPath(const QString& binary)
{
#if !defined(_WIN32)
  QProcess findProcess;
  findProcess.start("which", {binary});
  findProcess.setReadChannel(QProcess::ProcessChannel::StandardOutput);

  if (!findProcess.waitForFinished())
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
const QString& locatePurrDataBinary() noexcept
{
  static const QString pdbinary = []() -> QString {
#if __APPLE__
    {
      const auto& applist = QDir{"/Applications"}.entryList();
      if (applist.contains("Pd-l2ork.app"))
      {
        QString pd_path = "/Applications/Pd-l2ork.app/Contents/MacOS/nwjs";
        if (QFile::exists(pd_path))
          return pd_path;
      }
    }
#endif

#if _WIN32
    if (QFile::exists("c:\\Program Files\\Purr Data\\bin\\pd.exe"))
      return "c:\\Program Files\\Purr Data\\bin\\pd.exe";
    else if (QFile::exists("c:\\Program Files (x86)\\Purr Data\\bin\\pd.exe"))
      return "c:\\Program Files (x86)\\Purr Data\\bin\\pd.exe";
#else
    if (QFile::exists("/usr/bin/purr-data"))
      return "/usr/bin/purr-data";
    else if (QFile::exists("/usr/local/bin/purr-data"))
      return "/usr/local/bin/purr-data";

#endif

    if (checkIfBinaryIsInPath("purr-data"))
      return "purr-data";

    return {};
  }();
  return pdbinary;
}

const QString& locatePdBinary() noexcept
{
  static const QString pdbinary = []() -> QString {
#if __APPLE__
    {
      const auto& applist = QDir{"/Applications"}.entryList();

      for (const auto& app : applist)
      {
        if (app.startsWith("Pd-"))
        {
          QString pd_path = "/Applications/" + app + "/Contents/MacOS/Pd";
          if (QFile::exists(pd_path))
            return pd_path;
        }
      }
    }
#endif

#if _WIN32

    if (QFile::exists("c:\\Program Files\\Pd\\bin\\pd.exe"))
      return "c:\\Program Files\\Pd\\bin\\pd.exe";
    else if (QFile::exists("c:\\Program Files (x86)\\Pd\\bin\\pd.exe"))
      return "c:\\Program Files (x86)\\Pd\\bin\\pd.exe";
    else if (QString k = readKeyFromRegistry(
                 "HKEY_LOCAL_"
                 "MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\App "
                 "Paths\\pd.exe",
                 "64");
             !k.isEmpty())
      return k + "\\bin";
    else if (QString k = readKeyFromRegistry(
                 "HKEY_LOCAL_"
                 "MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\App "
                 "Paths\\pd.exe",
                 "32");
             !k.isEmpty())
      return k + "\\bin";
    else if (QString k = readKeyFromRegistry(
                 "HKEY_CURRENT_"
                 "USER\\Software\\Microsoft\\Windows\\CurrentVersion\\App "
                 "Paths\\pd.exe",
                 "64");
             !k.isEmpty())
      return k + "\\bin";
    else if (QString k = readKeyFromRegistry(
                 "HKEY_CURRENT_"
                 "USER\\Software\\Microsoft\\Windows\\CurrentVersion\\App "
                 "Paths\\pd.exe",
                 "32");
             !k.isEmpty())
      return k + "\\bin";
#else
    if (QFile::exists("/usr/bin/pd"))
      return "/usr/bin/pd";
    else if (QFile::exists("/usr/local/bin/pd"))
      return "/usr/local/bin/pd";
#endif

    if (checkIfBinaryIsInPath("pd"))
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
    : Process::ProcessModel{
        duration,
        id,
        Metadata<ObjectKey_k, ProcessModel>::get(),
        parent}
{
  metadata().setInstanceName(*this);
  init();
  setScript(pdpatch);
}

bool ProcessModel::hasExternalUI() const noexcept
{
  const QString& pathToPd = locatePdBinary();
  return !pathToPd.isEmpty();
}

ProcessModel::~ProcessModel()
{
  libpd_set_instance(m_instance->instance);

  if(m_instance->ui_open)
  {
    m_instance->ui_open = false;
    libpd_stop_gui();
  }

  if(m_instance->file_handle)
  {
    libpd_closefile(m_instance->file_handle);
  }
}

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

void ProcessModel::init()
{
  m_instance = std::make_shared<Instance>();
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
        p->setName("Audio In");
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
        p->setName("Audio Out");
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
        p->setName("Midi In");
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
        p->setName("Midi Out");
        m_outlets.push_back(p);

        setMidiOutput(true);
      }
    }

    {
      static const QRegularExpression recv_regex{
          R"_((r|receive)\s+\\\$0-(.*?)(,\s+f\s+[0-9]+)?;)_",
          QRegularExpression::DotMatchesEverythingOption};
      auto it = recv_regex.globalMatch(patch);
      while (it.hasNext())
      {
        const auto& m = it.next();
        if (m.hasMatch())
        {
          if (const auto var = m.captured(2); !var.isEmpty())
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
      static const QRegularExpression send_regex{
          R"_((s|send)\s+\\\$0-(.*?)(,\s+f\s+[0-9]+)?;)_",
          QRegularExpression::DotMatchesEverythingOption};
      auto it = send_regex.globalMatch(patch);
      while (it.hasNext())
      {
        const auto& m = it.next();
        if (m.hasMatch())
        {
          if (const auto var = m.captured(2); !var.isEmpty())
          {
            PatchSpec::Control ctl = parseControlSpec(var);

            Process::Outlet* p{};
            p = new Process::ValueOutlet{get_next_id(), this};
            p->setName(ctl.name);
            m_outlets.push_back(p);

            m_spec.sends.push_back(ctl);
          }
        }
      }
    }
    inletsChanged();
    outletsChanged();
  }
  // Create instance
  pd_setinstance(m_instance->instance);

  if(m_instance->file_handle)
    libpd_closefile(m_instance->file_handle);

  // Enable audio
  libpd_init_audio(m_audioInputs, m_audioOutputs, score::AppContext().settings<Audio::Settings::Model>().getRate());

  libpd_start_message(1);
  libpd_add_float(1.0f);
  libpd_finish_message("pd", "dsp");

  // Open
  QFileInfo fileinfo{f};
  auto folder = fileinfo.canonicalPath();
  libpd_add_to_search_path(folder.toUtf8().data());

  m_instance->file_handle = libpd_openfile(fileinfo.fileName().toUtf8().data(), folder.toUtf8().data());
  m_instance->dollarzero = libpd_getdollarzero(m_instance->file_handle);

  std::vector<float> temp_buff;
  temp_buff.resize(libpd_blocksize() * (std::max(m_audioInputs, m_audioOutputs)));

  libpd_process_raw(temp_buff.data(), temp_buff.data());

  scriptChanged(script);
}


const QString& ProcessModel::script() const
{
  return m_script;
}

QString ProcessModel::effect() const noexcept
{
  return m_script;
}

void ProcessModel::loadPreset(const Process::Preset& preset)
{
  Process::loadScriptProcessPreset<ProcessModel::p_script>(*this, preset);
}

Process::Preset ProcessModel::savePreset() const noexcept
{
  return Process::saveScriptProcessPreset(*this, this->m_script);
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

  QString script;
  m_stream >> script >> proc.m_audioInputs >> proc.m_audioOutputs
      >> proc.m_midiInput >> proc.m_midiOutput;
  proc.setScript(script);

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
  proc.setScript(obj["Script"].toString());
  proc.m_audioInputs = obj["AudioInputs"].toInt();
  proc.m_audioOutputs = obj["AudioOutputs"].toInt();
  proc.m_midiInput = obj["MidiInput"].toBool();
  proc.m_midiOutput = obj["MidiOutput"].toBool();

  // TODO what happens if the patch's inputs / outputs changed??
  // Maybe there should be the "edit script" algorithm available in a more general way
  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);
}
