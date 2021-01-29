// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Media/Effect/Settings/Model.hpp>
#include <Media/Effect/Settings/Presenter.hpp>
#include <Media/Effect/Settings/View.hpp>

#include <score/command/Command.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>
#include <score/widgets/SetIcons.hpp>

#include <QApplication>
#include <QStyle>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::Settings::View)
W_OBJECT_IMPL(Media::Settings::Model)
namespace Media::Settings
{
Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::GlobalSettingsPresenter{m, v, parent}
{
}

QString Presenter::settingsName()
{
  return tr("Effects");
}

QIcon Presenter::settingsIcon()
{
  return makeIcons(
      QStringLiteral(":/icons/settings_effect_on.png"),
      QStringLiteral(":/icons/settings_effect_off.png"),
      QStringLiteral(":/icons/settings_effect_off.png"));
}
}
