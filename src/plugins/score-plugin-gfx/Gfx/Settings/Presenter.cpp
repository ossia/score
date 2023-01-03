// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Gfx/Settings/Model.hpp>
#include <Gfx/Settings/Presenter.hpp>
#include <Gfx/Settings/View.hpp>

#include <score/command/Command.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>
#include <score/widgets/SetIcons.hpp>

#include <QApplication>
#include <QStyle>

#include <wobjectimpl.h>
namespace Gfx::Settings
{
Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::GlobalSettingsPresenter{m, v, parent}
{
  SETTINGS_PRESENTER(GraphicsApi);
  SETTINGS_PRESENTER(HardwareDecode);
  SETTINGS_PRESENTER(Samples);
  SETTINGS_PRESENTER(DecodingThreads);
  SETTINGS_PRESENTER(Rate);
  SETTINGS_PRESENTER(VSync);
}

QString Presenter::settingsName()
{
  return tr("Graphics");
}

QIcon Presenter::settingsIcon()
{
  return makeIcons(
      QStringLiteral(":/icons/settings_graphics_on.png"),
      QStringLiteral(":/icons/settings_graphics_off.png"),
      QStringLiteral(":/icons/settings_graphics_off.png"));
}
}
