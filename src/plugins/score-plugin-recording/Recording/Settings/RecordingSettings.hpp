#pragma once
#include <Recording/Session/RecordOptions.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <QStringList>

#include <score_plugin_recording_export.h>

#include <verdigris>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
namespace score
{
class FormWidget;
}

namespace Recording::Settings
{
//! The choices of the settings, as they are saved
struct AudioFormats
{
  QString float32{"32-bit float"};
  QString pcm24{"24-bit"};
  QString pcm16{"16-bit"};
  operator QStringList() const { return {float32, pcm24, pcm16}; }
};
struct Starts
{
  QString now{"At once"};
  QString onPlay{"When playback starts"};
  QString firstEvent{"On the first note or value"};
  operator QStringList() const { return {now, onPlay, firstEvent}; }
};
struct Grids
{
  QString none{"Where it is"};
  QString bar{"Next bar"};
  QString quarter{"Next quarter note"};
  operator QStringList() const { return {none, bar, quarter}; }
};
struct Targets
{
  QString box{"A new process in the box"};
  QString file{"Only the file"};
  operator QStringList() const { return {box, file}; }
};

/**
 * What a recording does when the menu entry that starts it does not say: the
 * Recording page of the settings. The options of the recording of addresses
 * (simplification, ramps, playing while recording) are the curve plug-in's
 * model (Curve::Settings::Model), shown on the same page.
 */
class SCORE_PLUGIN_RECORDING_EXPORT Model : public score::SettingsDelegateModel
{
  W_OBJECT(Model)

  QString m_AudioFormat;
  QString m_Start;
  QString m_StartOn;
  double m_LengthBars{};
  QString m_Target;

public:
  Model(
      const UuidKey<score::SettingsDelegateFactory>& k, QSettings& set,
      const score::ApplicationContext& ctx);

  //! The options these settings say
  RecordOptions options() const;

  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_RECORDING_EXPORT, QString, AudioFormat)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_RECORDING_EXPORT, QString, Start)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_RECORDING_EXPORT, QString, StartOn)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_RECORDING_EXPORT, double, LengthBars)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_RECORDING_EXPORT, QString, Target)
};

SCORE_SETTINGS_PARAMETER(Model, AudioFormat)
SCORE_SETTINGS_PARAMETER(Model, Start)
SCORE_SETTINGS_PARAMETER(Model, StartOn)
SCORE_SETTINGS_PARAMETER(Model, LengthBars)
SCORE_SETTINGS_PARAMETER(Model, Target)

class View : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();

  SETTINGS_UI_COMBOBOX_HPP(AudioFormat)
  SETTINGS_UI_COMBOBOX_HPP(Start)
  SETTINGS_UI_COMBOBOX_HPP(StartOn)
  SETTINGS_UI_DOUBLE_SPINBOX_HPP(LengthBars)
  SETTINGS_UI_COMBOBOX_HPP(Target)

  // The curve plug-in's
  SETTINGS_UI_SPINBOX_HPP(SimplificationRatio)
  SETTINGS_UI_TOGGLE_HPP(Simplify)
  SETTINGS_UI_TOGGLE_HPP(Ramp)
  SETTINGS_UI_TOGGLE_HPP(PlayWhileRecording)

private:
  QWidget* getWidget() override;
  score::FormWidget* m_widg{};
};

class Presenter : public score::GlobalSettingsPresenter
{
public:
  using model_type = Model;
  using view_type = View;
  Presenter(Model&, View&, QObject* parent);

private:
  QString settingsName() override;
  QIcon settingsIcon() override;
};

SCORE_DECLARE_SETTINGS_FACTORY(
    Factory, Model, Presenter, View, "0e1d5a32-6f8c-4a8b-9a1e-58b6a1d2c7f4")
}
