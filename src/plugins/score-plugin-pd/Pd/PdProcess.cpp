#include "PdProcess.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/File.hpp>

#include <QFile>
#include <QRegularExpression>
#include <QProcess>
#include <QSettings>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Pd::ProcessModel)
namespace Pd
{

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
    if(QFile::exists("/usr/bin/pd"))
      return "/usr/bin/pd";
    else if(QFile::exists("/usr/local/bin/pd"))
      return "/usr/local/bin/pd";
#if _WIN32
    else if(QFile::exists("c:\\Program Files\\Pd\\bin\\pd.exe"))
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
    else if(checkIfBinaryIsInPath("pd"))
      return "pd";
#endif
    else
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
    setMidiInput(false);
    setMidiOutput(false);

    auto old_inlets = score::clearAndDeleteLater(m_inlets);
    auto old_outlets = score::clearAndDeleteLater(m_outlets);

    int i = 0;
    auto get_next_id = [&] {
      i++;
      return Id<Process::Port>(i);
    };

    auto patch = f.readAll();
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
      static const QRegularExpression recv_regex{R"_(r \\\$0-(.*);)_"};
      auto it = recv_regex.globalMatch(patch);
      while (it.hasNext())
      {
        const auto& m = it.next();
        if (m.hasMatch())
        {
          const auto var = m.capturedTexts()[1];

          auto p = new Process::ValueInlet{get_next_id(), this};
          p->setCustomData(var);
          m_inlets.push_back(p);
        }
      }
    }

    {
      static const QRegularExpression send_regex{R"_(s \\\$0-(.*);)_"};
      auto it = send_regex.globalMatch(patch);
      while (it.hasNext())
      {
        const auto& m = it.next();
        if (m.hasMatch())
        {
          const auto var = m.capturedTexts()[1];

          auto p = new Process::ValueOutlet{get_next_id(), this};
          p->setCustomData(var);
          m_outlets.push_back(p);
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
