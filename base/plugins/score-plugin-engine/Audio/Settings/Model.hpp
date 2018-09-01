#pragma once
#include <Audio/AudioInterface.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <wobjectdefs.h>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score_plugin_engine_export.h>

#define AUDIO_PARAMETER_HPP(Export, Type, Name)                                          \
public:                                                                                  \
  Type get##Name() const;                                                                \
  void set##Name(Type);                                                                  \
  W_PROPERTY(Type, Name READ get##Name WRITE set##Name)                                  \
  struct p_ ## Name {                                                                    \
    using param_type = Type;                                                             \
    using model_type = W_ThisType;                                                       \
    static constexpr auto name = #Name;                                                  \
    static constexpr auto get() { constexpr auto x = &model_type::get##Name; return x; } \
    static constexpr auto set() { constexpr auto x = &model_type::set##Name; return x; } \
  };                                                                                     \
private:

namespace Audio::Settings
{

class SCORE_PLUGIN_ENGINE_EXPORT Model : public score::SettingsDelegateModel
{
  W_OBJECT(Model)

  Audio::AudioFactory::ConcreteKey m_Driver{};
  QString m_CardIn{};
  QString m_CardOut{};
  int m_BufferSize{};
  int m_Rate{};
  int m_DefaultIn{};
  int m_DefaultOut{};

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  void changed() E_SIGNAL(SCORE_PLUGIN_ENGINE_EXPORT, changed);
  AUDIO_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, Audio::AudioFactory::ConcreteKey, Driver)
  AUDIO_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, QString, CardIn)
  AUDIO_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, QString, CardOut)
  AUDIO_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, int, BufferSize)
  AUDIO_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, int, Rate)
  AUDIO_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, int, DefaultIn)
  AUDIO_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, int, DefaultOut)
};

SCORE_SETTINGS_PARAMETER(Model, Driver)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, CardIn)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, CardOut)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, BufferSize)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, Rate)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, DefaultIn)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, DefaultOut)
}

#undef AUDIO_PARAMETER_HPP
