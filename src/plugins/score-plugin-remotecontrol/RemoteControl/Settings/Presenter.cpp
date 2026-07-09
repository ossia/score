#include "Presenter.hpp"

#include "Model.hpp"
#include "View.hpp"

#include <score/command/Command.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>
#include <score/widgets/SetIcons.hpp>

#include <QApplication>
#include <QStyle>

namespace RemoteControl
{
namespace Settings
{
Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::GlobalSettingsPresenter{m, v, parent}
{
  {
    // view -> model
    con(v, &View::enabledChanged, this, [&](auto val) {
      if(val != m.getEnabled())
      {
        m_disp.submit<SetModelEnabled>(this->model(this), val);
      }
    });

    con(v, &View::webUiPathChanged, this, [&](auto val) {
      if(val != m.getWebUiPath())
      {
        m_disp.submit<SetModelWebUiPath>(this->model(this), val);
      }
    });

    con(v, &View::serverAddressChanged, this, [&](auto val) {
      if(val != m.getServerAddress())
      {
        m_disp.submit<SetModelServerAddress>(this->model(this), val);
      }
    });

    con(v, &View::serverPortChanged, this, [&](auto val) {
      if(val != m.getServerPort())
      {
        m_disp.submit<SetModelServerPort>(this->model(this), val);
      }
    });

    con(v, &View::serverEnabledChanged, this, [&](auto val) {
      if(val != m.getServerEnabled())
      {
        m_disp.submit<SetModelServerEnabled>(this->model(this), val);
      }
    });

    // model -> view
    con(m, &Model::EnabledChanged, &v, &View::setEnabled);
    con(m, &Model::WebUiPathChanged, &v, &View::setWebUiPath);
    con(m, &Model::ServerAddressChanged, &v, &View::setServerAddress);
    con(m, &Model::ServerPortChanged, &v, &View::setServerPort);
    con(m, &Model::ServerEnabledChanged, &v, &View::setServerEnabled);

    // initial value
    v.setEnabled(m.getEnabled());
    v.setWebUiPath(m.getWebUiPath());
    v.setServerAddress(m.getServerAddress());
    v.setServerPort(m.getServerPort());
    v.setServerEnabled(m.getServerEnabled());
  }
}

QString Presenter::settingsName()
{
  return tr("Remote control");
}

QIcon Presenter::settingsIcon()
{
  return makeIcons(
      QStringLiteral(":/icons/settings_remote_control_on.png"),
      QStringLiteral(":/icons/settings_remote_control_off.png"),
      QStringLiteral(":/icons/settings_remote_control_off.png"));
}

}
}
