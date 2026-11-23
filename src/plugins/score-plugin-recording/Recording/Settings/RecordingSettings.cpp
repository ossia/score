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

static auto list()
{
  return std::tie(AudioFormat, Start, StartOn, LengthBars, Target, MuteSource);
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
  return o;
}

SCORE_SETTINGS_PARAMETER_CPP(QString, Model, AudioFormat)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, Start)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, StartOn)
SCORE_SETTINGS_PARAMETER_CPP(double, Model, LengthBars)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, Target)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, MuteSource)

View::View()
{
  m_widg = new score::FormWidget{tr("Recording")};
  auto lay = m_widg->layout();

  SETTINGS_UI_COMBOBOX_SETUP("Audio takes", AudioFormat, AudioFormats{});
  score::setHelp(m_AudioFormat, tr("The sample format of the WAV files audio takes are written to."));
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
  SETTINGS_UI_COMBOBOX_SETUP("Takes go to", Target, Targets{});
  score::setHelp(
      m_Target, tr("Where a take is placed when the recording ends; it is always "
                   "written to the Takes folder of the document."));

  SETTINGS_UI_TOGGLE_SETUP("Mute what was recorded", MuteSource);
  score::setHelp(
      m_MuteSource, tr("When a take is placed, the process it was recorded from is muted "
                       "in the same step, so that it is not heard twice."));

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
  SETTINGS_UI_TOGGLE_SETUP("Play while recording", PlayWhileRecording);
  score::setHelp(
      m_PlayWhileRecording,
      tr("Recording automations or messages from a point starts playback from there with "
         "the first value; else it records once playback is started."));
}

QWidget* View::getWidget()
{
  return m_widg;
}

SETTINGS_UI_COMBOBOX_IMPL(AudioFormat)
SETTINGS_UI_COMBOBOX_IMPL(Start)
SETTINGS_UI_COMBOBOX_IMPL(StartOn)
SETTINGS_UI_DOUBLE_SPINBOX_IMPL(LengthBars)
SETTINGS_UI_COMBOBOX_IMPL(Target)
SETTINGS_UI_TOGGLE_IMPL(MuteSource)
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
