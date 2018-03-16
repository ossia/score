// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Media/Effect/Settings/Presenter.hpp>
#include <Media/Effect/Settings/Model.hpp>
#include <Media/Effect/Settings/View.hpp>
#include <QApplication>
#include <QStyle>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/Command.hpp>
#include <score/command/SettingsCommand.hpp>

namespace Media::Settings
{
Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::GlobalSettingsPresenter{m, v, parent}
{
  SETTINGS_PRESENTER(Card);
  SETTINGS_PRESENTER(BufferSize);
  SETTINGS_PRESENTER(Rate);
  SETTINGS_PRESENTER(VstPaths);
}

QString Presenter::settingsName()
{
  return tr("Effects");
}

QIcon Presenter::settingsIcon()
{
  return QApplication::style()->standardIcon(QStyle::SP_MediaPlay);
}
}
