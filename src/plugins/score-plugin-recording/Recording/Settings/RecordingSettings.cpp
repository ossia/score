#include "RecordingSettings.hpp"

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
SETTINGS_PARAMETER_IMPL(VideoFps){QStringLiteral("score_plugin_recording/VideoFps"), 30};
SETTINGS_PARAMETER_IMPL(VideoCodec){
    QStringLiteral("score_plugin_recording/VideoCodec"), VideoCodecs{}.h264};
SETTINGS_PARAMETER_IMPL(VideoWidth){QStringLiteral("score_plugin_recording/VideoWidth"), 1920};
SETTINGS_PARAMETER_IMPL(VideoHeight){
    QStringLiteral("score_plugin_recording/VideoHeight"), 1080};
SETTINGS_PARAMETER_IMPL(CaptureBars){
    QStringLiteral("score_plugin_recording/CaptureBars"), 4};
SETTINGS_PARAMETER_IMPL(TouchRelease){
    QStringLiteral("score_plugin_recording/TouchRelease"), 500};

static auto list()
{
  return std::tie(
      AudioFormat, Start, StartOn, LengthBars, Target, MuteSource, LoopFixedLength,
      MidiQuantize, VideoFps, VideoCodec, VideoWidth, VideoHeight, TouchRelease, CaptureBars);
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
  o.video.fps = std::clamp(m_VideoFps, 1, 240);
  o.touchRelease = int64_t(std::max(0, m_TouchRelease)) * 705600;
  o.video.size = QSize{std::clamp(m_VideoWidth, 16, 8192), std::clamp(m_VideoHeight, 16, 8192)};
  const VideoCodecs c;
  o.video.codec = m_VideoCodec == c.h265     ? VideoSettings::H265
                  : m_VideoCodec == c.hap    ? VideoSettings::Hap
                  : m_VideoCodec == c.prores ? VideoSettings::ProRes
                  : m_VideoCodec == c.ffv1   ? VideoSettings::Ffv1
                                             : VideoSettings::H264;
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
SCORE_SETTINGS_PARAMETER_CPP(int, Model, VideoFps)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, VideoCodec)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, VideoWidth)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, VideoHeight)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, TouchRelease)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, CaptureBars)

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

  tab(tr("Audio"));
  SETTINGS_UI_COMBOBOX_SETUP("Sample format", AudioFormat, AudioFormats{});
  score::setHelp(
      m_AudioFormat, tr("The sample format of the WAV files audio takes are written to."));

  tab(tr("MIDI"));
  SETTINGS_UI_COMBOBOX_SETUP("Quantize notes to", MidiQuantize, Quantizes{});
  score::setHelp(
      m_MidiQuantize, tr("The start of each recorded note is moved to the nearest of these, "
                         "in the box's bars."));

  tab(tr("Video"));
  SETTINGS_UI_SPINBOX_SETUP("Frames per second", VideoFps);
  m_VideoFps->setRange(1, 240);
  SETTINGS_UI_SPINBOX_SETUP("Width", VideoWidth);
  m_VideoWidth->setRange(16, 8192);
  m_VideoWidth->setSuffix(tr(" px"));
  SETTINGS_UI_SPINBOX_SETUP("Height", VideoHeight);
  m_VideoHeight->setRange(16, 8192);
  m_VideoHeight->setSuffix(tr(" px"));
  score::setHelp(
      m_VideoWidth, tr("The size video takes are rendered and encoded at, whatever the "
                       "size of what is recorded."));
  SETTINGS_UI_COMBOBOX_SETUP("Codec", VideoCodec, VideoCodecs{});
  score::setHelp(
      m_VideoCodec,
      tr("H.264 and H.265 make small files; HAP plays back with little CPU, as score "
         "decodes it on the GPU; ProRes is for editing; FFV1 keeps every pixel. A codec "
         "this FFmpeg cannot encode falls back to FFV1."));

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
SETTINGS_UI_SPINBOX_IMPL(VideoFps)
SETTINGS_UI_COMBOBOX_IMPL(VideoCodec)
SETTINGS_UI_SPINBOX_IMPL(VideoWidth)
SETTINGS_UI_SPINBOX_IMPL(VideoHeight)
SETTINGS_UI_SPINBOX_IMPL(TouchRelease)
SETTINGS_UI_SPINBOX_IMPL(CaptureBars)
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
  SETTINGS_PRESENTER(VideoFps);
  SETTINGS_PRESENTER(VideoCodec);
  SETTINGS_PRESENTER(VideoWidth);
  SETTINGS_PRESENTER(VideoHeight);
  SETTINGS_PRESENTER(TouchRelease);
  SETTINGS_PRESENTER(CaptureBars);

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
