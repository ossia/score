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
      if (val != m.getEnabled())
      {
        m_disp.submit<SetModelEnabled>(this->model(this), val);
      }
    });

    // model -> view
    con(m, &Model::EnabledChanged, &v, &View::setEnabled);

    // initial value
    v.setEnabled(m.getEnabled());
  }
}

QString Presenter::settingsName()
{
  return tr("Remote control");
}

QIcon Presenter::settingsIcon()
{
  return makeIcons(QStringLiteral(":/icons/settings_remote_control_on.png")
                   , QStringLiteral(":/icons/settings_remote_control_off.png")
                   , QStringLiteral(":/icons/settings_remote_control_off.png"));

}

}
}
