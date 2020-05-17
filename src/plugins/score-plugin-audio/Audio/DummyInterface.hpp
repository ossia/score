#pragma once
#include <ossia/audio/dummy_protocol.hpp>

#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
namespace Audio
{

class DummyFactory final : public AudioFactory
{
  SCORE_CONCRETE("13dabcc3-9cda-422f-a8c7-5fef5c220677")
public:
  ~DummyFactory() override { }

  QString prettyName() const override { return QObject::tr("Dummy (No audio)"); };
  std::unique_ptr<ossia::audio_engine>
  make_engine(const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override
  {
    return std::make_unique<ossia::dummy_engine>(set.getRate(), set.getBufferSize());
  }

  QWidget* make_settings(
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent) override
  {
    return nullptr;
  }
};
}
