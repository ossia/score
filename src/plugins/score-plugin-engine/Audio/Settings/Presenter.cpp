// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score/command/Command.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>

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
  auto& ctx = score::GUIAppContext().interfaces<AudioFactoryList>();
  for (auto& drv : ctx)
  {
    v.addDriver(
        drv.prettyName(),
        QVariant::fromValue(drv.concreteKey()),
        drv.make_settings(m, v, m_disp, nullptr));
  }

  v.setDriver(m.getDriver());
  v.setRate(m.getRate());
  v.setBufferSize(m.getBufferSize());
  v.setAutoStereo(m.getAutoStereo());

  con(v, &View::DriverChanged, this, [&](auto val) {
    if (val != m.getDriver())
    {
      m_disp.submit<SetModelDriver>(this->model(this), val);
    }
  });
  con(v, &View::AutoStereoChanged, this, [&](auto val) {
    if (val != m.getAutoStereo())
    {
      m_disp.submitDeferredCommand<SetModelAutoStereo>(this->model(this), val);
    }
  });

  con(v, &View::BufferSizeChanged, this, [&](auto val) {
    if (val != m.getBufferSize())
    {
      m_disp.submitDeferredCommand<SetModelBufferSize>(this->model(this), val);
    }
  });
  con(v, &View::RateChanged, this, [&](auto val) {
    if (val != m.getRate())
    {
      m_disp.submitDeferredCommand<SetModelRate>(this->model(this), val);
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
  return QApplication::style()->standardIcon(QStyle::SP_MediaVolume);
}
}
