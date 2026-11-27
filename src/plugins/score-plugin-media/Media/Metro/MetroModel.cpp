#include "MetroModel.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortSerialization.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/ExternalFiles.hpp>

#include <Audio/Settings/Model.hpp>
#include <Library/LibrarySettings.hpp>
#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/Drop/SoundDrop.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/tools/FilePath.hpp>

#include <core/document/Document.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QCoreApplication>
#include <QFileInfo>
#include <QPointer>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::Metro::Model)

namespace Media::Metro
{
namespace
{
int engineRate()
{
  const int rate = score::AppContext().settings<Audio::Settings::Model>().getRate();
  return rate > 0 ? rate : 48000;
}

//! Starts decoding a file, in memory and at the rate of the engine.
std::shared_ptr<AudioFile> decode(const QString& path, const QString& absolutePath)
{
  auto f = std::make_shared<AudioFile>();
  f->load({path, absolutePath, DecodingMethod::Libav});
  return f;
}

/**
 * @brief The two ticks of the user library, shared by all the metronomes.
 *
 * Looked up again when asked: the library can be installed, moved, or the
 * rate of the engine changed, after the first metronome was made.
 */
class ClassicTicks final
    : public QObject
    , public Nano::Observer
{
  W_OBJECT(ClassicTicks)
public:
  static ClassicTicks& instance()
  {
    static ClassicTicks ticks;
    return ticks;
  }

  void refresh()
  {
    // Nothing of an application outlives it: tests run several in a row.
    if(m_app != qApp)
    {
      m_app = qApp;
      connect(qApp, &QCoreApplication::aboutToQuit, this, &ClassicTicks::clear);
    }

    const QString root
        = score::AppContext().settings<Library::Settings::Model>().getDefaultLibraryPath()
          + QStringLiteral("/Util/");
    const QString tick_path = root + QStringLiteral("metro_tick.wav");
    const QString tock_path = root + QStringLiteral("metro_tock.wav");
    const bool present = QFileInfo::exists(tick_path) && QFileInfo::exists(tock_path);
    const int rate = engineRate();
    if(root == m_root && rate == m_rate && present == m_present)
      return;

    m_root = root;
    m_rate = rate;
    m_present = present;
    tick.reset();
    tock.reset();
    if(!present)
      return;

    tick = decode(tick_path, tick_path);
    tock = decode(tock_path, tock_path);
    tick->on_finishedDecoding.connect<&ClassicTicks::on_decoded>(*this);
    tock->on_finishedDecoding.connect<&ClassicTicks::on_decoded>(*this);
  }

  std::shared_ptr<AudioFile> tick;
  std::shared_ptr<AudioFile> tock;

  void changed() W_SIGNAL(changed);

private:
  void on_decoded() { changed(); }

  void clear()
  {
    tick.reset();
    tock.reset();
    m_root.clear();
    m_rate = 0;
    m_present = false;
  }

  QPointer<QCoreApplication> m_app;

  QString m_root;
  int m_rate{};
  bool m_present{};
};

//! A decoded file as a click at `rate`. Empty while it is not decoded.
click_sound toSound(const std::shared_ptr<AudioFile>& file, int rate, bool mono)
{
  click_sound s;
  if(!file || !file->finishedDecoding() || rate <= 0)
    return s;

  const auto data = file->getAudioArray();
  const int file_rate = file->sampleRate();
  if(data.empty() || data[0].empty() || file_rate <= 0)
    return s;

  const int64_t in_frames = std::min<int64_t>(
      data[0].size(), std::llround(Model::maxFileSeconds * file_rate));
  const auto convert = [&](const auto& in, std::vector<float>& out) {
    if(file_rate == rate)
    {
      out.assign(in.begin(), in.begin() + in_frames);
      return;
    }
    // The decoder already resampled to the rate of the audio settings; this
    // only happens when the engine runs at another one, e.g. a device that
    // imposes its own.
    const double step = double(file_rate) / rate;
    const int64_t out_frames = std::max<int64_t>(1, std::floor(in_frames / step));
    out.resize(out_frames);
    for(int64_t i = 0; i < out_frames; i++)
    {
      const double pos = i * step;
      const int64_t a = std::min<int64_t>(int64_t(pos), in_frames - 1);
      const int64_t b = std::min<int64_t>(a + 1, in_frames - 1);
      const float f = float(pos - double(a));
      out[i] = in[a] + (in[b] - in[a]) * f;
    }
  };

  convert(data[0], s.left);
  if(!mono && data.size() >= 2 && data[1].size() >= std::size_t(in_frames))
    convert(data[1], s.right);
  fade_tail(s, rate);
  return s;
}

std::vector<std::pair<QString, ossia::value>> soundChoices()
{
  return {{QObject::tr("Classic"), int(Classic)},     {QObject::tr("Click"), int(Click)},
          {QObject::tr("Woodblock"), int(Woodblock)}, {QObject::tr("Beep"), int(Beep)},
          {QObject::tr("Cowbell"), int(Cowbell)},     {QObject::tr("Files"), int(Files)}};
}

QString fileFilters()
{
  QStringList exts;
  for(const auto& e : Media::Sound::DropHandler{}.fileExtensions())
    exts.push_back(QStringLiteral("*.") + e);
  exts.sort();
  return QObject::tr("Sound files (%1)").arg(exts.join(QLatin1Char(' ')));
}

std::unique_ptr<Process::ControlInlet> makeSound(QObject* parent)
{
  return std::make_unique<Process::ComboBox>(
      soundChoices(), int(Classic), QObject::tr("Sound"), Id<Process::Port>(0), parent);
}
std::unique_ptr<Process::ControlInlet> makeVolume(QObject* parent)
{
  return std::make_unique<Process::FloatSlider>(
      0.f, 1.f, 1.f, QObject::tr("Volume"), Id<Process::Port>(3), parent);
}
std::unique_ptr<Process::ControlInlet> makeAccent(QObject* parent)
{
  return std::make_unique<Process::Toggle>(
      true, QObject::tr("Accent"), Id<Process::Port>(4), parent);
}
std::unique_ptr<Process::ControlInlet> makeSubdivision(QObject* parent)
{
  return std::make_unique<Process::IntSpinBox>(
      1, 8, 1, QObject::tr("Subdivision"), Id<Process::Port>(5), parent);
}
//! The last port: the ones before it keep the ids documents know them by.
std::unique_ptr<Process::ControlInlet> makeBeat(beat_unit unit, QObject* parent)
{
  auto p = std::make_unique<Process::ComboBox>(
      std::vector<std::pair<QString, ossia::value>>{
          {QObject::tr("Time signature"), int(SignatureBeat)},
          {QObject::tr("Quarter note"), int(QuarterBeat)}},
      int(unit), QObject::tr("Beat"), Id<Process::Port>(6), parent);
  return p;
}
std::unique_ptr<Process::ControlInlet> makeAccentFile(QObject* parent)
{
  return std::make_unique<Process::FileChooser>(
      QString{}, fileFilters(), QObject::tr("Accent file"), Id<Process::Port>(1), parent);
}
std::unique_ptr<Process::ControlInlet> makeBeatFile(QObject* parent)
{
  return std::make_unique<Process::FileChooser>(
      QString{}, fileFilters(), QObject::tr("Beat file"), Id<Process::Port>(2), parent);
}

//! The controls of a new metronome.
void makeDefaultControls(Model& proc)
{
  proc.sound = makeSound(&proc);
  proc.volume = makeVolume(&proc);
  proc.accent = makeAccent(&proc);
  proc.subdivision = makeSubdivision(&proc);
  proc.beat = makeBeat(SignatureBeat, &proc);
  proc.accentFile = makeAccentFile(&proc);
  proc.beatFile = makeBeatFile(&proc);
}
}

Model::Model(
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::
          ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , audio_outlet{std::make_unique<Process::AudioOutlet>(
          "Audio Out", Id<Process::Port>(0), this)}
    , bang_outlet{std::make_unique<Process::ValueOutlet>(
          "Pulse Out", Id<Process::Port>(1), this)}
{
  makeDefaultControls(*this);
  audio_outlet->setPropagate(true);

  metadata().setInstanceName(*this);
  init();
}

Model::~Model() { }

void Model::init()
{
  // The files next to the control that plays them.
  for(auto* p : {sound.get(), accentFile.get(), beatFile.get(), volume.get(),
                 accent.get(), subdivision.get(), beat.get()})
    m_inlets.push_back(p);

  // Here and not where the port is made: a loaded port has it too.
  beat->setDescription(tr(
      "Time signature: a click on every beat of the signature, an eighth note in 7/8. "
      "Quarter note: a click on every quarter note from the bar line, whatever the "
      "signature."));
  m_outlets.push_back(audio_outlet.get());
  m_outlets.push_back(bang_outlet.get());

  auto& classic = ClassicTicks::instance();
  classic.refresh();
  connect(&classic, &ClassicTicks::changed, this, &Model::soundsChanged);

  connect(
      accentFile.get(), &Process::ControlInlet::valueChanged, this,
      [this](const ossia::value& v) {
    loadFile(m_accentFile, v);
    soundsChanged();
  });
  connect(
      beatFile.get(), &Process::ControlInlet::valueChanged, this,
      [this](const ossia::value& v) {
    loadFile(m_beatFile, v);
    soundsChanged();
  });

  // What is decoded is at the rate of the engine.
  auto& settings = score::AppContext().settings<Audio::Settings::Model>();
  connect(&settings, &Audio::Settings::Model::RateChanged, this, &Model::reloadFiles);

  loadFile(m_accentFile, accentFile->value());
  loadFile(m_beatFile, beatFile->value());
}

std::shared_ptr<AudioFile> Model::decodeFile(const QString& path, const QObject& context)
{
  if(path.isEmpty())
    return {};

  // An object that is not in a document has no project folder to look in.
  const auto* doc = score::IDocument::try_documentFromObject(context);
  const QString absolute
      = doc ? score::locateFilePath(path, doc->context()) : score::locateFilePath(path);
  if(!AudioFile::isSupported(QFile{absolute}))
    return {};

  // Checked before decoding: the whole file goes in memory.
  const auto info = Media::probe(absolute);
  if(!info || info->duration().sec() > maxFileSeconds)
  {
    qWarning() << "Metronome: not a sound of less than" << maxFileSeconds
               << "seconds:" << absolute;
    return {};
  }

  return decode(path, absolute);
}

void Model::loadFile(std::shared_ptr<AudioFile>& file, const ossia::value& value)
{
  if(file)
    file->on_finishedDecoding.disconnect<&Model::on_decoded>(*this);

  file = decodeFile(QString::fromStdString(ossia::convert<std::string>(value)), *this);
  if(file)
    file->on_finishedDecoding.connect<&Model::on_decoded>(*this);
}

sound_set_ptr Model::fileSounds(
    const std::shared_ptr<AudioFile>& accent, const std::shared_ptr<AudioFile>& beat,
    int rate)
{
  auto set = std::make_shared<sound_set>();
  set->accent = toSound(accent, rate, false);
  set->beat = toSound(beat, rate, false);
  if(set->accent.empty() && set->beat.empty())
    return {};
  return set;
}

void Model::mapExternalFiles(Process::ExternalFileMap& map)
{
  // The two choosers are plain ones, which keeps them small in the layer:
  // what they hold is still sound.
  map.control(*accentFile, score::FileKind::Audio);
  map.control(*beatFile, score::FileKind::Audio);
}

void Model::reloadFiles()
{
  ClassicTicks::instance().refresh();
  loadFile(m_accentFile, accentFile->value());
  loadFile(m_beatFile, beatFile->value());
  soundsChanged();
}

void Model::on_decoded()
{
  soundsChanged();
}

sound_set_ptr Model::classicSounds(int rate)
{
  auto set = std::make_shared<sound_set>();
  auto& classic = ClassicTicks::instance();
  classic.refresh();
  // The first channel on both outputs, as these ticks always played.
  set->accent = toSound(classic.tick, rate, true);
  set->beat = toSound(classic.tock, rate, true);
  // Both or nothing: one of the two with a synthesized other is no pair.
  if(set->accent.empty() || set->beat.empty())
    return {};
  return set;
}

sound_set_ptr Model::sounds(sound_kind kind, int rate) const
{
  auto set = std::make_shared<sound_set>();
  switch(kind)
  {
    case Classic:
      return classicSounds(rate);
    case Files:
      return fileSounds(m_accentFile, m_beatFile, rate);
    default:
      return {};
  }
  return set;
}
}

template <>
void DataStreamReader::read(const Media::Metro::Model& proc)
{
  m_stream << *proc.audio_outlet << *proc.bang_outlet << *proc.sound << *proc.accentFile << *proc.beatFile << *proc.volume
           << *proc.accent << *proc.subdivision << *proc.beat;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Metro::Model& proc)
{
  using namespace Media::Metro;
  proc.audio_outlet = Process::load_audio_outlet(*this, &proc);
  proc.bang_outlet = Process::load_value_outlet(*this, &proc);

  proc.sound = Process::load_control_inlet(*this, &proc);
  proc.accentFile = Process::load_control_inlet(*this, &proc);
  proc.beatFile = Process::load_control_inlet(*this, &proc);
  proc.volume = Process::load_control_inlet(*this, &proc);
  proc.accent = Process::load_control_inlet(*this, &proc);
  proc.subdivision = Process::load_control_inlet(*this, &proc);
  proc.beat = Process::load_control_inlet(*this, &proc);
  checkDelimiter();
}

template <>
void JSONReader::read(const Media::Metro::Model& proc)
{
  obj["Outlet"] = *proc.audio_outlet;
  obj["BangOutlet"] = *proc.bang_outlet;
  obj["Sound"] = *proc.sound;
  obj["Volume"] = *proc.volume;
  obj["Accent"] = *proc.accent;
  obj["Subdivision"] = *proc.subdivision;
  obj["Beat"] = *proc.beat;
  obj["AccentFile"] = *proc.accentFile;
  obj["BeatFile"] = *proc.beatFile;
}

template <>
void JSONWriter::write(Media::Metro::Model& proc)
{
  using namespace Media::Metro;
  {
    JSONWriter writer{obj["Outlet"]};
    proc.audio_outlet = Process::load_audio_outlet(writer, &proc);
  }
  {
    JSONWriter writer{obj["BangOutlet"]};
    proc.bang_outlet = Process::load_value_outlet(writer, &proc);
  }

  // A document from before a control has what the control starts with.
  const auto load = [&](const auto& key, auto make) {
    if(auto port = obj.tryGet(key))
    {
      JSONWriter writer{*port};
      return Process::load_control_inlet(writer, &proc);
    }
    return make(&proc);
  };
  proc.sound = load("Sound", makeSound);
  proc.volume = load("Volume", makeVolume);
  proc.accent = load("Accent", makeAccent);
  proc.subdivision = load("Subdivision", makeSubdivision);
  proc.accentFile = load("AccentFile", makeAccentFile);
  proc.beatFile = load("BeatFile", makeBeatFile);

  // Not the default of a new metronome: a document without this control was
  // saved when a beat was always a quarter note, and still ticks so.
  proc.beat = load("Beat", [](QObject* parent) { return makeBeat(QuarterBeat, parent); });
}

W_OBJECT_IMPL(Media::Metro::ClassicTicks)
