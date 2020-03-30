// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score/command/Command.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>
#include <score/widgets/SetIcons.hpp>

#include <QApplication>
#include <QStyle>

#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/Presenter.hpp>
#include <Audio/Settings/View.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Audio::Settings::Model)
W_OBJECT_IMPL(Audio::Settings::View)
namespace Audio::Settings
{
Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::GlobalSettingsPresenter{m, v, parent}
{

  v.setDriver(m.getDriver());
  loadDriver(m.getDriver());
  v.setRate(m.getRate());
  v.setBufferSize(m.getBufferSize());
  v.setAutoStereo(m.getAutoStereo());

  con(v, &View::DriverChanged, this, [this, &m] (auto val) {
    if (val != m.getDriver())
    {
      m_disp.submit<SetModelDriver>(m, val);
      loadDriver(val);
    }
  });
  con(v, &View::AutoStereoChanged, this, [this, &m](auto val) {
    if (val != m.getAutoStereo())
    {
      m_disp.submitDeferredCommand<SetModelAutoStereo>(m, val);
    }
  });

  con(v, &View::BufferSizeChanged, this, [this, &m](auto val) {
    if (val != m.getBufferSize())
    {
      m_disp.submitDeferredCommand<SetModelBufferSize>(m, val);
    }
  });
  con(v, &View::RateChanged, this, [this, &m](auto val) {
    if (val != m.getRate())
    {
      m_disp.submitDeferredCommand<SetModelRate>(m, val);
    }
  });

  con(m, &Model::changed, this, [&] {
    v.setDriver(m.getDriver());
    v.setRate(m.getRate());
    v.setBufferSize(m.getBufferSize());
  });
}

void Presenter::on_accept()
{
  m_disp.commit();
  model(this).changed();
}
QString Presenter::settingsName()
{
  return tr("Audio");
}

QIcon Presenter::settingsIcon()
{
  return makeIcons(QStringLiteral(":/icons/settings_audio_on.png")
                   , QStringLiteral(":/icons/settings_audio_off.png")
                   , QStringLiteral(":/icons/settings_audio_off.png"));
}

void Presenter::loadDriver(const UuidKey<AudioFactory>& val)
{
  auto& list = score::GUIAppContext().interfaces<AudioFactoryList>();
  auto factory = list.get(val);
  auto& m = this->model(this);
  auto& v = this->view(this);

  if(factory)
  {
    auto widg = factory->make_settings(m, v, m_disp, nullptr);
    v.setDriverWidget(widg);
  }
  else {
    v.setDriverWidget(nullptr);
  }
}
}
