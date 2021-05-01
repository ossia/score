// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Protocols/Settings/Model.hpp>
#include <Protocols/Settings/Presenter.hpp>
#include <Protocols/Settings/View.hpp>

#include <score/command/Command.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>
#include <score/widgets/SetIcons.hpp>

#include <QApplication>
#include <QStyle>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::Settings::Model)
W_OBJECT_IMPL(Protocols::Settings::View)
namespace Protocols::Settings
{
Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::GlobalSettingsPresenter{m, v, parent}
{
  SETTINGS_PRESENTER(MidiAPI);
}

QString Presenter::settingsName()
{
  return tr("Protocols");
}

QIcon Presenter::settingsIcon()
{
  return makeIcons(
      QStringLiteral(":/icons/settings_audio_on.png"),
      QStringLiteral(":/icons/settings_audio_off.png"),
      QStringLiteral(":/icons/settings_audio_off.png"));
}

}
