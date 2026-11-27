#include <ossia/detail/flicks.hpp>
#include "RecordingSettings.hpp"

#if SCORE_RECORDING_TEXTURES
#include <Recording/Video/VideoEncoding.hpp>
#include <Gfx/Libav/LibavSettingsWidget.hpp>
#endif

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>

#include <Curve/Settings/CurveSettingsModel.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/FormWidget.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Recording::Settings::Model)
W_OBJECT_IMPL(Recording::Settings::View)

namespace Recording::Settings
{
namespace Parameters
{
SETTINGS_PARAMETER_IMPL(AudioFormat){
    QStringLiteral("score_plugin_recording/AudioFormat"), AudioFormats{}.float32};
SETTINGS_PARAMETER_IMPL(Start){
    QStringLiteral("score_plugin_recording/Start"), Starts{}.now};
SETTINGS_PARAMETER_IMPL(StartOn){
    QStringLiteral("score_plugin_recording/StartOn"), Grids{}.none};
SETTINGS_PARAMETER_IMPL(LengthBars){
    QStringLiteral("score_plugin_recording/LengthBars"), 0.};
SETTINGS_PARAMETER_IMPL(Target){
    QStringLiteral("score_plugin_recording/Target"), Targets{}.box};
SETTINGS_PARAMETER_IMPL(MuteSource){
    QStringLiteral("score_plugin_recording/MuteSource"), false};
SETTINGS_PARAMETER_IMPL(LoopFixedLength){
    QStringLiteral("score_plugin_recording/LoopFixedLength"), true};
SETTINGS_PARAMETER_IMPL(MidiQuantize){
    QStringLiteral("score_plugin_recording/MidiQuantize"), Quantizes{}.none};
SETTINGS_PARAMETER_IMPL(VideoEncoder){
    QStringLiteral("score_plugin_recording/VideoEncoder"), QString{}};
SETTINGS_PARAMETER_IMPL(InputLatency){
    QStringLiteral("score_plugin_recording/InputLatency"), 0};
SETTINGS_PARAMETER_IMPL(EngineLatency){
    QStringLiteral("score_plugin_recording/EngineLatency"), false};
SETTINGS_PARAMETER_IMPL(CaptureBars){
    QStringLiteral("score_plugin_recording/CaptureBars"), 4};
SETTINGS_PARAMETER_IMPL(TouchRelease){
    QStringLiteral("score_plugin_recording/TouchRelease"), 500};
SETTINGS_PARAMETER_IMPL(ClickSound){
    QStringLiteral("score_plugin_recording/ClickSound"), ClickSounds{}.classic};
SETTINGS_PARAMETER_IMPL(ClickVolume){
    QStringLiteral("score_plugin_recording/ClickVolume"), 100};
SETTINGS_PARAMETER_IMPL(ClickAccent){
    QStringLiteral("score_plugin_recording/ClickAccent"), true};
SETTINGS_PARAMETER_IMPL(CountInBars){
    QStringLiteral("score_plugin_recording/CountInBars"), 1};
SETTINGS_PARAMETER_IMPL(Metronome){
    QStringLiteral("score_plugin_recording/Metronome"), false};
SETTINGS_PARAMETER_IMPL(CountInOnPlay){
    QStringLiteral("score_plugin_recording/CountInOnPlay"), false};

static auto list()
{
  return std::tie(
      AudioFormat, Start, StartOn, LengthBars, Target, MuteSource, LoopFixedLength,
      MidiQuantize, VideoEncoder, TouchRelease, CaptureBars, InputLatency, EngineLatency,
      ClickSound, ClickVolume, ClickAccent, CountInBars, Metronome, CountInOnPlay);
}
}

Model::Model(
    const UuidKey<score::SettingsDelegateFactory>& k, QSettings& set,
    const score::ApplicationContext& ctx)
    : score::SettingsDelegateModel{k, nullptr}
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

RecordOptions Model::options() const
{
  RecordOptions o;
  if(m_AudioFormat == AudioFormats{}.pcm24)
    o.audioFormat = WavStreamWriter::Format::Pcm24;
  else if(m_AudioFormat == AudioFormats{}.pcm16)
    o.audioFormat = WavStreamWriter::Format::Pcm16;
  else
    o.audioFormat = WavStreamWriter::Format::Float32;

  if(m_Start == Starts{}.onPlay)
    o.start = RecordOptions::Start::OnPlay;
  else if(m_Start == Starts{}.firstEvent)
    o.start = RecordOptions::Start::FirstEvent;
  else
    o.start = RecordOptions::Start::Now;

  if(m_StartOn == Grids{}.bar)
    o.startRate = 1.;
  else if(m_StartOn == Grids{}.quarter)
    o.startRate = 4.;
  else
    o.startRate = 0.;

  o.lengthBars = std::max(0., m_LengthBars);
  o.target = m_Target == Targets{}.file ? RecordOptions::Target::FileOnly
                                        : RecordOptions::Target::ThisBox;
  o.muteSource = m_MuteSource;
  o.loopFixedLength = m_LoopFixedLength;
  if(m_MidiQuantize == Quantizes{}.quarter)
    o.midiQuantize = 1.;
  else if(m_MidiQuantize == Quantizes{}.eighth)
    o.midiQuantize = 0.5;
  else if(m_MidiQuantize == Quantizes{}.sixteenth)
    o.midiQuantize = 0.25;
  else if(m_MidiQuantize == Quantizes{}.thirtysecond)
    o.midiQuantize = 0.125;
  else
    o.midiQuantize = 0.;
  o.inputLatencyMs = std::clamp(m_InputLatency, 0, 500);
  o.engineLatency = m_EngineLatency;
  o.touchRelease = int64_t(std::max(0, m_TouchRelease)) * ossia::flicks_per_millisecond<int64_t>;
  o.video.encoder = m_VideoEncoder;
  const QStringList sounds = ClickSounds{};
  o.click.sound = std::max(0, int(sounds.indexOf(m_ClickSound)));
  o.click.volume = float(std::clamp(m_ClickVolume, 0, 100)) / 100.f;
  o.click.accent = m_ClickAccent;
  o.click.bars = std::clamp(m_CountInBars, 1, 16);
  return o;
}

SCORE_SETTINGS_PARAMETER_CPP(QString, Model, AudioFormat)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, Start)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, StartOn)
SCORE_SETTINGS_PARAMETER_CPP(double, Model, LengthBars)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, Target)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, MuteSource)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, LoopFixedLength)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, MidiQuantize)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, VideoEncoder)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, TouchRelease)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, CaptureBars)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, InputLatency)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, EngineLatency)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, ClickSound)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, ClickVolume)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, ClickAccent)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, CountInBars)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, Metronome)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, CountInOnPlay)

View::View()
{
  m_tabs = new QTabWidget;
  QFormLayout* lay{};
  const auto tab = [&](const QString& name) {
    auto f = new score::FormWidget{name};
    m_tabs->addTab(f, name);
    m_widg = f;
    lay = f->layout();
  };

  tab(tr("General"));
  SETTINGS_UI_COMBOBOX_SETUP("Start", Start, Starts{});
  score::setHelp(
      m_Start, tr("When \"Record now\" starts recording. Audio starts at once when told to "
                  "wait for the first note or value."));
  SETTINGS_UI_COMBOBOX_SETUP("Start on", StartOn, Grids{});
  score::setHelp(
      m_StartOn, tr("Where in the bar a recording starts, counted in the box the "
                    "recorded port plays in."));
  SETTINGS_UI_DOUBLE_SPINBOX_SETUP("Length in bars", LengthBars);
  m_LengthBars->setRange(0., 10000.);
  m_LengthBars->setDecimals(2);
  m_LengthBars->setSpecialValueText(tr("Until stopped"));
  score::setHelp(m_LengthBars, tr("The recording stops by itself after this many bars."));
  SETTINGS_UI_SPINBOX_SETUP("Capture length in bars", CaptureBars);
  m_CaptureBars->setRange(1, 256);
  score::setHelp(
      m_CaptureBars,
      tr("Capture places the last whole bars the armed processes played, up to this "
         "many, as loops."));

  SETTINGS_UI_TOGGLE_SETUP("Loop takes of a number of bars", LoopFixedLength);
  score::setHelp(
      m_LoopFixedLength,
      tr("A take of a number of bars is placed as a loop that long from the start of "
         "its box, turned so that each moment of the box plays what was played there."));
  SETTINGS_UI_COMBOBOX_SETUP("Takes go to", Target, Targets{});
  score::setHelp(
      m_Target, tr("Where a take is placed when the recording ends; it is always "
                   "written to the Takes folder of the document."));

  SETTINGS_UI_TOGGLE_SETUP("Mute what was recorded", MuteSource);
  score::setHelp(
      m_MuteSource, tr("When a take is placed, the process it was recorded from is muted "
                       "in the same step, so that it is not heard twice."));

  tab(tr("Metronome"));
  SETTINGS_UI_TOGGLE_SETUP("Click while playing", Metronome);
  score::setHelp(
      m_Metronome, tr("The beats of the score are clicked on the cue output (audio:/out/cue) "
                      "while it plays."));
  SETTINGS_UI_TOGGLE_SETUP("Count in before playback", CountInOnPlay);
  score::setHelp(
      m_CountInOnPlay, tr("Play waits for the count-in's bars of clicks, at the tempo and "
                          "time signature the score starts with."));
  SETTINGS_UI_SPINBOX_SETUP("Count-in bars", CountInBars);
  m_CountInBars->setRange(1, 16);
  score::setHelp(
      m_CountInBars, tr("The bars clicked before playback, and at least before a recording "
                        "counted in starts on a bar line."));
  SETTINGS_UI_COMBOBOX_SETUP("Sound", ClickSound, ClickSounds{});
  SETTINGS_UI_SPINBOX_SETUP("Volume", ClickVolume);
  m_ClickVolume->setRange(0, 100);
  m_ClickVolume->setSuffix(tr(" %"));
  SETTINGS_UI_TOGGLE_SETUP("Accent the first beat", ClickAccent);

  tab(tr("Audio"));
  SETTINGS_UI_COMBOBOX_SETUP("Sample format", AudioFormat, AudioFormats{});
  score::setHelp(
      m_AudioFormat, tr("The sample format of the WAV files audio takes are written to."));

  SETTINGS_UI_SPINBOX_SETUP("Input latency", InputLatency);
  m_InputLatency->setRange(0, 500);
  m_InputLatency->setSuffix(tr(" ms"));
  score::setHelp(
      m_InputLatency,
      tr("How late the sound card's inputs reach score: what is recorded from them is "
         "moved this much earlier, as the loopers do."));
  SETTINGS_UI_TOGGLE_SETUP("Add the driver's latency", EngineLatency);
  score::setHelp(
      m_EngineLatency,
      tr("Also moves what is recorded from the sound card earlier by the input and "
         "output latency its driver reports, for playing along with what is heard."));

  tab(tr("MIDI"));
  SETTINGS_UI_COMBOBOX_SETUP("Quantize notes to", MidiQuantize, Quantizes{});
  score::setHelp(
      m_MidiQuantize, tr("The start of each recorded note is moved to the nearest of these, "
                         "in the box's bars."));

#if SCORE_RECORDING_TEXTURES
  tab(tr("Video"));
  setupVideo(*lay);
#endif

  tab(tr("Automations"));
  SETTINGS_UI_SPINBOX_SETUP("Simplification ratio", SimplificationRatio);
  m_SimplificationRatio->setRange(1, 10000);
  score::setHelp(
      m_SimplificationRatio,
      tr("How close simplified curves stay to what was recorded or drawn: higher is "
         "closer. Also used by the Pen tool of curves."));
  SETTINGS_UI_TOGGLE_SETUP("Simplify recorded curves", Simplify);
  score::setHelp(m_Simplify, tr("Keep fewer points of the recorded automations."));
  SETTINGS_UI_TOGGLE_SETUP("Ramp to new value", Ramp);
  score::setHelp(
      m_Ramp, tr("A recorded automation goes from one value to the next, instead of "
                 "holding each value until the next one comes."));
  SETTINGS_UI_SPINBOX_SETUP("Touch release", TouchRelease);
  m_TouchRelease->setRange(0, 10000);
  m_TouchRelease->setSuffix(tr(" ms"));
  score::setHelp(
      m_TouchRelease,
      tr("Recording touching an automation: how long after the last value received the "
         "automation's own curve comes back."));
  SETTINGS_UI_TOGGLE_SETUP("Play while recording", PlayWhileRecording);
  score::setHelp(
      m_PlayWhileRecording,
      tr("Recording automations or messages from a point starts playback from there with "
         "the first value; else it records once playback is started."));
}

#if SCORE_RECORDING_TEXTURES
namespace
{
constexpr auto videoPresetsKey = "score_plugin_recording/VideoPresets";

QVariantMap userVideoPresets()
{
  return QSettings{}.value(videoPresetsKey).toMap();
}

//! Two encodings that write the same file
bool sameEncoding(const Gfx::LibavSettings& a, const Gfx::LibavSettings& b)
{
  return a.width == b.width && a.height == b.height && a.rate == b.rate
         && a.muxer == b.muxer && a.video_encoder_short == b.video_encoder_short
         && a.video_converted_pixfmt == b.video_converted_pixfmt && a.options == b.options;
}
}

void View::setupVideo(QFormLayout& lay)
{
  auto row = new QWidget;
  auto h = new QHBoxLayout{row};
  h->setContentsMargins(0, 0, 0, 0);
  m_videoPreset = new QComboBox;
  auto save = new QPushButton{tr("Save as preset")};
  auto del = new QPushButton{tr("Delete preset")};
  h->addWidget(m_videoPreset, 1);
  h->addWidget(save);
  h->addWidget(del);
  lay.addRow(tr("Preset"), row);
  score::setHelp(
      m_videoPreset,
      tr("H.264 and H.265 make small files; HAP plays back with little CPU, as score "
         "decodes it on the GPU; ProRes is for editing; FFV1 keeps every pixel. Your own "
         "presets follow. An encoder this FFmpeg cannot use falls back to FFV1."));

  auto* w = new Gfx::LibavSettingsWidget;
  w->setEncodingOnly();
  m_videoEncoding = w;
  lay.addRow(w);
  refreshVideoPresets();

  connect(m_videoPreset, &QComboBox::activated, this, [this](int i) {
    const auto json = m_videoPreset->itemData(i).toString();
    if(json.isEmpty())
      return;
    setVideoEncoder(json);
    VideoEncoderChanged(json);
  });

  const auto edited = [this, w] {
    if(m_videoUpdating)
      return;
    const auto ls = w->getSettings().deviceSpecificSettings.value<Gfx::LibavSettings>();
    const auto json = videoEncodingToJson(ls);
    if(json == m_videoJson)
      return;
    m_videoJson = json;
    refreshVideoPresets();
    VideoEncoderChanged(json);
  };
  for(auto* c : w->findChildren<QComboBox*>())
    connect(c, &QComboBox::currentIndexChanged, this, edited);
  for(auto* c : w->findChildren<QSpinBox*>())
    connect(c, &QSpinBox::valueChanged, this, edited);
  for(auto* c : w->findChildren<QDoubleSpinBox*>())
    connect(c, &QDoubleSpinBox::valueChanged, this, edited);
  for(auto* c : w->findChildren<QPlainTextEdit*>())
    connect(c, &QPlainTextEdit::textChanged, this, edited);

  connect(save, &QPushButton::clicked, this, [this] {
    bool ok{};
    const auto name = QInputDialog::getText(
        m_videoPreset, tr("Save as preset"), tr("Name"), QLineEdit::Normal, {}, &ok);
    if(!ok || name.trimmed().isEmpty())
      return;
    auto presets = userVideoPresets();
    presets[name.trimmed()] = m_videoJson;
    QSettings{}.setValue(videoPresetsKey, presets);
    refreshVideoPresets();
  });
  connect(del, &QPushButton::clicked, this, [this] {
    const auto name = m_videoPreset->currentText();
    auto presets = userVideoPresets();
    if(!presets.contains(name))
      return;
    presets.remove(name);
    QSettings{}.setValue(videoPresetsKey, presets);
    refreshVideoPresets();
  });
}

void View::refreshVideoPresets()
{
  if(!m_videoPreset)
    return;
  QSignalBlocker _{m_videoPreset};
  m_videoPreset->clear();
  const auto current = videoEncodingFromJson(m_videoJson);
  int selected = -1;
  const auto add = [&](const QString& name, const QString& json) {
    m_videoPreset->addItem(name, json);
    if(auto e = videoEncodingFromJson(json); selected < 0 && e && current
                                            && sameEncoding(*e, *current))
      selected = m_videoPreset->count() - 1;
  };
  for(const auto& p : builtinVideoPresets())
    add(p.name, videoEncodingToJson(p.settings));
  const auto user = userVideoPresets();
  if(!user.isEmpty())
    m_videoPreset->insertSeparator(m_videoPreset->count());
  for(auto it = user.begin(); it != user.end(); ++it)
    add(it.key(), it.value().toString());
  if(selected < 0)
  {
    m_videoPreset->addItem(tr("Custom"), QString{});
    selected = m_videoPreset->count() - 1;
  }
  m_videoPreset->setCurrentIndex(selected);
}

void View::setVideoEncoder(QString json)
{
  if(!m_videoEncoding)
    return;
  const auto ls = videoEncodingFromJson(json).value_or(defaultVideoEncoding());
  m_videoJson = videoEncodingToJson(ls);
  Device::DeviceSettings ds;
  ds.name = QStringLiteral("take");
  ds.protocol = Gfx::LibavProtocolFactory::static_concreteKey();
  ds.deviceSpecificSettings = QVariant::fromValue(ls);
  m_videoUpdating = true;
  auto* w = static_cast<Gfx::LibavSettingsWidget*>(m_videoEncoding);
  w->setSettings(ds);
  w->setEncodingOnly();
  m_videoUpdating = false;
  refreshVideoPresets();
}
#endif

QWidget* View::getWidget()
{
  return m_tabs;
}

SETTINGS_UI_COMBOBOX_IMPL(AudioFormat)
SETTINGS_UI_COMBOBOX_IMPL(Start)
SETTINGS_UI_COMBOBOX_IMPL(StartOn)
SETTINGS_UI_DOUBLE_SPINBOX_IMPL(LengthBars)
SETTINGS_UI_COMBOBOX_IMPL(Target)
SETTINGS_UI_TOGGLE_IMPL(MuteSource)
SETTINGS_UI_TOGGLE_IMPL(LoopFixedLength)
SETTINGS_UI_COMBOBOX_IMPL(MidiQuantize)
SETTINGS_UI_SPINBOX_IMPL(TouchRelease)
SETTINGS_UI_SPINBOX_IMPL(CaptureBars)
SETTINGS_UI_SPINBOX_IMPL(InputLatency)
SETTINGS_UI_TOGGLE_IMPL(EngineLatency)
SETTINGS_UI_COMBOBOX_IMPL(ClickSound)
SETTINGS_UI_SPINBOX_IMPL(ClickVolume)
SETTINGS_UI_TOGGLE_IMPL(ClickAccent)
SETTINGS_UI_SPINBOX_IMPL(CountInBars)
SETTINGS_UI_TOGGLE_IMPL(Metronome)
SETTINGS_UI_TOGGLE_IMPL(CountInOnPlay)
SETTINGS_UI_SPINBOX_IMPL(SimplificationRatio)
SETTINGS_UI_TOGGLE_IMPL(Simplify)
SETTINGS_UI_TOGGLE_IMPL(Ramp)
SETTINGS_UI_TOGGLE_IMPL(PlayWhileRecording)

Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::GlobalSettingsPresenter{m, v, parent}
{
  SETTINGS_PRESENTER(AudioFormat);
  SETTINGS_PRESENTER(Start);
  SETTINGS_PRESENTER(StartOn);
  SETTINGS_PRESENTER(LengthBars);
  SETTINGS_PRESENTER(Target);
  SETTINGS_PRESENTER(MuteSource);
  SETTINGS_PRESENTER(LoopFixedLength);
  SETTINGS_PRESENTER(MidiQuantize);
  SETTINGS_PRESENTER(VideoEncoder);
  SETTINGS_PRESENTER(TouchRelease);
  SETTINGS_PRESENTER(CaptureBars);
  SETTINGS_PRESENTER(InputLatency);
  SETTINGS_PRESENTER(EngineLatency);
  SETTINGS_PRESENTER(ClickSound);
  SETTINGS_PRESENTER(ClickVolume);
  SETTINGS_PRESENTER(ClickAccent);
  SETTINGS_PRESENTER(CountInBars);
  SETTINGS_PRESENTER(Metronome);
  SETTINGS_PRESENTER(CountInOnPlay);

  // The curve plug-in's model: the same commands as its own page had
  auto& c = score::AppContext().settings<Curve::Settings::Model>();
  using namespace Curve::Settings;
  con(v, &View::SimplificationRatioChanged, this, [this, &c](int val) {
    if(val != c.getSimplificationRatio())
      m_disp.submit<SetModelSimplificationRatio>(c, val);
  });
  con(c, &Curve::Settings::Model::SimplificationRatioChanged, &v,
      &View::setSimplificationRatio);
  v.setSimplificationRatio(c.getSimplificationRatio());

  con(v, &View::SimplifyChanged, this, [this, &c](bool val) {
    if(val != c.getSimplify())
      m_disp.submit<SetModelSimplify>(c, val);
  });
  con(c, &Curve::Settings::Model::SimplifyChanged, &v, &View::setSimplify);
  v.setSimplify(c.getSimplify());

  con(v, &View::RampChanged, this, [this, &c](bool val) {
    const auto mode = val ? Mode::Message : Mode::Parameter;
    if(mode != c.getCurveMode())
      m_disp.submit<SetModelCurveMode>(c, mode);
  });
  con(c, &Curve::Settings::Model::CurveModeChanged, &v,
      [&v](Mode mode) { v.setRamp(mode == Mode::Message); });
  v.setRamp(c.getCurveMode() == Mode::Message);

  con(v, &View::PlayWhileRecordingChanged, this, [this, &c](bool val) {
    if(val != c.getPlayWhileRecording())
      m_disp.submit<SetModelPlayWhileRecording>(c, val);
  });
  con(c, &Curve::Settings::Model::PlayWhileRecordingChanged, &v,
      &View::setPlayWhileRecording);
  v.setPlayWhileRecording(c.getPlayWhileRecording());
}

QString Presenter::settingsName()
{
  return tr("Recording");
}

QIcon Presenter::settingsIcon()
{
  return makeIcons(
      QStringLiteral(":/icons/settings_recording_on.png"),
      QStringLiteral(":/icons/settings_recording_off.png"),
      QStringLiteral(":/icons/settings_recording_off.png"));
}
}
